// Options
BIND_ASSERT( engine->SetEngineProperty( asEP_ALLOW_UNSAFE_REFERENCES, true ) );
BIND_ASSERT( engine->SetEngineProperty( asEP_OPTIMIZE_BYTECODE, true ) );
BIND_ASSERT( engine->SetEngineProperty( asEP_SCRIPT_SCANNER, 1 ) );
BIND_ASSERT( engine->SetEngineProperty( asEP_USE_CHARACTER_LITERALS, true ) );
BIND_ASSERT( engine->SetEngineProperty( asEP_ALWAYS_IMPL_DEFAULT_CONSTRUCT, true ) );

BIND_ASSERT( engine->RegisterTypedef( "hash", "uint" ) );
// Todo: register new type with automating string - number convertation, exclude GetStrHash
// BIND_ASSERT( engine->RegisterObjectType( "hash", 0, asOBJ_VALUE | asOBJ_POD ) );

#if defined ( BIND_CLIENT ) || defined ( BIND_SERVER )
// Reference value
BIND_ASSERT( engine->RegisterObjectType( "DataRef", 0, asOBJ_REF | asOBJ_NOHANDLE ) );
BIND_ASSERT( engine->RegisterObjectMethod( "DataRef", "const int& opIndex(uint) const", asFUNCTION( BIND_CLASS DataRef_Index ), asCALL_CDECL_OBJFIRST ) );
# ifdef BIND_SERVER
BIND_ASSERT( engine->RegisterObjectMethod( "DataRef", "int& opIndex(uint)", asFUNCTION( BIND_CLASS DataRef_Index ), asCALL_CDECL_OBJFIRST ) );
# endif
// Computed value
BIND_ASSERT( engine->RegisterObjectType( "DataVal", 0, asOBJ_REF | asOBJ_NOHANDLE ) );
BIND_ASSERT( engine->RegisterObjectMethod( "DataVal", "const int opIndex(uint) const", asFUNCTION( BIND_CLASS DataVal_Index ), asCALL_CDECL_OBJFIRST ) );
#endif // #if defined(BIND_CLIENT) || defined(BIND_SERVER)

// Item prototype
BIND_ASSERT( engine->RegisterObjectType( "ProtoItem", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "ProtoItem", asBEHAVE_ADDREF, "void f()", asMETHOD( ProtoItem, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "ProtoItem", asBEHAVE_RELEASE, "void f()", asMETHOD( ProtoItem, Release ), asCALL_THISCALL ) );

BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const hash ProtoId", OFFSETOF( ProtoItem, ProtoId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Type", OFFSETOF( ProtoItem, Type ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const hash PicMap", OFFSETOF( ProtoItem, PicMap ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const hash PicInv", OFFSETOF( ProtoItem, PicInv ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Flags", OFFSETOF( ProtoItem, Flags ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool Stackable", OFFSETOF( ProtoItem, Stackable ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool Deteriorable", OFFSETOF( ProtoItem, Deteriorable ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool GroundLevel", OFFSETOF( ProtoItem, GroundLevel ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Corner", OFFSETOF( ProtoItem, Corner ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 Slot", OFFSETOF( ProtoItem, Slot ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weight", OFFSETOF( ProtoItem, Weight ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Volume", OFFSETOF( ProtoItem, Volume ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Cost", OFFSETOF( ProtoItem, Cost ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint StartCount", OFFSETOF( ProtoItem, StartCount ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 SoundId", OFFSETOF( ProtoItem, SoundId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 Material", OFFSETOF( ProtoItem, Material ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 LightFlags", OFFSETOF( ProtoItem, LightFlags ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 LightDistance", OFFSETOF( ProtoItem, LightDistance ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int8 LightIntensity", OFFSETOF( ProtoItem, LightIntensity ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint LightColor", OFFSETOF( ProtoItem, LightColor ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool DisableEgg", OFFSETOF( ProtoItem, DisableEgg ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint16 AnimWaitBase", OFFSETOF( ProtoItem, AnimWaitBase ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint16 AnimWaitRndMin", OFFSETOF( ProtoItem, AnimWaitRndMin ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint16 AnimWaitRndMax", OFFSETOF( ProtoItem, AnimWaitRndMax ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 AnimStay_0", OFFSETOF( ProtoItem, AnimStay[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 AnimStay_1", OFFSETOF( ProtoItem, AnimStay[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 AnimShow_0", OFFSETOF( ProtoItem, AnimShow[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 AnimShow_1", OFFSETOF( ProtoItem, AnimShow[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 AnimHide_0", OFFSETOF( ProtoItem, AnimHide[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 AnimHide_1", OFFSETOF( ProtoItem, AnimHide[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int16 OffsetX", OFFSETOF( ProtoItem, OffsetX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int16 OffsetY", OFFSETOF( ProtoItem, OffsetY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 SpriteCut", OFFSETOF( ProtoItem, SpriteCut ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int8 DrawOrderOffsetHexY", OFFSETOF( ProtoItem, DrawOrderOffsetHexY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint16 RadioChannel", OFFSETOF( ProtoItem, RadioChannel ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint16 RadioFlags", OFFSETOF( ProtoItem, RadioFlags ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 RadioBroadcastSend", OFFSETOF( ProtoItem, RadioBroadcastSend ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 RadioBroadcastRecv", OFFSETOF( ProtoItem, RadioBroadcastRecv ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 IndicatorStart", OFFSETOF( ProtoItem, IndicatorStart ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 IndicatorMax", OFFSETOF( ProtoItem, IndicatorMax ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint HolodiskNum", OFFSETOF( ProtoItem, HolodiskNum ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int StartValue_0", OFFSETOF( ProtoItem, StartValue[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int StartValue_1", OFFSETOF( ProtoItem, StartValue[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int StartValue_2", OFFSETOF( ProtoItem, StartValue[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int StartValue_3", OFFSETOF( ProtoItem, StartValue[ 3 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int StartValue_4", OFFSETOF( ProtoItem, StartValue[ 4 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int StartValue_5", OFFSETOF( ProtoItem, StartValue[ 5 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int StartValue_6", OFFSETOF( ProtoItem, StartValue[ 6 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int StartValue_7", OFFSETOF( ProtoItem, StartValue[ 7 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int StartValue_8", OFFSETOF( ProtoItem, StartValue[ 8 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int StartValue_9", OFFSETOF( ProtoItem, StartValue[ 9 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 BlockLines", OFFSETOF( ProtoItem, BlockLines ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const hash ChildPid_0", OFFSETOF( ProtoItem, ChildPid[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const hash ChildPid_1", OFFSETOF( ProtoItem, ChildPid[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const hash ChildPid_2", OFFSETOF( ProtoItem, ChildPid[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const hash ChildPid_3", OFFSETOF( ProtoItem, ChildPid[ 3 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const hash ChildPid_4", OFFSETOF( ProtoItem, ChildPid[ 4 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 ChildLines_0", OFFSETOF( ProtoItem, ChildLines[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 ChildLines_1", OFFSETOF( ProtoItem, ChildLines[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 ChildLines_2", OFFSETOF( ProtoItem, ChildLines[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 ChildLines_3", OFFSETOF( ProtoItem, ChildLines[ 3 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 ChildLines_4", OFFSETOF( ProtoItem, ChildLines[ 4 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool Weapon_IsUnarmed", OFFSETOF( ProtoItem, Weapon_IsUnarmed ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Weapon_UnarmedTree", OFFSETOF( ProtoItem, Weapon_UnarmedTree ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Weapon_UnarmedPriority", OFFSETOF( ProtoItem, Weapon_UnarmedPriority ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Weapon_UnarmedMinAgility", OFFSETOF( ProtoItem, Weapon_UnarmedMinAgility ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Weapon_UnarmedMinUnarmed", OFFSETOF( ProtoItem, Weapon_UnarmedMinUnarmed ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Weapon_UnarmedMinLevel", OFFSETOF( ProtoItem, Weapon_UnarmedMinLevel ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weapon_Anim1", OFFSETOF( ProtoItem, Weapon_Anim1 ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weapon_MaxAmmoCount", OFFSETOF( ProtoItem, Weapon_MaxAmmoCount ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Weapon_Caliber", OFFSETOF( ProtoItem, Weapon_Caliber ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const hash Weapon_DefaultAmmoPid", OFFSETOF( ProtoItem, Weapon_DefaultAmmoPid ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Weapon_MinStrength", OFFSETOF( ProtoItem, Weapon_MinStrength ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Weapon_Perk", OFFSETOF( ProtoItem, Weapon_Perk ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weapon_ActiveUses", OFFSETOF( ProtoItem, Weapon_ActiveUses ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Weapon_Skill_0", OFFSETOF( ProtoItem, Weapon_Skill[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Weapon_Skill_1", OFFSETOF( ProtoItem, Weapon_Skill[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Weapon_Skill_2", OFFSETOF( ProtoItem, Weapon_Skill[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const hash Weapon_PicUse_0", OFFSETOF( ProtoItem, Weapon_PicUse[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const hash Weapon_PicUse_1", OFFSETOF( ProtoItem, Weapon_PicUse[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const hash Weapon_PicUse_2", OFFSETOF( ProtoItem, Weapon_PicUse[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weapon_MaxDist_0", OFFSETOF( ProtoItem, Weapon_MaxDist[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weapon_MaxDist_1", OFFSETOF( ProtoItem, Weapon_MaxDist[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weapon_MaxDist_2", OFFSETOF( ProtoItem, Weapon_MaxDist[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weapon_Round_0", OFFSETOF( ProtoItem, Weapon_Round[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weapon_Round_1", OFFSETOF( ProtoItem, Weapon_Round[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weapon_Round_2", OFFSETOF( ProtoItem, Weapon_Round[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weapon_ApCost_0", OFFSETOF( ProtoItem, Weapon_ApCost[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weapon_ApCost_1", OFFSETOF( ProtoItem, Weapon_ApCost[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Weapon_ApCost_2", OFFSETOF( ProtoItem, Weapon_ApCost[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool Weapon_Aim_0", OFFSETOF( ProtoItem, Weapon_Aim[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool Weapon_Aim_1", OFFSETOF( ProtoItem, Weapon_Aim[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool Weapon_Aim_2", OFFSETOF( ProtoItem, Weapon_Aim[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 Weapon_SoundId_0", OFFSETOF( ProtoItem, Weapon_SoundId[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 Weapon_SoundId_1", OFFSETOF( ProtoItem, Weapon_SoundId[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint8 Weapon_SoundId_2", OFFSETOF( ProtoItem, Weapon_SoundId[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Ammo_Caliber", OFFSETOF( ProtoItem, Ammo_Caliber ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool Door_NoBlockMove", OFFSETOF( ProtoItem, Door_NoBlockMove ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool Door_NoBlockShoot", OFFSETOF( ProtoItem, Door_NoBlockShoot ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool Door_NoBlockLight", OFFSETOF( ProtoItem, Door_NoBlockLight ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Container_Volume", OFFSETOF( ProtoItem, Container_Volume ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool Container_Changeble", OFFSETOF( ProtoItem, Container_Changeble ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool Container_CannotPickUp", OFFSETOF( ProtoItem, Container_CannotPickUp ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const bool Container_MagicHandsGrnd", OFFSETOF( ProtoItem, Container_MagicHandsGrnd ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint16 Locker_Condition", OFFSETOF( ProtoItem, Locker_Condition ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const int Grid_Type", OFFSETOF( ProtoItem, Grid_Type ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Car_Speed", OFFSETOF( ProtoItem, Car_Speed ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Car_Passability", OFFSETOF( ProtoItem, Car_Passability ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Car_DeteriorationRate", OFFSETOF( ProtoItem, Car_DeteriorationRate ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Car_CrittersCapacity", OFFSETOF( ProtoItem, Car_CrittersCapacity ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Car_TankVolume", OFFSETOF( ProtoItem, Car_TankVolume ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Car_MaxDeterioration", OFFSETOF( ProtoItem, Car_MaxDeterioration ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Car_FuelConsumption", OFFSETOF( ProtoItem, Car_FuelConsumption ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Car_Entrance", OFFSETOF( ProtoItem, Car_Entrance ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ProtoItem", "const uint Car_MovementType", OFFSETOF( ProtoItem, Car_MovementType ) ) );

#ifdef BIND_SERVER
BIND_ASSERT( engine->RegisterObjectMethod( "ProtoItem", "string@ GetScriptName() const", asFUNCTION( BIND_CLASS ProtoItem_GetScriptName ), asCALL_CDECL_OBJFIRST ) );

/************************************************************************/
/* Types                                                                */
/************************************************************************/
BIND_ASSERT( engine->RegisterObjectType( "GameVar", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "GameVar", asBEHAVE_ADDREF, "void f()", asMETHOD( GameVar, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "GameVar", asBEHAVE_RELEASE, "void f()", asMETHOD( GameVar, Release ), asCALL_THISCALL ) );

BIND_ASSERT( engine->RegisterObjectType( "NpcPlane", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "NpcPlane", asBEHAVE_ADDREF, "void f()", asMETHOD( AIDataPlane, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "NpcPlane", asBEHAVE_RELEASE, "void f()", asMETHOD( AIDataPlane, Release ), asCALL_THISCALL ) );

BIND_ASSERT( engine->RegisterObjectType( "Item", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "Item", asBEHAVE_ADDREF, "void f()", asMETHOD( Item, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "Item", asBEHAVE_RELEASE, "void f()", asMETHOD( Item, Release ), asCALL_THISCALL ) );

BIND_ASSERT( engine->RegisterObjectType( "Scenery", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "Scenery", asBEHAVE_ADDREF, "void f()", asMETHOD( MapObject, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "Scenery", asBEHAVE_RELEASE, "void f()", asMETHOD( MapObject, Release ), asCALL_THISCALL ) );

BIND_ASSERT( engine->RegisterObjectType( "Critter", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "Critter", asBEHAVE_ADDREF, "void f()", asMETHOD( Critter, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "Critter", asBEHAVE_RELEASE, "void f()", asMETHOD( Critter, Release ), asCALL_THISCALL ) );

BIND_ASSERT( engine->RegisterObjectType( "Map", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "Map", asBEHAVE_ADDREF, "void f()", asMETHOD( Map, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "Map", asBEHAVE_RELEASE, "void f()", asMETHOD( Map, Release ), asCALL_THISCALL ) );

BIND_ASSERT( engine->RegisterObjectType( "Location", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "Location", asBEHAVE_ADDREF, "void f()", asMETHOD( Location, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "Location", asBEHAVE_RELEASE, "void f()", asMETHOD( Location, Release ), asCALL_THISCALL ) );

/************************************************************************/
/* Synchronizer                                                         */
/************************************************************************/
BIND_ASSERT( engine->RegisterObjectType( "Synchronizer", sizeof( SyncObject ), asOBJ_VALUE ) );

BIND_ASSERT( engine->RegisterObjectBehaviour( "Synchronizer", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION( BIND_CLASS Synchronizer_Constructor ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "Synchronizer", asBEHAVE_DESTRUCT, "void f()", asFUNCTION( BIND_CLASS Synchronizer_Destructor ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Synchronizer", "void Lock()", asMETHOD( SyncObject, Lock ), asCALL_THISCALL ) );

/************************************************************************/
/* GameVar                                                              */
/************************************************************************/
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int GetValue() const", asMETHOD( GameVar, GetValue ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int GetMin() const", asMETHOD( GameVar, GetMin ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int GetMax() const", asMETHOD( GameVar, GetMax ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "bool IsQuest() const", asMETHOD( GameVar, IsQuest ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "uint GetQuestStr() const", asMETHOD( GameVar, GetQuestStr ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "GameVar& opAddAssign(const int)", asMETHODPR( GameVar, operator+=, (const int), GameVar & ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "GameVar& opSubAssign(const int)", asMETHODPR( GameVar, operator-=, (const int), GameVar & ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "GameVar& opMulAssign(const int)", asMETHODPR( GameVar, operator*=, (const int), GameVar & ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "GameVar& opDivAssign(const int)", asMETHODPR( GameVar, operator/=, (const int), GameVar & ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "GameVar& opAssign(const int)", asMETHODPR( GameVar, operator=, (const int), GameVar & ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "GameVar& opAddAssign(const GameVar&)", asMETHODPR( GameVar, operator+=, ( const GameVar & ), GameVar & ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "GameVar& opSubAssign(const GameVar&)", asMETHODPR( GameVar, operator-=, ( const GameVar & ), GameVar & ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "GameVar& opMulAssign(const GameVar&)", asMETHODPR( GameVar, operator*=, ( const GameVar & ), GameVar & ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "GameVar& opDivAssign(const GameVar&)", asMETHODPR( GameVar, operator/=, ( const GameVar & ), GameVar & ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "GameVar& opAssign(const GameVar&)", asMETHODPR( GameVar, operator=, ( const GameVar & ), GameVar & ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int opAdd(const int)", asFUNCTION( GameVarAddInt ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int opSub(const int)", asFUNCTION( GameVarSubInt ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int opMul(const int)", asFUNCTION( GameVarMulInt ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int opDiv(const int)", asFUNCTION( GameVarDivInt ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int opAdd(const GameVar&)", asFUNCTION( GameVarAddGameVar ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int opSub(const GameVar&)", asFUNCTION( GameVarSubGameVar ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int opMul(const GameVar&)", asFUNCTION( GameVarMulGameVar ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int opDiv(const GameVar&)", asFUNCTION( GameVarDivGameVar ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "bool opEquals(const int)", asFUNCTION( GameVarEqualInt ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int opCmp(const int)", asFUNCTION( GameVarCmpInt ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "bool opEquals(const GameVar&)", asFUNCTION( GameVarEqualGameVar ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "GameVar", "int opCmp(const GameVar&)", asFUNCTION( GameVarCmpGameVar ), asCALL_CDECL_OBJFIRST ) );

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
BIND_ASSERT( engine->RegisterObjectProperty( "NpcPlane", "int Misc_ScriptId", OFFSETOF( AIDataPlane, Misc.ScriptBindId ) ) );
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

BIND_ASSERT( engine->RegisterObjectMethod( "NpcPlane", "NpcPlane@ GetCopy() const", asFUNCTION( BIND_CLASS NpcPlane_GetCopy ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "NpcPlane", "NpcPlane@+ SetChild(NpcPlane& child)", asFUNCTION( BIND_CLASS NpcPlane_SetChild ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "NpcPlane", "NpcPlane@+ GetChild(uint index) const", asFUNCTION( BIND_CLASS NpcPlane_GetChild ), asCALL_CDECL_OBJFIRST ) );

// BIND_ASSERT( engine->RegisterObjectMethod( "NpcPlane", "uint GetIndex() const", asFUNCTION(BIND_CLASS NpcPlane_GetIndex), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "NpcPlane", "bool Misc_SetScript(string& funcName)", asFUNCTION( BIND_CLASS NpcPlane_Misc_SetScript ), asCALL_CDECL_OBJFIRST ) );

/************************************************************************/
/* Item                                                                 */
/************************************************************************/
// Methods
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool IsStackable() const", asFUNCTION( BIND_CLASS Item_IsStackable ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool IsDeteriorable() const", asFUNCTION( BIND_CLASS Item_IsDeteriorable ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool SetScript(string@+ script)", asFUNCTION( BIND_CLASS Item_SetScript ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "hash GetScriptId() const", asFUNCTION( BIND_CLASS Item_GetScriptId ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool SetEvent(int eventType, string@+ funcName)", asFUNCTION( BIND_CLASS Item_SetEvent ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "uint8 GetType() const", asFUNCTION( BIND_CLASS Item_GetType ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "hash GetProtoId() const", asFUNCTION( BIND_CLASS Item_GetProtoId ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "uint GetCost() const", asFUNCTION( BIND_CLASS Item_GetCost ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "Item@+ AddItem(hash protoId, uint count, uint specialId)", asFUNCTION( BIND_CLASS Container_AddItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "Item@+ GetItem(hash protoId, uint specialId) const", asFUNCTION( BIND_CLASS Container_GetItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "uint GetItems(uint specialId, Item@[]@+ items) const", asFUNCTION( BIND_CLASS Container_GetItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "Map@+ GetMapPosition(uint16& hexX, uint16& hexY) const", asFUNCTION( BIND_CLASS Item_GetMapPosition ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool ChangeProto(hash protoId) const", asFUNCTION( BIND_CLASS Item_ChangeProto ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "void Animate(uint8 fromFrame, uint8 toFrame)", asFUNCTION( BIND_CLASS Item_Animate ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "void SetLexems(string@+ lexems)", asFUNCTION( BIND_CLASS Item_SetLexems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "Item@+ GetChild(uint childIndex) const", asFUNCTION( BIND_CLASS Item_GetChild ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Item", "void EventFinish(bool deleted)", asFUNCTION( BIND_CLASS Item_EventFinish ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool EventAttack(Critter& attacker, Critter& target)", asFUNCTION( BIND_CLASS Item_EventAttack ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool EventUse(Critter& cr, Critter@+ onCritter, Item@+ onItem, Scenery@+ onScenery)", asFUNCTION( BIND_CLASS Item_EventUse ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool EventUseOnMe(Critter& cr, Item@+ usedItem)", asFUNCTION( BIND_CLASS Item_EventUseOnMe ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool EventSkill(Critter& cr, int skill)", asFUNCTION( BIND_CLASS Item_EventSkill ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "void EventDrop(Critter& cr)", asFUNCTION( BIND_CLASS Item_EventDrop ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "void EventMove(Critter& cr, uint8 fromSlot)", asFUNCTION( BIND_CLASS Item_EventMove ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "void EventWalk(Critter& cr, bool entered, uint8 dir)", asFUNCTION( BIND_CLASS Item_EventWalk ), asCALL_CDECL_OBJFIRST ) );

// Parameters
BIND_ASSERT( engine->RegisterObjectProperty( "Item", "const uint Id", OFFSETOF( Item, Id ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Item", "const ProtoItem@ Proto", OFFSETOF( Item, Proto ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Item", "const uint8 Accessory", OFFSETOF( Item, Accessory ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Item", "const uint MapId", OFFSETOF( Item, AccHex.MapId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Item", "const uint16 HexX", OFFSETOF( Item, AccHex.HexX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Item", "const uint16 HexY", OFFSETOF( Item, AccHex.HexY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Item", "const uint CritId", OFFSETOF( Item, AccCritter.Id ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Item", "const uint8 CritSlot", OFFSETOF( Item, AccCritter.Slot ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Item", "const uint ContainerId", OFFSETOF( Item, AccContainer.ContainerId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Item", "const uint StackId", OFFSETOF( Item, AccContainer.StackId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Item", "const bool IsNotValid", OFFSETOF( Item, IsNotValid ) ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool LockerOpen()", asFUNCTION( BIND_CLASS Item_LockerOpen ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Item", "bool LockerClose()", asFUNCTION( BIND_CLASS Item_LockerClose ), asCALL_CDECL_OBJFIRST ) );

/************************************************************************/
/* CraftItem
   /************************************************************************/
BIND_ASSERT( engine->RegisterObjectType( "CraftItem", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "CraftItem", asBEHAVE_ADDREF, "void f()", asMETHOD( CraftItem, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "CraftItem", asBEHAVE_RELEASE, "void f()", asMETHOD( CraftItem, Release ), asCALL_THISCALL ) );

// Properties
BIND_ASSERT( engine->RegisterObjectProperty( "CraftItem", "const uint Num", OFFSETOF( CraftItem, Num ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CraftItem", "const string@ Name", OFFSETOF( CraftItem, Name ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CraftItem", "const string@ Info", OFFSETOF( CraftItem, Info ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CraftItem", "const uint Experience", OFFSETOF( CraftItem, Experience ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CraftItem", "const string@ Script", OFFSETOF( CraftItem, Script ) ) );

// Methods
BIND_ASSERT( engine->RegisterObjectMethod( "CraftItem", "uint GetShowParams(array<uint>@+ nums, array<int>@+ values, array<bool>@+ ors)", asFUNCTION( BIND_CLASS CraftItem_GetShowParams ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CraftItem", "uint GetNeedParams(array<uint>@+ nums, array<int>@+ values, array<bool>@+ ors)", asFUNCTION( BIND_CLASS CraftItem_GetNeedParams ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CraftItem", "uint GetNeedTools(array<hash>@+ pids, array<uint>@+ values, array<bool>@+ ors)", asFUNCTION( BIND_CLASS CraftItem_GetNeedTools ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CraftItem", "uint GetNeedItems(array<hash>@+ pids, array<uint>@+ values, array<bool>@+ ors)", asFUNCTION( BIND_CLASS CraftItem_GetNeedItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CraftItem", "uint GetOutItems(array<hash>@+ pids, array<uint>@+ values)", asFUNCTION( BIND_CLASS CraftItem_GetOutItems ), asCALL_CDECL_OBJFIRST ) );

/************************************************************************/
/* Scenery                                                              */
/************************************************************************/
BIND_ASSERT( engine->RegisterObjectProperty( "Scenery", "const hash ProtoId", OFFSETOF( MapObject, ProtoId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Scenery", "const uint16 HexX", OFFSETOF( MapObject, MapX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Scenery", "const uint16 HexY", OFFSETOF( MapObject, MapY ) ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Scenery", "bool CallSceneryFunction(Critter& cr, int skill, Item@+ item)", asFUNCTION( BIND_CLASS Scen_CallSceneryFunction ), asCALL_CDECL_OBJFIRST ) );

/************************************************************************/
/* Critter                                                              */
/************************************************************************/
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsPlayer() const", asFUNCTION( BIND_CLASS Crit_IsPlayer ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsNpc() const", asFUNCTION( BIND_CLASS Crit_IsNpc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsCanWalk() const", asFUNCTION( BIND_CLASS Crit_IsCanWalk ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsCanRun() const", asFUNCTION( BIND_CLASS Crit_IsCanRun ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsCanRotate() const", asFUNCTION( BIND_CLASS Crit_IsCanRotate ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsCanAim() const", asFUNCTION( BIND_CLASS Crit_IsCanAim ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsAnim1(uint index) const", asFUNCTION( BIND_CLASS Crit_IsAnim1 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "int GetAccess() const", asFUNCTION( BIND_CLASS Cl_GetAccess ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool SetAccess(int access)", asFUNCTION( BIND_CLASS Cl_SetAccess ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool SetEvent(int eventType, string@+ funcName)", asFUNCTION( BIND_CLASS Crit_SetEvent ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetLexems(string@+ lexems)", asFUNCTION( BIND_CLASS Crit_SetLexems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Map@+ GetMap() const", asFUNCTION( BIND_CLASS Crit_GetMap ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetMapId() const", asFUNCTION( BIND_CLASS Crit_GetMapId ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "hash GetMapProtoId() const", asFUNCTION( BIND_CLASS Crit_GetMapProtoId ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetHomePos(uint16 hexX, uint16 hexY, uint8 dir)", asFUNCTION( BIND_CLASS Crit_SetHomePos ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void GetHomePos(uint& mapId, uint16& hexX, uint16& hexY, uint8& dir)", asFUNCTION( BIND_CLASS Crit_GetHomePos ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool ChangeCrType(uint newType)", asFUNCTION( BIND_CLASS Crit_ChangeCrType ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void DropTimers()", asFUNCTION( BIND_CLASS Cl_DropTimers ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool MoveRandom()", asFUNCTION( BIND_CLASS Crit_MoveRandom ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool MoveToDir(uint8 dir)", asFUNCTION( BIND_CLASS Crit_MoveToDir ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool TransitToHex(uint16 hexX, uint16 hexY, uint8 dir)", asFUNCTION( BIND_CLASS Crit_TransitToHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool TransitToMap(uint mapId, uint16 hexX, uint16 hexY, uint8 dir, bool withGroup = false)", asFUNCTION( BIND_CLASS Crit_TransitToMapHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool TransitToMap(uint mapId, int entireNum, bool withGroup = false)", asFUNCTION( BIND_CLASS Crit_TransitToMapEntire ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool TransitToGlobal(bool requestGroup)", asFUNCTION( BIND_CLASS Crit_TransitToGlobal ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool TransitToGlobal(Critter@[]& group)", asFUNCTION( BIND_CLASS Crit_TransitToGlobalWithGroup ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool TransitToGlobalGroup(uint critterId)", asFUNCTION( BIND_CLASS Crit_TransitToGlobalGroup ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void AddScore(uint score, int val)", asFUNCTION( BIND_CLASS Crit_AddScore ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "int GetScore(uint score)", asFUNCTION( BIND_CLASS Crit_GetScore ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void AddHolodiskInfo(uint holodiskNum)", asFUNCTION( BIND_CLASS Crit_AddHolodiskInfo ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EraseHolodiskInfo(uint holodiskNum)", asFUNCTION( BIND_CLASS Crit_EraseHolodiskInfo ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsHolodiskInfo(uint holodiskNum) const", asFUNCTION( BIND_CLASS Crit_IsHolodiskInfo ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsLife() const", asFUNCTION( BIND_CLASS Crit_IsLife ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsKnockout() const", asFUNCTION( BIND_CLASS Crit_IsKnockout ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsDead() const", asFUNCTION( BIND_CLASS Crit_IsDead ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsFree() const", asFUNCTION( BIND_CLASS Crit_IsFree ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsBusy() const", asFUNCTION( BIND_CLASS Crit_IsBusy ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Wait(uint ms)", asFUNCTION( BIND_CLASS Crit_Wait ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ToDead(uint anim2, Critter@+ killer)", asFUNCTION( BIND_CLASS Crit_ToDead ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool ToLife()", asFUNCTION( BIND_CLASS Crit_ToLife ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool ToKnockout(uint anim2begin, uint anim2idle, uint anim2end, uint lostAp, uint16 knockHx, uint16 knockHy)", asFUNCTION( BIND_CLASS Crit_ToKnockout ), asCALL_CDECL_OBJFIRST ) );

// BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void GetProtoData(int[]& data)", asFUNCTION( BIND_CLASS Npc_GetProtoData ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void RefreshVisible()", asFUNCTION( BIND_CLASS Crit_RefreshVisible ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ViewMap(Map& map, uint look, uint16 hx, uint16 hy, uint8 dir)", asFUNCTION( BIND_CLASS Crit_ViewMap ), asCALL_CDECL_OBJFIRST ) );
// BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Mute(uint ms)", asFUNCTION( BIND_CLASS Crit_Mute ), asCALL_CDECL_OBJFIRST ) );
// BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Ban(uint ms)", asFUNCTION( BIND_CLASS Crit_Mute ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ AddItem(hash protoId, uint count)", asFUNCTION( BIND_CLASS Crit_AddItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool DeleteItem(hash protoId, uint count)", asFUNCTION( BIND_CLASS Crit_DeleteItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint ItemsCount() const", asFUNCTION( BIND_CLASS Crit_ItemsCount ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint ItemsWeight() const", asFUNCTION( BIND_CLASS Crit_ItemsWeight ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint ItemsVolume() const", asFUNCTION( BIND_CLASS Crit_ItemsVolume ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint CountItem(hash protoId) const", asFUNCTION( BIND_CLASS Crit_CountItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItem(hash protoId, int slot) const", asFUNCTION( BIND_CLASS Crit_GetItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Item@+ GetItemById(uint itemId) const", asFUNCTION( BIND_CLASS Crit_GetItemById ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetItems(int slot, Item@[]@+ items) const", asFUNCTION( BIND_CLASS Crit_GetItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetItemsByType(int type, Item@[]@+ items) const", asFUNCTION( BIND_CLASS Crit_GetItemsByType ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "ProtoItem@+ GetSlotProto(int slot, uint8& mode) const", asFUNCTION( BIND_CLASS Crit_GetSlotProto ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool MoveItem(uint itemId, uint count, uint8 toSlot)", asFUNCTION( BIND_CLASS Crit_MoveItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool PickItem(uint16 hexX, uint16 hexY, hash protoId)", asFUNCTION( BIND_CLASS Crit_PickItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetFavoriteItem(int slot, hash pid)", asFUNCTION( BIND_CLASS Crit_SetFavoriteItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "hash GetFavoriteItem(int slot)", asFUNCTION( BIND_CLASS Crit_GetFavoriteItem ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetCritters(bool lookOnMe, int findType, Critter@[]@+ critters) const", asFUNCTION( BIND_CLASS Crit_GetCritters ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetFollowGroup(int findType, Critter@[]@+ critters) const", asFUNCTION( BIND_CLASS Crit_GetFollowGroup ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Critter@+ GetFollowLeader() const", asFUNCTION( BIND_CLASS Crit_GetFollowLeader ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "Critter@[]@ GetGlobalGroup() const", asFUNCTION( BIND_CLASS Crit_GetGlobalGroup ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsGlobalGroupLeader()", asFUNCTION( BIND_CLASS Crit_IsGlobalGroupLeader ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void LeaveGlobalGroup()", asFUNCTION( BIND_CLASS Crit_LeaveGlobalGroup ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void GiveGlobalGroupLead(Critter& toCr)", asFUNCTION( BIND_CLASS Crit_GiveGlobalGroupLead ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetTalkedPlayers(Critter@[]@+ players) const", asFUNCTION( BIND_CLASS Npc_GetTalkedPlayers ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsSee(Critter& cr) const", asFUNCTION( BIND_CLASS Crit_IsSeeCr ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsSeenBy(Critter& cr) const", asFUNCTION( BIND_CLASS Crit_IsSeenByCr ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsSee(Item& item) const", asFUNCTION( BIND_CLASS Crit_IsSeeItem ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Say(uint8 howSay, string& text)", asFUNCTION( BIND_CLASS Crit_Say ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SayMsg(uint8 howSay, uint16 textMsg, uint strNum)", asFUNCTION( BIND_CLASS Crit_SayMsg ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SayMsg(uint8 howSay, uint16 textMsg, uint strNum, string& lexems)", asFUNCTION( BIND_CLASS Crit_SayMsgLex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetDir(uint8 dir)", asFUNCTION( BIND_CLASS Crit_SetDir ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint ErasePlane(int planeType, bool all)", asFUNCTION( BIND_CLASS Npc_ErasePlane ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool ErasePlane(uint index)", asFUNCTION( BIND_CLASS Npc_ErasePlaneIndex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void DropPlanes()", asFUNCTION( BIND_CLASS Npc_DropPlanes ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsNoPlanes() const", asFUNCTION( BIND_CLASS Npc_IsNoPlanes ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsCurPlane(int planeType) const", asFUNCTION( BIND_CLASS Npc_IsCurPlane ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "NpcPlane@+ GetCurPlane() const", asFUNCTION( BIND_CLASS Npc_GetCurPlane ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetPlanes(NpcPlane@[]@+ planes) const", asFUNCTION( BIND_CLASS Npc_GetPlanes ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetPlanes(int identifier, NpcPlane@[]@+ planes) const", asFUNCTION( BIND_CLASS Npc_GetPlanesIdentifier ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetPlanes(int identifier, uint identifierExt, NpcPlane@[]@+ planes) const", asFUNCTION( BIND_CLASS Npc_GetPlanesIdentifier2 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool AddPlane(NpcPlane& plane)", asFUNCTION( BIND_CLASS Npc_AddPlane ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SendMessage(int num, int val, int to)", asFUNCTION( BIND_CLASS Crit_SendMessage ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Action(int action, int actionExt, Item@+ item)", asFUNCTION( BIND_CLASS Crit_Action ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Animate(uint anim1, uint anim2, Item@+ item, bool clearSequence, bool delayPlay)", asFUNCTION( BIND_CLASS Crit_Animate ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetAnims(int cond, uint anim1, uint anim2)", asFUNCTION( BIND_CLASS Crit_SetAnims ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void PlaySound(string& soundName, bool sendSelf)", asFUNCTION( BIND_CLASS Crit_PlaySound ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void PlaySound(uint8 soundType, uint8 soundTypeExt, uint8 soundId, uint8 soundIdExt, bool sendSelf)", asFUNCTION( BIND_CLASS Crit_PlaySoundType ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SendCombatResult(uint[]& combatResult)", asFUNCTION( BIND_CLASS Crit_SendCombatResult ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool IsKnownLoc(bool byId, uint locNum) const", asFUNCTION( BIND_CLASS Cl_IsKnownLoc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool SetKnownLoc(bool byId, uint locNum)", asFUNCTION( BIND_CLASS Cl_SetKnownLoc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool UnsetKnownLoc(bool byId, uint locNum)", asFUNCTION( BIND_CLASS Cl_UnsetKnownLoc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetFog(uint16 zoneX, uint16 zoneY, int fog)", asFUNCTION( BIND_CLASS Cl_SetFog ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "int GetFog(uint16 zoneX, uint16 zoneY)", asFUNCTION( BIND_CLASS Cl_GetFog ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ShowContainer(Critter@+ contCr, Item@+ contItem, uint8 transferType)", asFUNCTION( BIND_CLASS Cl_ShowContainer ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ShowScreen(int screenType, uint param, string@+ funcName)", asFUNCTION( BIND_CLASS Cl_ShowScreen ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void RunClientScript(string& funcName, int p0, int p1, int p2, string@+ p3, int[]@+ p4)", asFUNCTION( BIND_CLASS Cl_RunClientScript ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void Disconnect()", asFUNCTION( BIND_CLASS Cl_Disconnect ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool SetScript(string@+ script)", asFUNCTION( BIND_CLASS Crit_SetScript ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "hash GetScriptId() const", asFUNCTION( BIND_CLASS Crit_GetScriptId ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetBagRefreshTime(uint realMinutes)", asFUNCTION( BIND_CLASS Crit_SetBagRefreshTime ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetBagRefreshTime() const", asFUNCTION( BIND_CLASS Crit_GetBagRefreshTime ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetInternalBag(hash[]& pids, uint[]@+ minCounts, uint[]@+ maxCounts, int[]@+ slots)", asFUNCTION( BIND_CLASS Crit_SetInternalBag ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetInternalBag(hash[]@+ pids, uint[]@+ minCounts, uint[]@+ maxCounts, int[]@+ slots) const", asFUNCTION( BIND_CLASS Crit_GetInternalBag ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "hash GetProtoId() const", asFUNCTION( BIND_CLASS Crit_GetProtoId ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetMultihex() const", asFUNCTION( BIND_CLASS Crit_GetMultihex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void SetMultihex(int value)", asFUNCTION( BIND_CLASS Crit_SetMultihex ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void AddEnemyInStack(uint critterId)", asFUNCTION( BIND_CLASS Crit_AddEnemyInStack ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool CheckEnemyInStack(uint critterId) const", asFUNCTION( BIND_CLASS Crit_CheckEnemyInStack ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EraseEnemyFromStack(uint critterId)", asFUNCTION( BIND_CLASS Crit_EraseEnemyFromStack ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ChangeEnemyStackSize(uint newSize)", asFUNCTION( BIND_CLASS Crit_ChangeEnemyStackSize ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void GetEnemyStack(uint[]& enemyStack) const", asFUNCTION( BIND_CLASS Crit_GetEnemyStack ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ClearEnemyStack()", asFUNCTION( BIND_CLASS Crit_ClearEnemyStack ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ClearEnemyStackNpc()", asFUNCTION( BIND_CLASS Crit_ClearEnemyStackNpc ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool AddTimeEvent(string& funcName, uint duration, int identifier)", asFUNCTION( BIND_CLASS Crit_AddTimeEvent ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool AddTimeEvent(string& funcName, uint duration, int identifier, uint rate)", asFUNCTION( BIND_CLASS Crit_AddTimeEventRate ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetTimeEvents(int identifier, uint[]@+ indexes, uint[]@+ durations, uint[]@+ rates) const", asFUNCTION( BIND_CLASS Crit_GetTimeEvents ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint GetTimeEvents(int[]& findIdentifiers, int[]@+ identifiers, uint[]@+ indexes, uint[]@+ durations, uint[]@+ rates) const", asFUNCTION( BIND_CLASS Crit_GetTimeEventsArr ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void ChangeTimeEvent(uint index, uint newDuration, uint newRate)", asFUNCTION( BIND_CLASS Crit_ChangeTimeEvent ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EraseTimeEvent(uint index)", asFUNCTION( BIND_CLASS Crit_EraseTimeEvent ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint EraseTimeEvents(int identifier)", asFUNCTION( BIND_CLASS Crit_EraseTimeEvents ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "uint EraseTimeEvents(int[]& identifiers)", asFUNCTION( BIND_CLASS Crit_EraseTimeEventsArr ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventIdle()", asFUNCTION( BIND_CLASS Crit_EventIdle ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventFinish(bool deleted)", asFUNCTION( BIND_CLASS Crit_EventFinish ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventDead(Critter@+ killer)", asFUNCTION( BIND_CLASS Crit_EventDead ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventRespawn()", asFUNCTION( BIND_CLASS Crit_EventRespawn ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventShowCritter(Critter& cr)", asFUNCTION( BIND_CLASS Crit_EventShowCritter ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventShowCritter1(Critter& cr)", asFUNCTION( BIND_CLASS Crit_EventShowCritter1 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventShowCritter2(Critter& cr)", asFUNCTION( BIND_CLASS Crit_EventShowCritter2 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventShowCritter3(Critter& cr)", asFUNCTION( BIND_CLASS Crit_EventShowCritter3 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventHideCritter(Critter& cr)", asFUNCTION( BIND_CLASS Crit_EventHideCritter ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventHideCritter1(Critter& cr)", asFUNCTION( BIND_CLASS Crit_EventHideCritter1 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventHideCritter2(Critter& cr)", asFUNCTION( BIND_CLASS Crit_EventHideCritter2 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventHideCritter3(Critter& cr)", asFUNCTION( BIND_CLASS Crit_EventHideCritter3 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventShowItemOnMap(Item& showItem, bool added, Critter@+ dropper)", asFUNCTION( BIND_CLASS Crit_EventShowItemOnMap ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventChangeItemOnMap(Item& item)", asFUNCTION( BIND_CLASS Crit_EventChangeItemOnMap ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventHideItemOnMap(Item& hideItem, bool removed, Critter@+ picker)", asFUNCTION( BIND_CLASS Crit_EventHideItemOnMap ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool EventAttack(Critter& target)", asFUNCTION( BIND_CLASS Crit_EventAttack ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool EventAttacked(Critter@+ attacker)", asFUNCTION( BIND_CLASS Crit_EventAttacked ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool EventStealing(Critter& thief, Item& item, uint count)", asFUNCTION( BIND_CLASS Crit_EventStealing ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventMessage(Critter& fromCr, int message, int value)", asFUNCTION( BIND_CLASS Crit_EventMessage ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool EventUseItem(Item& item, Critter@+ onCritter, Item@+ onItem, Scenery@+ onScenery)", asFUNCTION( BIND_CLASS Crit_EventUseItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool EventUseItemOnMe(Critter& whoUse, Item& item)", asFUNCTION( BIND_CLASS Crit_EventUseItemOnMe ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool EventUseSkill(int skill, Critter@+ onCritter, Item@+ onItem, Scenery@+ onScenery)", asFUNCTION( BIND_CLASS Crit_EventUseSkill ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool EventUseSkillOnMe(Critter& whoUse, int skill)", asFUNCTION( BIND_CLASS Crit_EventUseSkillOnMe ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventDropItem(Item& item)", asFUNCTION( BIND_CLASS Crit_EventDropItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventMoveItem(Item& item, uint8 fromSlot)", asFUNCTION( BIND_CLASS Crit_EventMoveItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventKnockout(uint anim2begin, uint anim2idle, uint anim2end, uint lostAp, uint knockDist)", asFUNCTION( BIND_CLASS Crit_EventKnockout ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventSmthDead(Critter& fromCr, Critter@+ killer)", asFUNCTION( BIND_CLASS Crit_EventSmthDead ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventSmthStealing(Critter& fromCr, Critter& thief, bool success, Item& item, uint count)", asFUNCTION( BIND_CLASS Crit_EventSmthStealing ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventSmthAttack(Critter& fromCr, Critter& target)", asFUNCTION( BIND_CLASS Crit_EventSmthAttack ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventSmthAttacked(Critter& fromCr, Critter@+ attacker)", asFUNCTION( BIND_CLASS Crit_EventSmthAttacked ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventSmthUseItem(Critter& fromCr, Item& item, Critter@+ onCritter, Item@+ onItem, Scenery@+ onScenery)", asFUNCTION( BIND_CLASS Crit_EventSmthUseItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventSmthUseSkill(Critter& fromCr, int skill, Critter@+ onCritter, Item@+ onItem, Scenery@+ onScenery)", asFUNCTION( BIND_CLASS Crit_EventSmthUseSkill ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventSmthDropItem(Critter& fromCr, Item& item)", asFUNCTION( BIND_CLASS Crit_EventSmthDropItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventSmthMoveItem(Critter& fromCr, Item& item, uint8 fromSlot)", asFUNCTION( BIND_CLASS Crit_EventSmthMoveItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventSmthKnockout(Critter& fromCr, uint anim2begin, uint anim2idle, uint anim2end, uint lostAp, uint knockDist)", asFUNCTION( BIND_CLASS Crit_EventSmthKnockout ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "int EventPlaneBegin(NpcPlane& plane, int reason, Critter@+ someCr, Item@+ someItem)", asFUNCTION( BIND_CLASS Crit_EventPlaneBegin ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "int EventPlaneEnd(NpcPlane& plane, int reason, Critter@+ someCr, Item@+ someItem)", asFUNCTION( BIND_CLASS Crit_EventPlaneEnd ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "int EventPlaneRun(NpcPlane& plane, int reason, uint& p0, uint& p1, uint& p2)", asFUNCTION( BIND_CLASS Crit_EventPlaneRun ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool EventBarter(Critter& barterCr, bool attach, uint barterCount)", asFUNCTION( BIND_CLASS Crit_EventBarter ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool EventTalk(Critter& talkCr, bool attach, uint talkCount)", asFUNCTION( BIND_CLASS Crit_EventTalk ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool EventGlobalProcess(int type, Item@ car, float& x, float& y, float& toX, float& toY, float& speed, uint& encounterDescriptor, bool& waitForAnswer)", asFUNCTION( BIND_CLASS Crit_EventGlobalProcess ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "bool EventGlobalInvite(Item@ car, uint encounterDescriptor, int combatMode, uint& mapId, uint16& hexX, uint16& hexY, uint8& dir)", asFUNCTION( BIND_CLASS Crit_EventGlobalInvite ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventTurnBasedProcess(Map& map, bool beginTurn)", asFUNCTION( BIND_CLASS Crit_EventTurnBasedProcess ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Critter", "void EventSmthTurnBasedProcess(Critter& fromCr, Map& map, bool beginTurn)", asFUNCTION( BIND_CLASS Crit_EventSmthTurnBasedProcess ), asCALL_CDECL_OBJFIRST ) );

// Parameters
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint Id", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, Id ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint CrType", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, BaseType ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint16 HexX", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, HexX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint16 HexY", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, HexY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint16 WorldX", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, WorldX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint16 WorldY", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, WorldY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint8 Dir", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, Dir ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint8 Cond", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, Cond ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint Anim1Life", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, Anim1Life ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint Anim1Knockout", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, Anim1Knockout ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint Anim1Dead", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, Anim1Dead ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint Anim2Life", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, Anim2Life ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint Anim2Knockout", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, Anim2Knockout ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint Anim2Dead", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, Anim2Dead ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const uint Flags", OFFSETOF( Critter, Flags ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const string@ Name", OFFSETOF( Critter, NameStr ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "uint ShowCritterDist1", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, ShowCritterDist1 ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "uint ShowCritterDist2", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, ShowCritterDist2 ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "uint ShowCritterDist3", OFFSETOF( Critter, Data ) + OFFSETOF( CritData, ShowCritterDist3 ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "bool IsRuning", OFFSETOF( Critter, IsRuning ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const bool IsNotValid", OFFSETOF( Critter, IsNotValid ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "const int Ref", OFFSETOF( Critter, RefCounter ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "DataVal Param", OFFSETOF( Critter, ThisPtr[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Critter", "DataRef ParamBase", OFFSETOF( Critter, ThisPtr[ 0 ] ) ) );

/************************************************************************/
/* Map                                                                  */
/************************************************************************/
BIND_ASSERT( engine->RegisterObjectProperty( "Map", "const bool IsNotValid", OFFSETOF( Map, IsNotValid ) ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "hash GetProtoId() const", asFUNCTION( BIND_CLASS Map_GetProtoId ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Location@+ GetLocation() const", asFUNCTION( BIND_CLASS Map_GetLocation ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool SetScript(string@+ script)", asFUNCTION( BIND_CLASS Map_SetScript ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "hash GetScriptId() const", asFUNCTION( BIND_CLASS Map_GetScriptId ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool SetEvent(int eventType, string@+ funcName)", asFUNCTION( BIND_CLASS Map_SetEvent ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetLoopTime(uint numLoop, uint ms)", asFUNCTION( BIND_CLASS Map_SetLoopTime ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint8 GetRain() const", asFUNCTION( BIND_CLASS Map_GetRain ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetRain(uint8 capacity)", asFUNCTION( BIND_CLASS Map_SetRain ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "int GetTime() const", asFUNCTION( BIND_CLASS Map_GetTime ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetTime(int time)", asFUNCTION( BIND_CLASS Map_SetTime ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetDayTime(uint dayPart) const", asFUNCTION( BIND_CLASS Map_GetDayTime ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetDayTime(uint dayPart, uint time)", asFUNCTION( BIND_CLASS Map_SetDayTime ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void GetDayColor(uint dayPart, uint8& r, uint8& g, uint8& b) const", asFUNCTION( BIND_CLASS Map_GetDayColor ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetDayColor(uint dayPart, uint8 r, uint8 g, uint8 b)", asFUNCTION( BIND_CLASS Map_SetDayColor ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetTurnBasedAvailability(bool value)", asFUNCTION( BIND_CLASS Map_SetTurnBasedAvailability ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool IsTurnBasedAvailability() const", asFUNCTION( BIND_CLASS Map_IsTurnBasedAvailability ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void BeginTurnBased(Critter@+ firstTurnCrit)", asFUNCTION( BIND_CLASS Map_BeginTurnBased ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool IsTurnBased() const", asFUNCTION( BIND_CLASS Map_IsTurnBased ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EndTurnBased()", asFUNCTION( BIND_CLASS Map_EndTurnBased ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "int GetTurnBasedSequence(uint[]& crittersIds) const", asFUNCTION( BIND_CLASS Map_GetTurnBasedSequence ), asCALL_CDECL_OBJFIRST ) );
// BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetTurnBasedSequence(uint[]& crittersIds)", asFUNCTION( BIND_CLASS Map_SetTurnBasedSequence ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetData(uint index, int value)", asFUNCTION( BIND_CLASS Map_SetData ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "int GetData(uint index) const", asFUNCTION( BIND_CLASS Map_GetData ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ AddItem(uint16 hexX, uint16 hexY, hash protoId, uint count)", asFUNCTION( BIND_CLASS Map_AddItem ), asCALL_CDECL_OBJFIRST ) );
// BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ AddItemFromScenery(uint16 hexX, uint16 hexY, hash protoId)", asFUNCTION( BIND_CLASS Map_AddItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ GetItem(uint itemId) const", asFUNCTION( BIND_CLASS Map_GetItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ GetItem(uint16 hexX, uint16 hexY, hash protoId) const", asFUNCTION( BIND_CLASS Map_GetItemHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetItems(uint16 hexX, uint16 hexY, Item@[]@+ items) const", asFUNCTION( BIND_CLASS Map_GetItemsHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetItems(uint16 hexX, uint16 hexY, uint radius, hash protoId, Item@[]@+ items) const", asFUNCTION( BIND_CLASS Map_GetItemsHexEx ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetItems(hash protoId, Item@[]@+ items) const", asFUNCTION( BIND_CLASS Map_GetItemsByPid ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetItemsByType(int type, Item@[]@+ items) const", asFUNCTION( BIND_CLASS Map_GetItemsByType ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ GetDoor(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Map_GetDoor ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Item@+ GetCar(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Map_GetCar ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Scenery@+ GetScenery(uint16 hexX, uint16 hexY, hash protoId) const", asFUNCTION( BIND_CLASS Map_GetSceneryHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetSceneries(uint16 hexX, uint16 hexY, Scenery@[]@+ sceneries) const", asFUNCTION( BIND_CLASS Map_GetSceneriesHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetSceneries(uint16 hexX, uint16 hexY, uint radius, hash protoId, Scenery@[]@+ sceneries) const", asFUNCTION( BIND_CLASS Map_GetSceneriesHexEx ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetSceneries(hash protoId, Scenery@[]@+ sceneries) const", asFUNCTION( BIND_CLASS Map_GetSceneriesByPid ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Critter@+ GetCritter(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Map_GetCritterHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Critter@+ GetCritter(uint critterId) const", asFUNCTION( BIND_CLASS Map_GetCritterById ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetCrittersHex(uint16 hexX, uint16 hexY, uint radius, int findType, Critter@[]@+ critters) const", asFUNCTION( BIND_CLASS Map_GetCritters ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetCritters(hash pid, int findType, Critter@[]@+ critters) const", asFUNCTION( BIND_CLASS Map_GetCrittersByPids ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, Critter@[]@+ critters) const", asFUNCTION( BIND_CLASS Map_GetCrittersInPath ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, Critter@[]@+ critters, uint16& preBlockHx, uint16& preBlockHy, uint16& blockHx, uint16& blockHy) const", asFUNCTION( BIND_CLASS Map_GetCrittersInPathBlock ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetCrittersWhoViewPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, int findType, Critter@[]@+ critters) const", asFUNCTION( BIND_CLASS Map_GetCrittersWhoViewPath ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetCrittersSeeing(Critter@[]& critters, bool lookOnThem, int find_type, Critter@[]@+ crittersResult) const", asFUNCTION( BIND_CLASS Map_GetCrittersSeeing ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void GetHexCoord(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist) const", asFUNCTION( BIND_CLASS Map_GetHexInPath ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void GetHexCoordWall(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist) const", asFUNCTION( BIND_CLASS Map_GetHexInPathWall ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetPathLength(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut) const", asFUNCTION( BIND_CLASS Map_GetPathLengthHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetPathLength(Critter& cr, uint16 toHx, uint16 toHy, uint cut) const", asFUNCTION( BIND_CLASS Map_GetPathLengthCr ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool VerifyTrigger(Critter& cr, uint16 hexX, uint16 hexY, uint8 dir)", asFUNCTION( BIND_CLASS Map_VerifyTrigger ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Critter@+ AddNpc(hash protoId, uint16 hexX, uint16 hexY, uint8 dir, int[]@+ params, int[]@+ items, string@+ script)", asFUNCTION( BIND_CLASS Map_AddNpc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetNpcCount(int npcRole, int findType) const", asFUNCTION( BIND_CLASS Map_GetNpcCount ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "Critter@+ GetNpc(int npcRole, int findType, uint skipCount) const", asFUNCTION( BIND_CLASS Map_GetNpc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint CountEntire(int entire) const", asFUNCTION( BIND_CLASS Map_CountEntire ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint GetEntires(int entire, uint[]@+ entires, uint16[]@+ hexX, uint16[]@+ hexY) const", asFUNCTION( BIND_CLASS Map_GetEntires ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool GetEntireCoords(int entire, uint skip, uint16& hexX, uint16& hexY) const", asFUNCTION( BIND_CLASS Map_GetEntireCoords ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool GetEntireCoords(int entire, uint skip, uint16& hexX, uint16& hexY, uint8& dir) const", asFUNCTION( BIND_CLASS Map_GetEntireCoordsDir ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool GetNearEntireCoords(int& entire, uint16& hexX, uint16& hexY) const", asFUNCTION( BIND_CLASS Map_GetNearEntireCoords ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool GetNearEntireCoords(int& entire, uint16& hexX, uint16& hexY, uint8& dir) const", asFUNCTION( BIND_CLASS Map_GetNearEntireCoordsDir ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool IsHexPassed(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Map_IsHexPassed ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool IsHexRaked(uint16 hexX, uint16 hexY) const", asFUNCTION( BIND_CLASS Map_IsHexRaked ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetText(uint16 hexX, uint16 hexY, uint color, string& text) const", asFUNCTION( BIND_CLASS Map_SetText ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetTextMsg(uint16 hexX, uint16 hexY, uint color, uint16 textMsg, uint strNum) const", asFUNCTION( BIND_CLASS Map_SetTextMsg ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void SetTextMsg(uint16 hexX, uint16 hexY, uint color, uint16 textMsg, uint strNum, string& lexems) const", asFUNCTION( BIND_CLASS Map_SetTextMsgLex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void RunEffect(hash effectPid, uint16 hexX, uint16 hexY, uint16 radius) const", asFUNCTION( BIND_CLASS Map_RunEffect ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void RunFlyEffect(hash effectPid, Critter@+ fromCr, Critter@+ toCr, uint16 fromX, uint16 fromY, uint16 toX, uint16 toY) const", asFUNCTION( BIND_CLASS Map_RunFlyEffect ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool CheckPlaceForItem(uint16 hexX, uint16 hexY, hash pid) const", asFUNCTION( BIND_CLASS Map_CheckPlaceForItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void BlockHex(uint16 hexX, uint16 hexY, bool full)", asFUNCTION( BIND_CLASS Map_BlockHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void UnblockHex(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS Map_UnblockHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void PlaySound(string& soundName) const", asFUNCTION( BIND_CLASS Map_PlaySound ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void PlaySound(string& soundName, uint16 hexX, uint16 hexY, uint radius) const", asFUNCTION( BIND_CLASS Map_PlaySoundRadius ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "bool Reload()", asFUNCTION( BIND_CLASS Map_Reload ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint16 GetWidth() const", asFUNCTION( BIND_CLASS Map_GetWidth ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "uint16 GetHeight() const", asFUNCTION( BIND_CLASS Map_GetHeight ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void MoveHexByDir(uint16& hexX, uint16& hexY, uint8 dir, uint steps) const", asFUNCTION( BIND_CLASS Map_MoveHexByDir ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EventFinish(bool deleted)", asFUNCTION( BIND_CLASS Map_EventFinish ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EventLoop0()", asFUNCTION( BIND_CLASS Map_EventLoop0 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EventLoop1()", asFUNCTION( BIND_CLASS Map_EventLoop1 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EventLoop2()", asFUNCTION( BIND_CLASS Map_EventLoop2 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EventLoop3()", asFUNCTION( BIND_CLASS Map_EventLoop3 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EventLoop4()", asFUNCTION( BIND_CLASS Map_EventLoop4 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EventInCritter(Critter& cr)", asFUNCTION( BIND_CLASS Map_EventInCritter ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EventOutCritter(Critter& cr)", asFUNCTION( BIND_CLASS Map_EventOutCritter ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EventCritterDead(Critter& cr, Critter@+ killer)", asFUNCTION( BIND_CLASS Map_EventCritterDead ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EventTurnBasedBegin()", asFUNCTION( BIND_CLASS Map_EventTurnBasedBegin ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EventTurnBasedEnd()", asFUNCTION( BIND_CLASS Map_EventTurnBasedEnd ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Map", "void EventTurnBasedProcess(Critter& cr, bool beginTurn)", asFUNCTION( BIND_CLASS Map_EventTurnBasedProcess ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectProperty( "Map", "const uint Id", OFFSETOF( Map, Data.MapId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Map", "const uint TurnBasedRound", OFFSETOF( Map, TurnBasedRound ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Map", "const uint TurnBasedTurn", OFFSETOF( Map, TurnBasedTurn ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Map", "const uint TurnBasedWholeTurn", OFFSETOF( Map, TurnBasedWholeTurn ) ) );

/************************************************************************/
/* Location                                                             */
/************************************************************************/
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "hash GetProtoId() const", asFUNCTION( BIND_CLASS Location_GetProtoId ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "bool SetEvent(int eventType, string@+ funcName)", asFUNCTION( BIND_CLASS Location_SetEvent ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "uint GetMapCount() const", asFUNCTION( BIND_CLASS Location_GetMapCount ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "Map@+ GetMap(hash mapPid) const", asFUNCTION( BIND_CLASS Location_GetMap ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "Map@+ GetMapByIndex(uint index) const", asFUNCTION( BIND_CLASS Location_GetMapByIndex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "uint GetMaps(Map@[]@+ maps) const", asFUNCTION( BIND_CLASS Location_GetMaps ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "bool GetEntrance(uint entrance, uint& mapIndex, uint& entire) const", asFUNCTION( BIND_CLASS Location_GetEntrance ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "uint GetEntrances(uint[]@+ mapsIndex, uint[]@+ entires) const", asFUNCTION( BIND_CLASS Location_GetEntrance ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "bool Reload()", asFUNCTION( BIND_CLASS Location_Reload ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "void Update()", asFUNCTION( BIND_CLASS Location_Update ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectMethod( "Location", "void EventFinish(bool deleted)", asFUNCTION( BIND_CLASS Location_EventFinish ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "Location", "bool EventEnter(Critter@[]& group, uint8 entrance)", asFUNCTION( BIND_CLASS Location_EventEnter ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectProperty( "Location", "const uint Id", OFFSETOF( Location, Data.LocId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Location", "uint16 WorldX", OFFSETOF( Location, Data.WX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Location", "uint16 WorldY", OFFSETOF( Location, Data.WY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Location", "bool Visible", OFFSETOF( Location, Data.Visible ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Location", "bool GeckVisible", OFFSETOF( Location, Data.GeckVisible ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Location", "bool AutoGarbage", OFFSETOF( Location, Data.AutoGarbage ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Location", "int GeckCount", OFFSETOF( Location, GeckCount ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Location", "uint16 Radius", OFFSETOF( Location, Data.Radius ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Location", "uint Color", OFFSETOF( Location, Data.Color ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "Location", "const bool IsNotValid", OFFSETOF( Location, IsNotValid ) ) );

/************************************************************************/
/* Global                                                               */
/************************************************************************/
BIND_ASSERT( engine->RegisterGlobalFunction( "GameVar@+ GetGlobalVar(uint16 varId)", asFUNCTION( BIND_CLASS Global_GetGlobalVar ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "GameVar@+ GetLocalVar(uint16 varId, uint masterId)", asFUNCTION( BIND_CLASS Global_GetLocalVar ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "GameVar@+ GetUnicumVar(uint16 varId, uint masterId, uint slaveId)", asFUNCTION( BIND_CLASS Global_GetUnicumVar ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Item@+ GetItem(uint itemId)", asFUNCTION( BIND_CLASS Global_GetItem ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItem(Item& item, uint count, Critter& toCr)", asFUNCTION( BIND_CLASS Global_MoveItemCr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItem(Item& item, uint count, Item& toCont, uint stackId)", asFUNCTION( BIND_CLASS Global_MoveItemCont ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItem(Item& item, uint count, Map& toMap, uint16 toHx, uint16 toHy)", asFUNCTION( BIND_CLASS Global_MoveItemMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItems(Item@[]& items, Critter& toCr)", asFUNCTION( BIND_CLASS Global_MoveItemsCr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItems(Item@[]& items, Item& toCont, uint stackId)", asFUNCTION( BIND_CLASS Global_MoveItemsCont ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveItems(Item@[]& items, Map& toMap, uint16 toHx, uint16 toHy)", asFUNCTION( BIND_CLASS Global_MoveItemsMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteItem(Item& item)", asFUNCTION( BIND_CLASS Global_DeleteItem ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteItems(Item@[]& items)", asFUNCTION( BIND_CLASS Global_DeleteItems ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteNpc(Critter& npc)", asFUNCTION( BIND_CLASS Global_DeleteNpc ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetCrittersDistantion(Critter& cr1, Critter& cr2)", asFUNCTION( BIND_CLASS Global_GetCrittersDistantion ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void RadioMessage(uint16 channel, string& text)", asFUNCTION( BIND_CLASS Global_RadioMessage ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void RadioMessageMsg(uint16 channel, uint16 textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_RadioMessageMsg ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void RadioMessageMsg(uint16 channel, uint16 textMsg, uint strNum, string@+ lexems)", asFUNCTION( BIND_CLASS Global_RadioMessageMsgLex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint CreateLocation(hash locPid, uint16 worldX, uint16 worldY, Critter@[]@+ critters)", asFUNCTION( BIND_CLASS Global_CreateLocation ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteLocation(uint locId)", asFUNCTION( BIND_CLASS Global_DeleteLocation ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void GetProtoCritter(hash protoId, int[]& data)", asFUNCTION( BIND_CLASS Global_GetProtoCritter ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetCritter(uint critterId)", asFUNCTION( BIND_CLASS Global_GetCritter ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Critter@+ GetPlayer(string& name)", asFUNCTION( BIND_CLASS Global_GetPlayer ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPlayerId(string& name)", asFUNCTION( BIND_CLASS Global_GetPlayerId ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetPlayerName(uint playerId)", asFUNCTION( BIND_CLASS Global_GetPlayerName ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetGlobalMapCritters(uint16 worldX, uint16 worldY, uint radius, int findType, Critter@[]@+ critters)", asFUNCTION( BIND_CLASS Global_GetGlobalMapCritters ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint CreateTimeEvent(uint beginSecond, string& funcName, bool save)", asFUNCTION( BIND_CLASS Global_CreateTimeEventEmpty ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint CreateTimeEvent(uint beginSecond, string& funcName, uint value, bool save)", asFUNCTION( BIND_CLASS Global_CreateTimeEventValue ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint CreateTimeEvent(uint beginSecond, string& funcName, int value, bool save)", asFUNCTION( BIND_CLASS Global_CreateTimeEventValue ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint CreateTimeEvent(uint beginSecond, string& funcName, uint[]& values, bool save)", asFUNCTION( BIND_CLASS Global_CreateTimeEventValues ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint CreateTimeEvent(uint beginSecond, string& funcName, int[]& values, bool save)", asFUNCTION( BIND_CLASS Global_CreateTimeEventValues ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool EraseTimeEvent(uint num)", asFUNCTION( BIND_CLASS Global_EraseTimeEvent ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetTimeEvent(uint num, uint& duration, uint[]@+ values)", asFUNCTION( BIND_CLASS Global_GetTimeEvent ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetTimeEvent(uint num, uint& duration, int[]@+ values)", asFUNCTION( BIND_CLASS Global_GetTimeEvent ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetTimeEvent(uint num, uint duration, uint[]@+ values)", asFUNCTION( BIND_CLASS Global_SetTimeEvent ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetTimeEvent(uint num, uint duration, int[]@+ values)", asFUNCTION( BIND_CLASS Global_SetTimeEvent ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetTimeEventList(uint[]@+ nums)", asFUNCTION( BIND_CLASS Global_GetTimeEventList ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, int64[]& data)", asFUNCTION( BIND_CLASS Global_SetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, int32[]& data)", asFUNCTION( BIND_CLASS Global_SetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, int16[]& data)", asFUNCTION( BIND_CLASS Global_SetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, int8[]& data)", asFUNCTION( BIND_CLASS Global_SetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, uint64[]& data)", asFUNCTION( BIND_CLASS Global_SetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, uint32[]& data)", asFUNCTION( BIND_CLASS Global_SetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, uint16[]& data)", asFUNCTION( BIND_CLASS Global_SetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, uint8[]& data)", asFUNCTION( BIND_CLASS Global_SetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, int64[]& data, uint dataSize)", asFUNCTION( BIND_CLASS Global_SetAnyDataSize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, int32[]& data, uint dataSize)", asFUNCTION( BIND_CLASS Global_SetAnyDataSize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, int16[]& data, uint dataSize)", asFUNCTION( BIND_CLASS Global_SetAnyDataSize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, int8[]& data, uint dataSize)", asFUNCTION( BIND_CLASS Global_SetAnyDataSize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, uint64[]& data, uint dataSize)", asFUNCTION( BIND_CLASS Global_SetAnyDataSize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, uint32[]& data, uint dataSize)", asFUNCTION( BIND_CLASS Global_SetAnyDataSize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, uint16[]& data, uint dataSize)", asFUNCTION( BIND_CLASS Global_SetAnyDataSize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetAnyData(string& name, uint8[]& data, uint dataSize)", asFUNCTION( BIND_CLASS Global_SetAnyDataSize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetAnyData(string& name, int64[]& data)", asFUNCTION( BIND_CLASS Global_GetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetAnyData(string& name, int32[]& data)", asFUNCTION( BIND_CLASS Global_GetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetAnyData(string& name, int16[]& data)", asFUNCTION( BIND_CLASS Global_GetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetAnyData(string& name, int8[]& data)", asFUNCTION( BIND_CLASS Global_GetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetAnyData(string& name, uint64[]& data)", asFUNCTION( BIND_CLASS Global_GetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetAnyData(string& name, uint32[]& data)", asFUNCTION( BIND_CLASS Global_GetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetAnyData(string& name, uint16[]& data)", asFUNCTION( BIND_CLASS Global_GetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetAnyData(string& name, uint8[]& data)", asFUNCTION( BIND_CLASS Global_GetAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetAnyDataList(string[]@+ names)", asFUNCTION( BIND_CLASS Global_GetAnyDataList ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsAnyData(string& name)", asFUNCTION( BIND_CLASS Global_IsAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void EraseAnyData(string& name)", asFUNCTION( BIND_CLASS Global_EraseAnyData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Map@+ GetMap(uint mapId)", asFUNCTION( BIND_CLASS Global_GetMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Map@+ GetMapByPid(hash mapPid, uint skipCount)", asFUNCTION( BIND_CLASS Global_GetMapByPid ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Location@+ GetLocation(uint locId)", asFUNCTION( BIND_CLASS Global_GetLocation ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "Location@+ GetLocationByPid(hash locPid, uint skipCount)", asFUNCTION( BIND_CLASS Global_GetLocationByPid ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetLocations(uint16 worldX, uint16 worldY, uint radius, Location@[]@+ locations)", asFUNCTION( BIND_CLASS Global_GetLocations ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetVisibleLocations(uint16 worldX, uint16 worldY, uint radius, Critter@+ visibleBy, Location@[]@+ locations)", asFUNCTION( BIND_CLASS Global_GetVisibleLocations ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetZoneLocationIds(uint16 zoneX, uint16 zoneY, uint zoneRadius, uint[]@+ locationIds)", asFUNCTION( BIND_CLASS Global_GetZoneLocationIds ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool RunDialog(Critter& player, Critter& npc, bool ignoreDistance)", asFUNCTION( BIND_CLASS Global_RunDialogNpc ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool RunDialog(Critter& player, Critter& npc, uint dialogPack, bool ignoreDistance)", asFUNCTION( BIND_CLASS Global_RunDialogNpcDlgPack ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool RunDialog(Critter& player, uint dialogPack, uint16 hexX, uint16 hexY, bool ignoreDistance)", asFUNCTION( BIND_CLASS Global_RunDialogHex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int64 WorldItemCount(hash protoId)", asFUNCTION( BIND_CLASS Global_WorldItemCount ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetBestScore(int score, Critter@+ player, string& name)", asFUNCTION( BIND_CLASS Global_SetBestScore ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool AddTextListener(int sayType, string& firstStr, uint parameter, string& scriptName)", asFUNCTION( BIND_CLASS Global_AddTextListener ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void EraseTextListener(int sayType, string& firstStr, uint parameter)", asFUNCTION( BIND_CLASS Global_EraseTextListener ), asCALL_CDECL ) );
// BIND_ASSERT( engine->RegisterGlobalFunction( "uint8 ReverseDir(uint8 dir)", asFUNCTION( BIND_CLASS Global_ReverseDir ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "NpcPlane@ CreatePlane()", asFUNCTION( BIND_CLASS Global_CreatePlane ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetBagItems(uint bagId, hash[]@+ pids, uint[]@+ minCounts, uint[]@+ maxCounts, int[]@+ slots)", asFUNCTION( BIND_CLASS Global_GetBagItems ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetChosenSendParameter(int index, bool enabled)", asFUNCTION( BIND_CLASS Global_SetChosenSendParameter ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetSendParameter(int index, bool enabled)", asFUNCTION( BIND_CLASS Global_SetSendParameter ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetSendParameter(int index, bool enabled, string@+ allowFunc)", asFUNCTION( BIND_CLASS Global_SetSendParameterFunc ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SwapCritters(Critter& cr1, Critter& cr2, bool withInventory, bool withVars)", asFUNCTION( BIND_CLASS Global_SwapCritters ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetAllItems(hash pid, Item@[]@+ items)", asFUNCTION( BIND_CLASS Global_GetAllItems ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetAllPlayers(Critter@[]@+ players)", asFUNCTION( BIND_CLASS Global_GetAllPlayers ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetRegisteredPlayers(uint[]& ids, string@[]& names)", asFUNCTION( BIND_CLASS Global_GetRegisteredPlayers ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetAllNpc(hash pid, Critter@[]@+ npc)", asFUNCTION( BIND_CLASS Global_GetAllNpc ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetAllMaps(hash pid, Map@[]@+ maps)", asFUNCTION( BIND_CLASS Global_GetAllMaps ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetAllLocations(hash pid, Location@[]@+ locations)", asFUNCTION( BIND_CLASS Global_GetAllLocations ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "hash GetScriptId(string& scriptName, string& funcDeclaration)", asFUNCTION( BIND_CLASS Global_GetScriptId ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetScriptName(hash scriptId)", asFUNCTION( BIND_CLASS Global_GetScriptName ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadImage(uint index, string@+ imageName, uint imageDepth, int pathType)", asFUNCTION( BIND_CLASS Global_LoadImage ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetImageColor(uint index, uint x, uint y)", asFUNCTION( BIND_CLASS Global_GetImageColor ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Synchronize()", asFUNCTION( BIND_CLASS Global_Synchronize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Resynchronize()", asFUNCTION( BIND_CLASS Global_Resynchronize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetParameterDialogGetBehaviour(uint index, string& funcName)", asFUNCTION( BIND_CLASS Global_SetParameterDialogGetBehaviour ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "CraftItem@ GetCraftItem(uint num)", asFUNCTION( BIND_CLASS Global_GetCraftItem ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetTime(uint16 multiplier, uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)", asFUNCTION( BIND_CLASS Global_SetTime ), asCALL_CDECL ) );
#endif

#if defined ( BIND_CLIENT ) || defined ( BIND_MAPPER )
BIND_ASSERT( engine->RegisterObjectType( "CritterCl", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "CritterCl", asBEHAVE_ADDREF, "void f()", asMETHOD( CritterCl, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "CritterCl", asBEHAVE_RELEASE, "void f()", asMETHOD( CritterCl, Release ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectType( "ItemCl", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "ItemCl", asBEHAVE_ADDREF, "void f()", asMETHOD( Item, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "ItemCl", asBEHAVE_RELEASE, "void f()", asMETHOD( Item, Release ), asCALL_THISCALL ) );
#endif

#ifdef BIND_CLIENT
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool IsChosen() const", asFUNCTION( BIND_CLASS Crit_IsChosen ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool IsPlayer() const", asFUNCTION( BIND_CLASS Crit_IsPlayer ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool IsNpc() const", asFUNCTION( BIND_CLASS Crit_IsNpc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool IsLife() const", asFUNCTION( BIND_CLASS Crit_IsLife ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool IsKnockout() const", asFUNCTION( BIND_CLASS Crit_IsKnockout ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool IsDead() const", asFUNCTION( BIND_CLASS Crit_IsDead ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool IsFree() const", asFUNCTION( BIND_CLASS Crit_IsFree ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool IsBusy() const", asFUNCTION( BIND_CLASS Crit_IsBusy ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool IsAnim3d() const", asFUNCTION( BIND_CLASS Crit_IsAnim3d ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool IsAnimAviable(uint anim1, uint anim2) const", asFUNCTION( BIND_CLASS Crit_IsAnimAviable ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool IsAnimPlaying() const", asFUNCTION( BIND_CLASS Crit_IsAnimPlaying ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "uint GetAnim1() const", asFUNCTION( BIND_CLASS Crit_GetAnim1 ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "void Animate(uint anim1, uint anim2)", asFUNCTION( BIND_CLASS Crit_Animate ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "void Animate(uint anim1, uint anim2, ItemCl@+ item)", asFUNCTION( BIND_CLASS Crit_AnimateEx ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "void ClearAnim()", asFUNCTION( BIND_CLASS Crit_ClearAnim ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "void Wait(uint ms)", asFUNCTION( BIND_CLASS Crit_Wait ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "uint ItemsCount() const", asFUNCTION( BIND_CLASS Crit_ItemsCount ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "uint ItemsWeight() const", asFUNCTION( BIND_CLASS Crit_ItemsWeight ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "uint ItemsVolume() const", asFUNCTION( BIND_CLASS Crit_ItemsVolume ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "uint CountItem(hash protoId) const", asFUNCTION( BIND_CLASS Crit_CountItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "ItemCl@+ GetItem(hash protoId, int slot) const", asFUNCTION( BIND_CLASS Crit_GetItem ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "ItemCl@+ GetItemById(uint itemId) const", asFUNCTION( BIND_CLASS Crit_GetItemById ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "uint GetItems(int slot, ItemCl@[]@+ items) const", asFUNCTION( BIND_CLASS Crit_GetItems ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "uint GetItemsByType(int type, ItemCl@[]@+ items) const", asFUNCTION( BIND_CLASS Crit_GetItemsByType ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "ProtoItem@+ GetSlotProto(int slot, uint8& mode) const", asFUNCTION( BIND_CLASS Crit_GetSlotProto ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "void SetVisible(bool visible)", asFUNCTION( BIND_CLASS Crit_SetVisible ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool GetVisible() const", asFUNCTION( BIND_CLASS Crit_GetVisible ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "void set_ContourColor(uint value)", asFUNCTION( BIND_CLASS Crit_set_ContourColor ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "uint get_ContourColor() const", asFUNCTION( BIND_CLASS Crit_get_ContourColor ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "uint GetMultihex() const", asFUNCTION( BIND_CLASS Crit_GetMultihex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "bool IsTurnBasedTurn() const", asFUNCTION( BIND_CLASS Crit_IsTurnBasedTurn ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterCl", "void GetNameTextInfo( bool& nameVisible, int& x, int& y, int& w, int& h, int& lines )", asFUNCTION( BIND_CLASS Crit_GetNameTextInfo ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint Id", OFFSETOF( CritterCl, Id ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const hash Pid", OFFSETOF( CritterCl, Pid ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint CrType", OFFSETOF( CritterCl, BaseType ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint CrTypeAlias", OFFSETOF( CritterCl, BaseTypeAlias ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint16 HexX", OFFSETOF( CritterCl, HexX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint16 HexY", OFFSETOF( CritterCl, HexY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint8 Dir", OFFSETOF( CritterCl, CrDir ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint8 Cond", OFFSETOF( CritterCl, Cond ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint Anim1Life", OFFSETOF( CritterCl, Anim1Life ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint Anim1Knockout", OFFSETOF( CritterCl, Anim1Knockout ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint Anim1Dead", OFFSETOF( CritterCl, Anim1Dead ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint Anim2Life", OFFSETOF( CritterCl, Anim2Life ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint Anim2Knockout", OFFSETOF( CritterCl, Anim2Knockout ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint Anim2Dead", OFFSETOF( CritterCl, Anim2Dead ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const uint Flags", OFFSETOF( CritterCl, Flags ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "string@ Name", OFFSETOF( CritterCl, Name ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "string@ NameOnHead", OFFSETOF( CritterCl, NameOnHead ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "string@ Lexems", OFFSETOF( CritterCl, Lexems ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "string@ Avatar", OFFSETOF( CritterCl, Avatar ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "uint NameColor", OFFSETOF( CritterCl, NameColor ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "const int16 Ref", OFFSETOF( CritterCl, RefCounter ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "DataVal Param", OFFSETOF( CritterCl, ThisPtr[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "DataRef ParamBase", OFFSETOF( CritterCl, ThisPtr[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "CritterCl", "int[]@ Anim3dLayer", OFFSETOF( CritterCl, Layers3d ) ) );

BIND_ASSERT( engine->RegisterObjectMethod( "ItemCl", "bool IsStackable() const", asFUNCTION( BIND_CLASS Item_IsStackable ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "ItemCl", "bool IsDeteriorable() const", asFUNCTION( BIND_CLASS Item_IsDeteriorable ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "ItemCl", "uint8 GetType() const", asFUNCTION( BIND_CLASS Item_GetType ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "ItemCl", "hash GetProtoId() const", asFUNCTION( BIND_CLASS Item_GetProtoId ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "ItemCl", "bool GetMapPosition(uint16& hexX, uint16& hexY) const", asFUNCTION( BIND_CLASS Item_GetMapPosition ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "ItemCl", "void Animate(uint8 fromFrame, uint8 toFrame)", asFUNCTION( BIND_CLASS Item_Animate ), asCALL_CDECL_OBJFIRST ) );

BIND_ASSERT( engine->RegisterObjectProperty( "ItemCl", "const uint Id", OFFSETOF( Item, Id ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ItemCl", "const ProtoItem@ Proto", OFFSETOF( Item, Proto ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ItemCl", "const string@ Lexems", OFFSETOF( Item, Lexems ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ItemCl", "const uint8 Accessory", OFFSETOF( Item, Accessory ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ItemCl", "const uint MapId", OFFSETOF( Item, AccHex.MapId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ItemCl", "const uint16 HexX", OFFSETOF( Item, AccHex.HexX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ItemCl", "const uint16 HexY", OFFSETOF( Item, AccHex.HexY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ItemCl", "const uint CritId", OFFSETOF( Item, AccCritter.Id ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ItemCl", "const uint8 CritSlot", OFFSETOF( Item, AccCritter.Slot ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ItemCl", "const uint ContainerId", OFFSETOF( Item, AccContainer.ContainerId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "ItemCl", "const uint StackId", OFFSETOF( Item, AccContainer.StackId ) ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "string@ CustomCall(string& command, string& separator = \" \")", asFUNCTION( BIND_CLASS Global_CustomCall ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "CritterCl@+ GetChosen()", asFUNCTION( BIND_CLASS Global_GetChosen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetChosenActions(uint[]@+ actions)", asFUNCTION( BIND_CLASS Global_GetChosenActions ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetChosenActions(uint[]@+ actions)", asFUNCTION( BIND_CLASS Global_SetChosenActions ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "ItemCl@+ GetItem(uint itemId)", asFUNCTION( BIND_CLASS Global_GetItem ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetCrittersDistantion(CritterCl& cr1, CritterCl& cr2)", asFUNCTION( BIND_CLASS Global_GetCrittersDistantion ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "CritterCl@+ GetCritter(uint critterId)", asFUNCTION( BIND_CLASS Global_GetCritter ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetCrittersHex(uint16 hexX, uint16 hexY, uint radius, int findType, CritterCl@[]@+ critters)", asFUNCTION( BIND_CLASS Global_GetCritters ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetCritters(hash pid, int findType, CritterCl@[]@+ critters)", asFUNCTION( BIND_CLASS Global_GetCrittersByPids ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, CritterCl@[]@+ critters)", asFUNCTION( BIND_CLASS Global_GetCrittersInPath ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetCrittersPath(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, float angle, uint dist, int findType, CritterCl@[]@+ critters, uint16& preBlockHx, uint16& preBlockHy, uint16& blockHx, uint16& blockHy)", asFUNCTION( BIND_CLASS Global_GetCrittersInPathBlock ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void GetHexCoord(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist)", asFUNCTION( BIND_CLASS Global_GetHexInPath ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPathLength(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)", asFUNCTION( BIND_CLASS Global_GetPathLengthHex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPathLength(CritterCl& cr, uint16 toHx, uint16 toHy, uint cut)", asFUNCTION( BIND_CLASS Global_GetPathLengthCr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void FlushScreen(uint fromColor, uint toColor, uint timeMs)", asFUNCTION( BIND_CLASS Global_FlushScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void QuakeScreen(uint noise, uint timeMs)", asFUNCTION( BIND_CLASS Global_QuakeScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool PlaySound(string& soundName)", asFUNCTION( BIND_CLASS Global_PlaySound ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool PlaySound(uint8 soundType, uint8 soundTypeExt, uint8 soundId, uint8 soundIdExt)", asFUNCTION( BIND_CLASS Global_PlaySoundType ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool PlayMusic(string& musicName, uint pos, uint repeat)", asFUNCTION( BIND_CLASS Global_PlayMusic ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void PlayVideo(string& videoName, bool canStop)", asFUNCTION( BIND_CLASS Global_PlayVideo ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsTurnBased()", asFUNCTION( BIND_CLASS Global_IsTurnBased ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetTurnBasedTime()", asFUNCTION( BIND_CLASS Global_GetTurnBasedTime ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "hash GetCurrentMapPid()", asFUNCTION( BIND_CLASS Global_GetCurrentMapPid ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(string& text)", asFUNCTION( BIND_CLASS Global_Message ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(string& text, int type)", asFUNCTION( BIND_CLASS Global_MessageType ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_MessageMsg ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(int textMsg, uint strNum, int type)", asFUNCTION( BIND_CLASS Global_MessageMsgType ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MapMessage(string& text, uint16 hx, uint16 hy, uint timeMs, uint color, bool fade, int offsX, int offsY)", asFUNCTION( BIND_CLASS Global_MapMessage ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetMsgStr(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetMsgStr(int textMsg, uint strNum, uint skipCount)", asFUNCTION( BIND_CLASS Global_GetMsgStrSkip ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrNumUpper(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStrNumUpper ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrNumLower(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStrNumLower ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrCount(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStrCount ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsMsgStr(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_IsMsgStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ ReplaceText(const string& text, const string& replace, const string& str)", asFUNCTION( BIND_CLASS Global_ReplaceTextStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ ReplaceText(const string& text, const string& replace, int i)", asFUNCTION( BIND_CLASS Global_ReplaceTextInt ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ FormatTags(const string& text, const string@+ lexems)", asFUNCTION( BIND_CLASS Global_FormatTags ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetSomeValue(int var)", asFUNCTION( BIND_CLASS Global_GetSomeValue ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveScreen(uint16 hexX, uint16 hexY, uint speed, bool canStop = false)", asFUNCTION( BIND_CLASS Global_MoveScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void LockScreenScroll(CritterCl@+ cr, bool unlockIfSame = false)", asFUNCTION( BIND_CLASS Global_LockScreenScroll ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetFog(uint16 zoneX, uint16 zoneY)", asFUNCTION( BIND_CLASS Global_GetFog ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void RefreshItemsCollection(int collection)", asFUNCTION( BIND_CLASS Global_RefreshItemsCollection ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetDayTime(uint dayPart)", asFUNCTION( BIND_CLASS Global_GetDayTime ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void GetDayColor(uint dayPart, uint8& r, uint8& g, uint8& b)", asFUNCTION( BIND_CLASS Global_GetDayColor ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void RunServerScript(string& funcName, int p0, int p1, int p2, string@+ p3, int[]@+ p4)", asFUNCTION( BIND_CLASS Global_RunServerScript ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void RunServerScriptUnsafe(string& funcName, int p0, int p1, int p2, string@+ p3, int[]@+ p4)", asFUNCTION( BIND_CLASS Global_RunServerScriptUnsafe ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ShowScreen(int screen, dictionary@+ params = null)", asFUNCTION( BIND_CLASS Global_ShowScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void HideScreen(int screen = 0)", asFUNCTION( BIND_CLASS Global_HideScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void GetHardcodedScreenPos(int screen, int& x, int& y)", asFUNCTION( BIND_CLASS Global_GetHardcodedScreenPos ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawHardcodedScreen(int screen)", asFUNCTION( BIND_CLASS Global_DrawHardcodedScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void HandleHardcodedScreenMouse(int screen, int button, bool down, bool move)", asFUNCTION( BIND_CLASS Global_HandleHardcodedScreenMouse ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void HandleHardcodedScreenKey(int screen, uint8 key, string@ text, bool down)", asFUNCTION( BIND_CLASS Global_HandleHardcodedScreenKey ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetHexPos(uint16 hx, uint16 hy, int& x, int& y)", asFUNCTION( BIND_CLASS Global_GetHexPos ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetMonitorHex(int x, int y, uint16& hx, uint16& hy, bool ignoreInterface = false)", asFUNCTION( BIND_CLASS Global_GetMonitorHex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "ItemCl@+ GetMonitorItem(int x, int y, bool ignoreInterface = false)", asFUNCTION( BIND_CLASS Global_GetMonitorItem ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "CritterCl@+ GetMonitorCritter(int x, int y, bool ignoreInterface = false)", asFUNCTION( BIND_CLASS Global_GetMonitorCritter ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint16 GetMapWidth()", asFUNCTION( BIND_CLASS Global_GetMapWidth ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint16 GetMapHeight()", asFUNCTION( BIND_CLASS Global_GetMapHeight ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetCurrentCursor()", asFUNCTION( BIND_CLASS Global_GetCurrentCursor ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetLastCursor()", asFUNCTION( BIND_CLASS Global_GetLastCursor ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ChangeCursor(int cursor, uint contextId = 0)", asFUNCTION( BIND_CLASS Global_ChangeCursor ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveHexByDir(uint16& hexX, uint16& hexY, uint8 dir, uint steps)", asFUNCTION( BIND_CLASS Global_MoveHexByDir ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool AppendIfaceIni(string& iniName)", asFUNCTION( BIND_CLASS Global_AppendIfaceIni ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetIfaceIniStr(string& key)", asFUNCTION( BIND_CLASS Global_GetIfaceIniStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Preload3dFiles(string[]& fileNames, int pathType)", asFUNCTION( BIND_CLASS Global_Preload3dFiles ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void WaitPing()", asFUNCTION( BIND_CLASS Global_WaitPing ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadFont(int font, string& fontFileName)", asFUNCTION( BIND_CLASS Global_LoadFont ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetDefaultFont(int font, uint color)", asFUNCTION( BIND_CLASS Global_SetDefaultFont ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetEffect(int effectType, int effectSubtype, string@+ effectName, string@+ effectDefines = null)", asFUNCTION( BIND_CLASS Global_SetEffect ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void RefreshMap(bool onlyTiles, bool onlyRoof, bool onlyLight)", asFUNCTION( BIND_CLASS Global_RefreshMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MouseClick(int x, int y, int button, int cursor)", asFUNCTION( BIND_CLASS Global_MouseClick ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void KeyboardPress(uint8 key1, uint8 key2, string@+ key1Text = null, string@+ key2Text = null)", asFUNCTION( BIND_CLASS Global_KeyboardPress ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetRainAnimation(string@+ fallAnimName, string@+ dropAnimName)", asFUNCTION( BIND_CLASS Global_SetRainAnimation ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ChangeZoom(float targetZoom)", asFUNCTION( BIND_CLASS Global_ChangeZoom ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SaveScreenshot(string& filePath)", asFUNCTION( BIND_CLASS Global_SaveScreenshot ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SaveText(string& filePath, string& text)", asFUNCTION( BIND_CLASS Global_SaveText ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetCacheData(const string& name, const uint8[]& data)", asFUNCTION( BIND_CLASS Global_SetCacheData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetCacheData(const string& name, const uint8[]& data, uint dataSize)", asFUNCTION( BIND_CLASS Global_SetCacheDataSize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetCacheData(const string& name, uint8[]& data)", asFUNCTION( BIND_CLASS Global_GetCacheData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetCacheDataStr(const string& name, const string& data)", asFUNCTION( BIND_CLASS Global_SetCacheDataStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetCacheDataStr(const string& name)", asFUNCTION( BIND_CLASS Global_GetCacheDataStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCacheData(const string& name)", asFUNCTION( BIND_CLASS Global_IsCacheData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void EraseCacheData(const string& name)", asFUNCTION( BIND_CLASS Global_EraseCacheData ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetUserConfig(string[]& keyValues)", asFUNCTION( BIND_CLASS Global_SetUserConfig ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalProperty( "bool __GmapActive", &BIND_CLASS GmapActive ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __GmapWait", &BIND_CLASS GmapWait ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __GmapZoom", &BIND_CLASS GmapZoom ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __GmapOffsetX", &BIND_CLASS GmapOffsetX ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __GmapOffsetY", &BIND_CLASS GmapOffsetY ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __GmapGroupCurX", &BIND_CLASS GmapGroupCurX ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __GmapGroupCurY", &BIND_CLASS GmapGroupCurY ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __GmapGroupToX", &BIND_CLASS GmapGroupToX ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __GmapGroupToY", &BIND_CLASS GmapGroupToY ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "float __GmapGroupSpeed", &BIND_CLASS GmapGroupSpeed ) );
#endif

#if defined ( BIND_CLIENT ) || defined ( BIND_SERVER )
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetFullSecond(uint16 year, uint16 month, uint16 day, uint16 hour, uint16 minute, uint16 second)", asFUNCTION( BIND_CLASS Global_GetFullSecond ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void GetTime(uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second, uint16& milliseconds)", asFUNCTION( BIND_CLASS Global_GetTime ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void GetGameTime(uint fullSecond, uint16& year, uint16& month, uint16& day, uint16& dayOfWeek, uint16& hour, uint16& minute, uint16& second)", asFUNCTION( BIND_CLASS Global_GetGameTime ), asCALL_CDECL ) );
// BIND_ASSERT( engine->RegisterGlobalFunction( "void GetVersion(uint& server, uint& client, uint& net)", asFUNCTION( BIND_CLASS Global_GetVersion ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetPropertyGetCallback(const string& className, const string& propertyName, const string& scriptFunc)", asFUNCTION( BIND_CLASS Global_SetPropertyGetCallback ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool AddPropertySetCallback(const string& className, const string& propertyName, const string& scriptFunc)", asFUNCTION( BIND_CLASS Global_AddPropertySetCallback ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetParameterGetBehaviour(uint index, string& funcName)", asFUNCTION( BIND_CLASS Global_SetParameterGetBehaviour ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SetParameterChangeBehaviour(uint index, string& funcName)", asFUNCTION( BIND_CLASS Global_SetParameterChangeBehaviour ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetRegistrationParameter(uint index, bool enabled)", asFUNCTION( BIND_CLASS Global_SetRegistrationParam ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCritterCanWalk(uint crType)", asFUNCTION( BIND_CLASS Global_IsCritterCanWalk ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCritterCanRun(uint crType)", asFUNCTION( BIND_CLASS Global_IsCritterCanRun ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCritterCanRotate(uint crType)", asFUNCTION( BIND_CLASS Global_IsCritterCanRotate ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCritterCanAim(uint crType)", asFUNCTION( BIND_CLASS Global_IsCritterCanAim ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCritterCanArmor(uint crType)", asFUNCTION( BIND_CLASS Global_IsCritterCanArmor ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCritterAnim1(uint crType, uint anim1)", asFUNCTION( BIND_CLASS Global_IsCritterAnim1 ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetCritterAnimType(uint crType)", asFUNCTION( BIND_CLASS Global_GetCritterAnimType ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetCritterAlias(uint crType)", asFUNCTION( BIND_CLASS Global_GetCritterAlias ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetCritterTypeName(uint crType)", asFUNCTION( BIND_CLASS Global_GetCritterTypeName ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetCritterSoundName(uint crType)", asFUNCTION( BIND_CLASS Global_GetCritterSoundName ), asCALL_CDECL ) );

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
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __FixBoyDefaultExperience", &GameOpt.FixBoyDefaultExperience ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __SneakDivider", &GameOpt.SneakDivider ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LevelCap", &GameOpt.LevelCap ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __LevelCapAddExperience", &GameOpt.LevelCapAddExperience ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookNormal", &GameOpt.LookNormal ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookMinimum", &GameOpt.LookMinimum ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __GlobalMapMaxGroupCount", &GameOpt.GlobalMapMaxGroupCount ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __CritterIdleTick", &GameOpt.CritterIdleTick ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TurnBasedTick", &GameOpt.TurnBasedTick ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __DeadHitPoints", &GameOpt.DeadHitPoints ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Breaktime", &GameOpt.Breaktime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TimeoutTransfer", &GameOpt.TimeoutTransfer ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TimeoutBattle", &GameOpt.TimeoutBattle ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ApRegeneration", &GameOpt.ApRegeneration ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __RtApCostCritterWalk", &GameOpt.RtApCostCritterWalk ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __RtApCostCritterRun", &GameOpt.RtApCostCritterRun ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __RtApCostMoveItemContainer", &GameOpt.RtApCostMoveItemContainer ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __RtApCostMoveItemInventory", &GameOpt.RtApCostMoveItemInventory ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __RtApCostPickItem", &GameOpt.RtApCostPickItem ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __RtApCostDropItem", &GameOpt.RtApCostDropItem ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __RtApCostReloadWeapon", &GameOpt.RtApCostReloadWeapon ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __RtApCostPickCritter", &GameOpt.RtApCostPickCritter ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __RtApCostUseItem", &GameOpt.RtApCostUseItem ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __RtApCostUseSkill", &GameOpt.RtApCostUseSkill ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __RtAlwaysRun", &GameOpt.RtAlwaysRun ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TbApCostCritterMove", &GameOpt.TbApCostCritterMove ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TbApCostMoveItemContainer", &GameOpt.TbApCostMoveItemContainer ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TbApCostMoveItemInventory", &GameOpt.TbApCostMoveItemInventory ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TbApCostPickItem", &GameOpt.TbApCostPickItem ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TbApCostDropItem", &GameOpt.TbApCostDropItem ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TbApCostReloadWeapon", &GameOpt.TbApCostReloadWeapon ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TbApCostPickCritter", &GameOpt.TbApCostPickCritter ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TbApCostUseItem", &GameOpt.TbApCostUseItem ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TbApCostUseSkill", &GameOpt.TbApCostUseSkill ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ApCostAimEyes", &GameOpt.ApCostAimEyes ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ApCostAimHead", &GameOpt.ApCostAimHead ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ApCostAimGroin", &GameOpt.ApCostAimGroin ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ApCostAimTorso", &GameOpt.ApCostAimTorso ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ApCostAimArms", &GameOpt.ApCostAimArms ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ApCostAimLegs", &GameOpt.ApCostAimLegs ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __TbAlwaysRun", &GameOpt.TbAlwaysRun ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __RunOnCombat", &GameOpt.RunOnCombat ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __RunOnTransfer", &GameOpt.RunOnTransfer ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __RunOnTurnBased", &GameOpt.RunOnTurnBased ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __GlobalMapWidth", &GameOpt.GlobalMapWidth ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __GlobalMapHeight", &GameOpt.GlobalMapHeight ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __GlobalMapZoneLength", &GameOpt.GlobalMapZoneLength ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __GlobalMapMoveTime", &GameOpt.GlobalMapMoveTime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __BagRefreshTime", &GameOpt.BagRefreshTime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __AttackAnimationsMinDist", &GameOpt.AttackAnimationsMinDist ) );
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
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __LookWeight", &GameOpt.LookWeight ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __CustomItemCost", &GameOpt.CustomItemCost ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __RegistrationTimeout", &GameOpt.RegistrationTimeout ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __AccountPlayTime", &GameOpt.AccountPlayTime ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __LoggingVars", &GameOpt.LoggingVars ) );
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
BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __CommandLine", &GameOpt.CommandLine ) );

# ifdef BIND_SERVER
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __GenerateWorldDisabled", &GameOpt.GenerateWorldDisabled ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __BuildMapperScripts", &GameOpt.BuildMapperScripts ) );
# endif

BIND_ASSERT( engine->RegisterGlobalProperty( "int __StartSpecialPoints", &GameOpt.StartSpecialPoints ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __StartTagSkillPoints", &GameOpt.StartTagSkillPoints ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __SkillMaxValue", &GameOpt.SkillMaxValue ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __SkillModAdd2", &GameOpt.SkillModAdd2 ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __SkillModAdd3", &GameOpt.SkillModAdd3 ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __SkillModAdd4", &GameOpt.SkillModAdd4 ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __SkillModAdd5", &GameOpt.SkillModAdd5 ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __SkillModAdd6", &GameOpt.SkillModAdd6 ) );

BIND_ASSERT( engine->RegisterGlobalProperty( "bool __AbsoluteOffsets", &GameOpt.AbsoluteOffsets ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __SkillBegin", &GameOpt.SkillBegin ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __SkillEnd", &GameOpt.SkillEnd ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TimeoutBegin", &GameOpt.TimeoutBegin ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TimeoutEnd", &GameOpt.TimeoutEnd ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __KillBegin", &GameOpt.KillBegin ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __KillEnd", &GameOpt.KillEnd ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __PerkBegin", &GameOpt.PerkBegin ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __PerkEnd", &GameOpt.PerkEnd ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __AddictionBegin", &GameOpt.AddictionBegin ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __AddictionEnd", &GameOpt.AddictionEnd ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __KarmaBegin", &GameOpt.KarmaBegin ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __KarmaEnd", &GameOpt.KarmaEnd ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __DamageBegin", &GameOpt.DamageBegin ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __DamageEnd", &GameOpt.DamageEnd ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TraitBegin", &GameOpt.TraitBegin ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TraitEnd", &GameOpt.TraitEnd ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ReputationBegin", &GameOpt.ReputationBegin ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ReputationEnd", &GameOpt.ReputationEnd ) );

BIND_ASSERT( engine->RegisterGlobalProperty( "int __ReputationLoved", &GameOpt.ReputationLoved ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __ReputationLiked", &GameOpt.ReputationLiked ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __ReputationAccepted", &GameOpt.ReputationAccepted ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __ReputationNeutral", &GameOpt.ReputationNeutral ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __ReputationAntipathy", &GameOpt.ReputationAntipathy ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __ReputationHated", &GameOpt.ReputationHated ) );
#endif

#ifdef BIND_MAPPER
BIND_ASSERT( engine->RegisterObjectType( "MapperObject", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "MapperObject", asBEHAVE_ADDREF, "void f()", asMETHOD( MapObject, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "MapperObject", asBEHAVE_RELEASE, "void f()", asMETHOD( MapObject, Release ), asCALL_THISCALL ) );

BIND_ASSERT( engine->RegisterObjectType( "MapperMap", 0, asOBJ_REF ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "MapperMap", asBEHAVE_ADDREF, "void f()", asMETHOD( ProtoMap, AddRef ), asCALL_THISCALL ) );
BIND_ASSERT( engine->RegisterObjectBehaviour( "MapperMap", asBEHAVE_RELEASE, "void f()", asMETHOD( ProtoMap, Release ), asCALL_THISCALL ) );

// MapperObject
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "void Update() const", asFUNCTION( BIND_CLASS MapperObject_Update ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "MapperObject@+ AddChild(hash pid)", asFUNCTION( BIND_CLASS MapperObject_AddChild ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "uint GetChilds(MapperObject@[]@+ objects) const", asFUNCTION( BIND_CLASS MapperObject_GetChilds ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "string@ get_ScriptName() const", asFUNCTION( BIND_CLASS MapperObject_get_ScriptName ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "void set_ScriptName(const string& name)", asFUNCTION( BIND_CLASS MapperObject_set_ScriptName ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "string@ get_FuncName() const", asFUNCTION( BIND_CLASS MapperObject_get_FuncName ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "void set_FuncName(const string& name)", asFUNCTION( BIND_CLASS MapperObject_set_FuncName ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "uint8 get_Critter_Cond() const", asFUNCTION( BIND_CLASS MapperObject_get_Critter_Cond ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "void set_Critter_Cond(uint8 value)", asFUNCTION( BIND_CLASS MapperObject_set_Critter_Cond ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "void MoveToHex(uint16 hexX, uint16 hexY)", asFUNCTION( BIND_CLASS MapperObject_MoveToHex ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "void MoveToHexOffset(int x, int y)", asFUNCTION( BIND_CLASS MapperObject_MoveToHexOffset ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "void MoveToDir(uint8 dir)", asFUNCTION( BIND_CLASS MapperObject_MoveToDir ), asCALL_CDECL_OBJFIRST ) );

// Generic
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "const uint8 MapObjType", OFFSETOF( MapObject, MapObjType ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "const hash ProtoId", OFFSETOF( MapObject, ProtoId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "const uint16 MapX", OFFSETOF( MapObject, MapX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "const uint16 MapY", OFFSETOF( MapObject, MapY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "const uint UID", OFFSETOF( MapObject, UID ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "const uint ContainerUID", OFFSETOF( MapObject, ContainerUID ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "const uint ParentUID", OFFSETOF( MapObject, ParentUID ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "const uint ParentChildIndex", OFFSETOF( MapObject, ParentChildIndex ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint LightColor", OFFSETOF( MapObject, LightColor ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 LightDay", OFFSETOF( MapObject, LightDay ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 LightDirOff", OFFSETOF( MapObject, LightDirOff ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 LightDistance", OFFSETOF( MapObject, LightDistance ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int8 LightIntensity", OFFSETOF( MapObject, LightIntensity ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int UserData0", OFFSETOF( MapObject, UserData[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int UserData1", OFFSETOF( MapObject, UserData[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int UserData2", OFFSETOF( MapObject, UserData[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int UserData3", OFFSETOF( MapObject, UserData[ 3 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int UserData4", OFFSETOF( MapObject, UserData[ 4 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int UserData5", OFFSETOF( MapObject, UserData[ 5 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int UserData6", OFFSETOF( MapObject, UserData[ 6 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int UserData7", OFFSETOF( MapObject, UserData[ 7 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int UserData8", OFFSETOF( MapObject, UserData[ 8 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int UserData9", OFFSETOF( MapObject, UserData[ 9 ] ) ) );

// Critter
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 Critter_Dir", OFFSETOF( MapObject, MCritter.Dir ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 Critter_Cond", OFFSETOF( MapObject, MCritter.Cond ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 Critter_Anim1", OFFSETOF( MapObject, MCritter.Anim1 ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 Critter_Anim2", OFFSETOF( MapObject, MCritter.Anim2 ) ) );

// Critter parameters
BIND_ASSERT( engine->RegisterObjectType( "CritterParam", 0, asOBJ_REF | asOBJ_NOHANDLE ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterParam", "const string& opIndex(uint) const", asFUNCTION( BIND_CLASS MapperObject_CritterParam_Index ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "CritterParam", "string& opIndex(uint)", asFUNCTION( BIND_CLASS MapperObject_CritterParam_Index ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "CritterParam Critter_Param", 0 ) );

// Item/critter shared parameters
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int16 OffsetX", OFFSETOF( MapObject, MItem.OffsetX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int16 OffsetY", OFFSETOF( MapObject, MItem.OffsetY ) ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "string@ get_PicMap() const", asFUNCTION( BIND_CLASS MapperObject_get_PicMap ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "void set_PicMap(const string& name)", asFUNCTION( BIND_CLASS MapperObject_set_PicMap ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "string@ get_PicInv() const", asFUNCTION( BIND_CLASS MapperObject_get_PicInv ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperObject", "void set_PicInv(const string& name)", asFUNCTION( BIND_CLASS MapperObject_set_PicInv ), asCALL_CDECL_OBJFIRST ) );

// Item
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint Item_Count", OFFSETOF( MapObject, MItem.Count ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 Item_BrokenFlags", OFFSETOF( MapObject, MItem.BrokenFlags ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 Item_BrokenCount", OFFSETOF( MapObject, MItem.BrokenCount ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint16 Item_Deterioration", OFFSETOF( MapObject, MItem.Deterioration ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 Item_ItemSlot", OFFSETOF( MapObject, MItem.ItemSlot ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "string@ Item_AmmoPid", OFFSETOF( MapObject, MItem.AmmoPid ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint Item_AmmoCount", OFFSETOF( MapObject, MItem.AmmoCount ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint Item_LockerDoorId", OFFSETOF( MapObject, MItem.LockerDoorId ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint16 Item_LockerCondition", OFFSETOF( MapObject, MItem.LockerCondition ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint16 Item_LockerComplexity", OFFSETOF( MapObject, MItem.LockerComplexity ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int16 Item_TrapValue", OFFSETOF( MapObject, MItem.TrapValue ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Item_Val0", OFFSETOF( MapObject, MItem.Val[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Item_Val1", OFFSETOF( MapObject, MItem.Val[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Item_Val2", OFFSETOF( MapObject, MItem.Val[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Item_Val3", OFFSETOF( MapObject, MItem.Val[ 3 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Item_Val4", OFFSETOF( MapObject, MItem.Val[ 4 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Item_Val5", OFFSETOF( MapObject, MItem.Val[ 5 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Item_Val6", OFFSETOF( MapObject, MItem.Val[ 6 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Item_Val7", OFFSETOF( MapObject, MItem.Val[ 7 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Item_Val8", OFFSETOF( MapObject, MItem.Val[ 8 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Item_Val9", OFFSETOF( MapObject, MItem.Val[ 9 ] ) ) );

// Scenery
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "bool Scenery_CanUse", OFFSETOF( MapObject, MScenery.CanUse ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "bool Scenery_CanTalk", OFFSETOF( MapObject, MScenery.CanTalk ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint Scenery_TriggerNum", OFFSETOF( MapObject, MScenery.TriggerNum ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 Scenery_ParamsCount", OFFSETOF( MapObject, MScenery.ParamsCount ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Scenery_Param0", OFFSETOF( MapObject, MScenery.Param[ 0 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Scenery_Param1", OFFSETOF( MapObject, MScenery.Param[ 1 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Scenery_Param2", OFFSETOF( MapObject, MScenery.Param[ 2 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Scenery_Param3", OFFSETOF( MapObject, MScenery.Param[ 3 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "int Scenery_Param4", OFFSETOF( MapObject, MScenery.Param[ 4 ] ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "string@ Scenery_ToMap", OFFSETOF( MapObject, MScenery.ToMap ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint Scenery_ToEntire", OFFSETOF( MapObject, MScenery.ToEntire ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 Scenery_ToDir", OFFSETOF( MapObject, MScenery.ToDir ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperObject", "uint8 Scenery_SpriteCut", OFFSETOF( MapObject, MScenery.SpriteCut ) ) );

// MapperMap
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "MapperObject@+ AddObject(uint16 hexX, uint16 hexY, int mapObjType, hash pid)", asFUNCTION( BIND_CLASS MapperMap_AddObject ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "MapperObject@+ GetObject(uint16 hexX, uint16 hexY, int mapObjType, hash pid, uint skip) const", asFUNCTION( BIND_CLASS MapperMap_GetObject ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "uint GetObjects(uint16 hexX, uint16 hexY, uint radius, int mapObjType, hash pid, MapperObject@[]@+ objects) const", asFUNCTION( BIND_CLASS MapperMap_GetObjects ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "void UpdateObjects() const", asFUNCTION( BIND_CLASS MapperMap_UpdateObjects ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "void Resize(uint16 width, uint16 height)", asFUNCTION( BIND_CLASS MapperMap_Resize ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "uint GetTilesCount(uint16 hexX, uint16 hexY, bool roof) const", asFUNCTION( BIND_CLASS MapperMap_GetTilesCount ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "void DeleteTile(uint16 hexX, uint16 hexY, bool roof, int layer)", asFUNCTION( BIND_CLASS MapperMap_DeleteTile ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "hash GetTile(uint16 hexX, uint16 hexY, bool roof, int layer) const", asFUNCTION( BIND_CLASS MapperMap_GetTileHash ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "void AddTile(uint16 hexX, uint16 hexY, int offsX, int offsY, int layer, bool roof, hash picHash)", asFUNCTION( BIND_CLASS MapperMap_AddTileHash ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "string@ GetTileName(uint16 hexX, uint16 hexY, bool roof, int layer) const", asFUNCTION( BIND_CLASS MapperMap_GetTileName ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "void AddTileName(uint16 hexX, uint16 hexY, int offsX, int offsY, int layer, bool roof, string@+ picName)", asFUNCTION( BIND_CLASS MapperMap_AddTileName ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "uint GetDayTime(uint dayPart) const", asFUNCTION( BIND_CLASS MapperMap_GetDayTime ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "void SetDayTime(uint dayPart, uint time)", asFUNCTION( BIND_CLASS MapperMap_SetDayTime ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "void GetDayColor(uint dayPart, uint8& r, uint8& g, uint8& b) const", asFUNCTION( BIND_CLASS MapperMap_GetDayColor ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "void SetDayColor(uint dayPart, uint8 r, uint8 g, uint8 b)", asFUNCTION( BIND_CLASS MapperMap_SetDayColor ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperMap", "const uint16 Width", OFFSETOF( ProtoMap, Header.MaxHexX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperMap", "const uint16 Height", OFFSETOF( ProtoMap, Header.MaxHexY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperMap", "const int WorkHexX", OFFSETOF( ProtoMap, Header.WorkHexX ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperMap", "const int WorkHexY", OFFSETOF( ProtoMap, Header.WorkHexY ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperMap", "int Time", OFFSETOF( ProtoMap, Header.Time ) ) );
BIND_ASSERT( engine->RegisterObjectProperty( "MapperMap", "bool NoLogOut", OFFSETOF( ProtoMap, Header.NoLogOut ) ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "string@ get_Name()", asFUNCTION( BIND_CLASS MapperMap_get_Name ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "string@ get_ScriptModule() const", asFUNCTION( BIND_CLASS MapperMap_get_ScriptModule ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "void set_ScriptModule(const string& name)", asFUNCTION( BIND_CLASS MapperMap_set_ScriptModule ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "string@ get_ScriptFunc() const", asFUNCTION( BIND_CLASS MapperMap_get_ScriptFunc ), asCALL_CDECL_OBJFIRST ) );
BIND_ASSERT( engine->RegisterObjectMethod( "MapperMap", "void set_ScriptFunc(const string& name)", asFUNCTION( BIND_CLASS MapperMap_set_ScriptFunc ), asCALL_CDECL_OBJFIRST ) );

// Global
BIND_ASSERT( engine->RegisterGlobalFunction( "void ShowCritterParam(int paramIndex, bool show, string@+ paramName = null)", asFUNCTION( BIND_CLASS Global_ShowCritterParam ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "MapperMap@+ LoadMap(string& fileName, int pathType)", asFUNCTION( BIND_CLASS Global_LoadMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void UnloadMap(MapperMap@+ map)", asFUNCTION( BIND_CLASS Global_UnloadMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool SaveMap(MapperMap@+ map, string@+ customName = null)", asFUNCTION( BIND_CLASS Global_SaveMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool ShowMap(MapperMap@+ map)", asFUNCTION( BIND_CLASS Global_ShowMap ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetLoadedMaps(MapperMap@[]@+ maps)", asFUNCTION( BIND_CLASS Global_GetLoadedMaps ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMapFileNames(string@+ dir, string@[]@+ names)", asFUNCTION( BIND_CLASS Global_GetMapFileNames ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteObject(MapperObject@+ obj)", asFUNCTION( BIND_CLASS Global_DeleteObject ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DeleteObjects(MapperObject@[]& objects)", asFUNCTION( BIND_CLASS Global_DeleteObjects ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SelectObject(MapperObject@+ obj, bool set)", asFUNCTION( BIND_CLASS Global_SelectObject ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SelectObjects(MapperObject@[]& objects, bool set)", asFUNCTION( BIND_CLASS Global_SelectObjects ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "MapperObject@+ GetSelectedObject()", asFUNCTION( BIND_CLASS Global_GetSelectedObject ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetSelectedObjects(MapperObject@[]@+ objects)", asFUNCTION( BIND_CLASS Global_GetSelectedObjects ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "uint TabGetTileDirs(int tab, string@[]@+ dirNames, bool[]@+ includeSubdirs)", asFUNCTION( BIND_CLASS Global_TabGetTileDirs ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint TabGetItemPids(int tab, string@+ subTab, hash[]@+ itemPids)", asFUNCTION( BIND_CLASS Global_TabGetItemPids ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint TabGetCritterPids(int tab, string@+ subTab, hash[]@+ critterPids)", asFUNCTION( BIND_CLASS Global_TabGetCritterPids ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSetTileDirs(int tab, string@[]@+ dirNames, bool[]@+ includeSubdirs)", asFUNCTION( BIND_CLASS Global_TabSetTileDirs ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSetItemPids(int tab, string@+ subTab, hash[]@+ itemPids)", asFUNCTION( BIND_CLASS Global_TabSetItemPids ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSetCritterPids(int tab, string@+ subTab, hash[]@+ critterPids)", asFUNCTION( BIND_CLASS Global_TabSetCritterPids ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void TabDelete(int tab)", asFUNCTION( BIND_CLASS Global_TabDelete ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSelect(int tab, string@+ subTab, bool show = false)", asFUNCTION( BIND_CLASS Global_TabSelect ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void TabSetName(int tab, string@+ tabName)", asFUNCTION( BIND_CLASS Global_TabSetName ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCritterCanWalk(uint crType)", asFUNCTION( BIND_CLASS Global_IsCritterCanWalk ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCritterCanRun(uint crType)", asFUNCTION( BIND_CLASS Global_IsCritterCanRun ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCritterCanRotate(uint crType)", asFUNCTION( BIND_CLASS Global_IsCritterCanRotate ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCritterCanAim(uint crType)", asFUNCTION( BIND_CLASS Global_IsCritterCanAim ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCritterCanArmor(uint crType)", asFUNCTION( BIND_CLASS Global_IsCritterCanArmor ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsCritterAnim1(uint crType, uint anim1)", asFUNCTION( BIND_CLASS Global_IsCritterAnim1 ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetCritterAnimType(uint crType)", asFUNCTION( BIND_CLASS Global_GetCritterAnimType ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetCritterAlias(uint crType)", asFUNCTION( BIND_CLASS Global_GetCritterAlias ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetCritterTypeName(uint crType)", asFUNCTION( BIND_CLASS Global_GetCritterTypeName ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetCritterSoundName(uint crType)", asFUNCTION( BIND_CLASS Global_GetCritterSoundName ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "void GetHexCoord(uint16 fromHx, uint16 fromHy, uint16& toHx, uint16& toHy, float angle, uint dist)", asFUNCTION( BIND_CLASS Global_GetHexInPath ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPathLength(uint16 fromHx, uint16 fromHy, uint16 toHx, uint16 toHy, uint cut)", asFUNCTION( BIND_CLASS Global_GetPathLengthHex ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(string& text)", asFUNCTION( BIND_CLASS Global_Message ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Message(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_MessageMsg ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "void MapMessage(string& text, uint16 hx, uint16 hy, uint timeMs, uint color, bool fade, int offsX, int offsY)", asFUNCTION( BIND_CLASS Global_MapMessage ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetMsgStr(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetMsgStr(int textMsg, uint strNum, uint skipCount)", asFUNCTION( BIND_CLASS Global_GetMsgStrSkip ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrNumUpper(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStrNumUpper ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrNumLower(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStrNumLower ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetMsgStrCount(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_GetMsgStrCount ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool IsMsgStr(int textMsg, uint strNum)", asFUNCTION( BIND_CLASS Global_IsMsgStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ ReplaceText(const string& text, const string& replace, const string& str)", asFUNCTION( BIND_CLASS Global_ReplaceTextStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ ReplaceText(const string& text, const string& replace, int i)", asFUNCTION( BIND_CLASS Global_ReplaceTextInt ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveScreen(uint16 hexX, uint16 hexY, uint speed, bool canStop = false)", asFUNCTION( BIND_CLASS Global_MoveScreen ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetHexPos(uint16 hx, uint16 hy, int& x, int& y)", asFUNCTION( BIND_CLASS Global_GetHexPos ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool GetMonitorHex(int x, int y, uint16& hx, uint16& hy, bool ignoreInterface = false)", asFUNCTION( BIND_CLASS Global_GetMonitorHex ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "MapperObject@+ GetMonitorObject(int x, int y, bool ignoreInterface = false)", asFUNCTION( BIND_CLASS Global_GetMonitorObject ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MoveHexByDir(uint16& hexX, uint16& hexY, uint8 dir, uint steps)", asFUNCTION( BIND_CLASS Global_MoveHexByDir ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetIfaceIniStr(string& key)", asFUNCTION( BIND_CLASS Global_GetIfaceIniStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadFont(int font, string& fontFileName)", asFUNCTION( BIND_CLASS Global_LoadFont ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetDefaultFont(int font, uint color)", asFUNCTION( BIND_CLASS Global_SetDefaultFont ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void MouseClick(int x, int y, int button, int cursor)", asFUNCTION( BIND_CLASS Global_MouseClick ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void KeyboardPress(uint8 key1, uint8 key2, string@+ key1Text = null, string@+ key2Text = null)", asFUNCTION( BIND_CLASS Global_KeyboardPress ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetRainAnimation(string@+ fallAnimName, string@+ dropAnimName)", asFUNCTION( BIND_CLASS Global_SetRainAnimation ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void ChangeZoom(float targetZoom)", asFUNCTION( BIND_CLASS Global_ChangeZoom ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __ClientPath", &GameOpt.ClientPath ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __ServerPath", &GameOpt.ServerPath ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowCorners", &GameOpt.ShowCorners ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowSpriteCuts", &GameOpt.ShowSpriteCuts ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __ShowDrawOrder", &GameOpt.ShowDrawOrder ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __SplitTilesCollection", &GameOpt.SplitTilesCollection ) );
#endif

#if defined ( BIND_CLIENT ) || defined ( BIND_MAPPER )
BIND_ASSERT( engine->RegisterGlobalFunction( "uint LoadSprite(string& name, int pathIndex)", asFUNCTION( BIND_CLASS Global_LoadSprite ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint LoadSprite(hash name)", asFUNCTION( BIND_CLASS Global_LoadSpriteHash ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetSpriteWidth(uint sprId, int frameIndex)", asFUNCTION( BIND_CLASS Global_GetSpriteWidth ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetSpriteHeight(uint sprId, int frameIndex)", asFUNCTION( BIND_CLASS Global_GetSpriteHeight ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetSpriteCount(uint sprId)", asFUNCTION( BIND_CLASS Global_GetSpriteCount ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetSpriteTicks(uint sprId)", asFUNCTION( BIND_CLASS Global_GetSpriteTicks ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetPixelColor(uint sprId, int frameIndex, int x, int y)", asFUNCTION( BIND_CLASS Global_GetPixelColor ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void GetTextInfo(string@+ text, int w, int h, int font, int flags, int& tw, int& th, int& lines)", asFUNCTION( BIND_CLASS Global_GetTextInfo ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawSprite(uint sprId, int frameIndex, int x, int y, uint color = 0, bool applyOffsets = false)", asFUNCTION( BIND_CLASS Global_DrawSprite ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawSprite(uint sprId, int frameIndex, int x, int y, int w, int h, bool zoom = false, uint color = 0, bool applyOffsets = false)", asFUNCTION( BIND_CLASS Global_DrawSpriteSize ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawSpritePattern(uint sprId, int frameIndex, int x, int y, int w, int h, int sprWidth, int sprHeight, uint color = 0)", asFUNCTION( BIND_CLASS Global_DrawSpritePattern ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawText(string& text, int x, int y, int w, int h, uint color, int font, int flags)", asFUNCTION( BIND_CLASS Global_DrawText ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawPrimitive(int primitiveType, int[]& data)", asFUNCTION( BIND_CLASS Global_DrawPrimitive ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawMapSprite(uint16 hx, uint16 hy, hash effectPid, uint sprId, int frameIndex, int offsX, int offsY)", asFUNCTION( BIND_CLASS Global_DrawMapSprite ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawCritter2d(uint crType, uint anim1, uint anim2, uint8 dir, int l, int t, int r, int b, bool scratch, bool center, uint color)", asFUNCTION( BIND_CLASS Global_DrawCritter2d ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void DrawCritter3d(uint instance, uint crType, uint anim1, uint anim2, int[]@+ layers, float[]@+ position, uint color)", asFUNCTION( BIND_CLASS Global_DrawCritter3d ), asCALL_CDECL ) );

BIND_ASSERT( engine->RegisterGlobalProperty( "bool __Quit", &GameOpt.Quit ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __OpenGLRendering", &GameOpt.OpenGLRendering ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __OpenGLDebug", &GameOpt.OpenGLDebug ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __AssimpLogging", &GameOpt.AssimpLogging ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MouseX", &GameOpt.MouseX ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MouseY", &GameOpt.MouseY ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint8 __RoofAlpha", &GameOpt.RoofAlpha ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __HideCursor", &GameOpt.HideCursor ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const int __ScreenWidth", &GameOpt.ScreenWidth ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "const int __ScreenHeight", &GameOpt.ScreenHeight ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __MultiSampling", &GameOpt.MultiSampling ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __DisableLMenu", &GameOpt.DisableLMenu ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __DisableMouseEvents", &GameOpt.DisableMouseEvents ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __DisableKeyboardEvents", &GameOpt.DisableKeyboardEvents ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __HidePassword", &GameOpt.HidePassword ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __PlayerOffAppendix", &GameOpt.PlayerOffAppendix ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __DamageHitDelay", &GameOpt.DamageHitDelay ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __CombatMessagesType", &GameOpt.CombatMessagesType ) );
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
BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __Host", &GameOpt.Host ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Port", &GameOpt.Port ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __UpdateServerHost", &GameOpt.UpdateServerHost ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __UpdateServerPort", &GameOpt.UpdateServerPort ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ProxyType", &GameOpt.ProxyType ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __ProxyHost", &GameOpt.ProxyHost ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __ProxyPort", &GameOpt.ProxyPort ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __ProxyUser", &GameOpt.ProxyUser ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __ProxyPass", &GameOpt.ProxyPass ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __Name", &GameOpt.Name ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __TextDelay", &GameOpt.TextDelay ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __AlwaysOnTop", &GameOpt.AlwaysOnTop ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __FixedFPS", &GameOpt.FixedFPS ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __FPS", &GameOpt.FPS ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __PingPeriod", &GameOpt.PingPeriod ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint __Ping", &GameOpt.Ping ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __MsgboxInvert", &GameOpt.MsgboxInvert ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "uint8 __DefaultCombatMode", &GameOpt.DefaultCombatMode ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __MessNotify", &GameOpt.MessNotify ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "bool __SoundNotify", &GameOpt.SoundNotify ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "int __IndicatorType", &GameOpt.IndicatorType ) );
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
BIND_ASSERT( engine->RegisterGlobalProperty( "int[]@ __RegParams", &GameOpt.RegParams ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __RegName", &GameOpt.RegName ) );
BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __RegPassword", &GameOpt.RegPassword ) );
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
BIND_ASSERT( engine->RegisterGlobalProperty( "string@ __MapDataPrefix", &GameOpt.MapDataPrefix ) );

BIND_ASSERT( engine->RegisterGlobalFunction( "ProtoItem@+ GetProtoItem(hash protoId)", asFUNCTION( BIND_CLASS Global_GetProtoItem ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadDataFile(string& dataFileName)", asFUNCTION( BIND_CLASS Global_LoadDataFile ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "int GetConstantValue(int constCollection, string@+ name)", asFUNCTION( BIND_CLASS Global_GetConstantValue ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetConstantName(int constCollection, int value)", asFUNCTION( BIND_CLASS Global_GetConstantName ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void AddConstant(int constCollection, string@+ name, int value)", asFUNCTION( BIND_CLASS Global_AddConstant ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool LoadConstants(int constCollection, string@+ fileName, int pathType)", asFUNCTION( BIND_CLASS Global_LoadConstants ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void AllowSlot(uint8 index, string& slotName)", asFUNCTION( BIND_CLASS Global_AllowSlot ), asCALL_CDECL ) );

// ScriptFunctions.h
BIND_ASSERT( engine->RegisterGlobalFunction( "int Random(int min, int max)", asFUNCTION( Global_Random ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void Log(string& text)", asFUNCTION( Global_Log ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool StrToInt(string@+ text, int& result)", asFUNCTION( Global_StrToInt ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool StrToFloat(string@+ text, float& result)", asFUNCTION( Global_StrToFloat ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetDistantion(uint16 hexX1, uint16 hexY1, uint16 hexX2, uint16 hexY2)", asFUNCTION( Global_GetDistantion ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint8 GetDirection(uint16 fromHexX, uint16 fromHexY, uint16 toHexX, uint16 toHexY)", asFUNCTION( Global_GetDirection ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint8 GetOffsetDir(uint16 fromHexX, uint16 fromHexY, uint16 toHexX, uint16 toHexY, float offset)", asFUNCTION( Global_GetOffsetDir ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetTick()", asFUNCTION( Global_GetTick ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetAngelScriptProperty(int property)", asFUNCTION( Global_GetAngelScriptProperty ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void SetAngelScriptProperty(int property, uint value)", asFUNCTION( Global_SetAngelScriptProperty ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "hash GetStrHash(string@+ str)", asFUNCTION( Global_GetStrHash ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetHashStr(hash h)", asFUNCTION( Global_GetHashStr ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint DecodeUTF8(const string& text, uint& length)", asFUNCTION( Global_DecodeUTF8 ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ EncodeUTF8(uint ucs)", asFUNCTION( Global_EncodeUTF8 ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "string@ GetFilePath(int path)", asFUNCTION( Global_GetFilePath ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "uint GetFolderFileNames(string& path, string@+ extension, bool includeSubdirs, string[]@+ result)", asFUNCTION( Global_GetFolderFileNames ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "bool DeleteFile(string& fileName)", asFUNCTION( Global_DeleteFile ), asCALL_CDECL ) );
BIND_ASSERT( engine->RegisterGlobalFunction( "void CreateDirectoryTree(string& path)", asFUNCTION( Global_CreateDirectoryTree ), asCALL_CDECL ) );

#define BIND_ASSERT_EXT( expr )    BIND_ASSERT( ( expr ) ? 0 : -1 )
if( registrators[ 0 ] )
{
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint16", "SortValue", Property::PublicModifiable ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint8", "Indicator", Property::ProtectedModifiable ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint8", "Info", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "hash", "PicMap", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "hash", "PicInv", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint", "Flags", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint8", "Mode", Property::PublicModifiable ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int8", "LightIntensity", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint8", "LightDistance", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint8", "LightFlags", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint", "LightColor", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "hash", "ScriptId", Property::PrivateServer ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint", "Count", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint", "Cost", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int", "Val0", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int", "Val1", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int", "Val2", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int", "Val3", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int", "Val4", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int", "Val5", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int", "Val6", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int", "Val7", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int", "Val8", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int", "Val9", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint8", "BrokenFlags", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint8", "BrokenCount", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint16", "Deterioration", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "hash", "AmmoPid", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint16", "AmmoCount", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int16", "TrapValue", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint", "LockerId", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint16", "LockerCondition", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint16", "LockerComplexity", Property::PrivateServer ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint", "HolodiskNumber", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint16", "RadioChannel", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint16", "RadioFlags", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint8", "RadioBroadcastSend", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "uint8", "RadioBroadcastRecv", Property::Protected ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int16", "OffsetX", Property::Public ) );
    BIND_ASSERT_EXT( registrators[ 0 ]->Register( "int16", "OffsetY", Property::Public ) );
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
