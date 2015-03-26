#include "Common.h"
#include "Item.h"
#include "ItemManager.h"

#ifdef FONLINE_SERVER
# include "MapManager.h"
# include "CritterManager.h"
# include "AI.h"
#endif

const char* ItemEventFuncName[ ITEM_EVENT_MAX ] =
{
    "void %s(Item&,bool)",                             // ITEM_EVENT_FINISH
    "bool %s(Item&,Critter&,Critter&)",                // ITEM_EVENT_ATTACK
    "bool %s(Item&,Critter&,Critter@,Item@,Scenery@)", // ITEM_EVENT_USE
    "bool %s(Item&,Critter&,Item@)",                   // ITEM_EVENT_USE_ON_ME
    "bool %s(Item&,Critter&,int)",                     // ITEM_EVENT_SKILL
    "void %s(Item&,Critter&)",                         // ITEM_EVENT_DROP
    "void %s(Item&,Critter&,uint8)",                   // ITEM_EVENT_MOVE
    "void %s(Item&,Critter&,bool,uint8)",              // ITEM_EVENT_WALK
};

PropertyRegistrator* Item::PropertiesRegistrator;
Property*            Item::PropertyFlags;
Property*            Item::PropertyLockerCondition;
Property*            Item::PropertyCount;
Property*            Item::PropertyHolodiskNumber;
Property*            Item::PropertySortValue;
Property*            Item::PropertyBrokenFlags;
Property*            Item::PropertyDeterioration;
Property*            Item::PropertyMode;
Property*            Item::PropertyAmmoPid;
Property*            Item::PropertyAmmoCount;

void Item::SetPropertyRegistrator( PropertyRegistrator* registrator )
{
    SAFEDEL( PropertiesRegistrator );
    PropertiesRegistrator = registrator;
    PropertiesRegistrator->FinishRegistration();
    PropertyCount = PropertiesRegistrator->Find( "Count" );
    PropertyFlags = PropertiesRegistrator->Find( "Flags" );
    PropertyLockerCondition = PropertiesRegistrator->Find( "LockerCondition" );
    PropertyHolodiskNumber = PropertiesRegistrator->Find( "HolodiskNumber" );
    PropertySortValue = PropertiesRegistrator->Find( "SortValue" );
    PropertyBrokenFlags = PropertiesRegistrator->Find( "BrokenFlags" );
    PropertyDeterioration = PropertiesRegistrator->Find( "Deterioration" );
    PropertyMode = PropertiesRegistrator->Find( "Mode" );
    PropertyAmmoPid = PropertiesRegistrator->Find( "AmmoPid" );
    PropertyAmmoCount = PropertiesRegistrator->Find( "AmmoCount" );
}

Item::Item( uint id, ProtoItem* proto ): Props( PropertiesRegistrator ),
#ifdef FONLINE_SERVER
                                         ScriptId( *( hash* )Props.FindData( "ScriptId" ) ),
                                         LockerComplexity( *( ushort* )Props.FindData( "LockerComplexity" ) ),
#endif
                                         SortValue( *( ushort* )Props.FindData( "SortValue" ) ),
                                         Indicator( *( uchar* )Props.FindData( "Indicator" ) ),
                                         PicMap( *( hash* )Props.FindData( "PicMap" ) ),
                                         PicInv( *( hash* )Props.FindData( "PicInv" ) ),
                                         Flags( *( uint* )Props.FindData( "Flags" ) ),
                                         Mode( *( uchar* )Props.FindData( "Mode" ) ),
                                         LightIntensity( *( char* )Props.FindData( "LightIntensity" ) ),
                                         LightDistance( *( uchar* )Props.FindData( "LightDistance" ) ),
                                         LightFlags( *( uchar* )Props.FindData( "LightFlags" ) ),
                                         LightColor( *( uint* )Props.FindData( "LightColor" ) ),
                                         Count( *( uint* )Props.FindData( "Count" ) ),
                                         Cost( *( uint* )Props.FindData( "Cost" ) ),
                                         Val0( *( int* )Props.FindData( "Val0" ) ),
                                         Val1( *( int* )Props.FindData( "Val1" ) ),
                                         Val2( *( int* )Props.FindData( "Val2" ) ),
                                         Val3( *( int* )Props.FindData( "Val3" ) ),
                                         Val4( *( int* )Props.FindData( "Val4" ) ),
                                         Val5( *( int* )Props.FindData( "Val5" ) ),
                                         Val6( *( int* )Props.FindData( "Val6" ) ),
                                         Val7( *( int* )Props.FindData( "Val7" ) ),
                                         Val8( *( int* )Props.FindData( "Val8" ) ),
                                         Val9( *( int* )Props.FindData( "Val9" ) ),
                                         BrokenFlags( *( uchar* )Props.FindData( "BrokenFlags" ) ),
                                         BrokenCount( *( uchar* )Props.FindData( "BrokenCount" ) ),
                                         Deterioration( *( ushort* )Props.FindData( "Deterioration" ) ),
                                         AmmoPid( *( hash* )Props.FindData( "AmmoPid" ) ),
                                         AmmoCount( *( ushort* )Props.FindData( "AmmoCount" ) ),
                                         TrapValue( *( short* )Props.FindData( "TrapValue" ) ),
                                         LockerId( *( uint* )Props.FindData( "LockerId" ) ),
                                         LockerCondition( *( ushort* )Props.FindData( "LockerCondition" ) ),
                                         HolodiskNumber( *( uint* )Props.FindData( "HolodiskNumber" ) ),
                                         RadioChannel( *( ushort* )Props.FindData( "RadioChannel" ) ),
                                         RadioFlags( *( ushort* )Props.FindData( "RadioFlags" ) ),
                                         RadioBroadcastSend( *( uchar* )Props.FindData( "RadioBroadcastSend" ) ),
                                         RadioBroadcastRecv( *( uchar* )Props.FindData( "RadioBroadcastRecv" ) ),
                                         OffsetX( *( short* )Props.FindData( "OffsetX" ) ),
                                         OffsetY( *( short* )Props.FindData( "OffsetY" ) )
{
    RUNTIME_ASSERT( proto );

    MEMORY_PROCESS( MEMORY_ITEM, sizeof( Item ) + PropertiesRegistrator->GetWholeDataSize() );

    Id = id;
    RefCounter = 1;
    IsNotValid = false;
    ViewPlaceOnMap = false;
    Accessory = 0;
    memzero( AccBuffer, sizeof( AccBuffer ) );

    Count = 1;
    ItemMngr.ChangeItemStatistics( proto->ProtoId, 1 );

    #ifdef FONLINE_SERVER
    memzero( FuncId, sizeof( FuncId ) );
    ViewByCritter = NULL;
    ChildItems = NULL;
    PLexems = NULL;
    #endif
    #ifdef FONLINE_CLIENT
    Lexems = ScriptString::Create();
    #endif

    SetProto( proto );
}

Item::~Item()
{
    #ifdef FONLINE_SERVER
    if( PLexems )
        MEMORY_PROCESS( MEMORY_ITEM, -LEXEMS_SIZE );
    SAFEDELA( PLexems );
    #endif
    #ifdef FONLINE_CLIENT
    SAFEREL( Lexems );
    #endif
    MEMORY_PROCESS( MEMORY_ITEM, -(int) ( sizeof( Item ) + PropertiesRegistrator->GetWholeDataSize() ) );
}

void Item::SetProto( ProtoItem* proto )
{
    Proto = proto;
    Accessory = ITEM_ACCESSORY_NONE;
    SortValue = 0x7FFF;
    Flags = Proto->Flags;
    Indicator = Proto->IndicatorStart;

    Val0 = Proto->StartValue[ 0 ];
    Val1 = Proto->StartValue[ 1 ];
    Val2 = Proto->StartValue[ 2 ];
    Val3 = Proto->StartValue[ 3 ];
    Val4 = Proto->StartValue[ 4 ];
    Val5 = Proto->StartValue[ 5 ];
    Val6 = Proto->StartValue[ 6 ];
    Val7 = Proto->StartValue[ 7 ];
    Val8 = Proto->StartValue[ 8 ];
    Val9 = Proto->StartValue[ 9 ];

    switch( GetType() )
    {
    case ITEM_TYPE_WEAPON:
        AmmoCount = Proto->Weapon_MaxAmmoCount;
        AmmoPid = Proto->Weapon_DefaultAmmoPid;
        break;
    case ITEM_TYPE_DOOR:
        SETFLAG( Flags, ITEM_GAG );
        if( !Proto->Door_NoBlockMove )
            UNSETFLAG( Flags, ITEM_NO_BLOCK );
        if( !Proto->Door_NoBlockShoot )
            UNSETFLAG( Flags, ITEM_SHOOT_THRU );
        if( !Proto->Door_NoBlockLight )
            UNSETFLAG( Flags, ITEM_LIGHT_THRU );
        LockerCondition = Proto->Locker_Condition;
        break;
    case  ITEM_TYPE_CONTAINER:
        LockerCondition = Proto->Locker_Condition;
        break;
    default:
        break;
    }

    if( IsRadio() )
    {
        RadioChannel = Proto->RadioChannel;
        RadioFlags = Proto->RadioFlags;
        RadioBroadcastSend = Proto->RadioBroadcastSend;
        RadioBroadcastRecv = Proto->RadioBroadcastRecv;
    }

    if( IsHolodisk() )
    {
        HolodiskNumber = Proto->HolodiskNum;
        #ifdef FONLINE_SERVER
        if( !HolodiskNumber )
            HolodiskNumber = Random( 1, 42 );
        #endif
    }
}

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
Item* Item::Clone()
{
    Item* clone = new Item( Id, Proto );
    clone->Accessory = Accessory;
    memcpy( clone->AccBuffer, AccBuffer, sizeof( AccBuffer ) );
    clone->Props = Props;
    # if defined ( FONLINE_CLIENT )
    *clone->Lexems = *Lexems;
    # endif
    return clone;
}
#endif

#ifdef FONLINE_SERVER
void Item::FullClear()
{
    IsNotValid = true;
    Accessory = 0x20 + GetType();

    if( IsContainer() && ChildItems )
    {
        MEMORY_PROCESS( MEMORY_ITEM, -(int) sizeof( ItemMap ) );

        ItemVec del_items = *ChildItems;
        ChildItems->clear();
        SAFEDEL( ChildItems );

        for( auto it = del_items.begin(), end = del_items.end(); it != end; ++it )
        {
            Item* item = *it;
            SYNC_LOCK( item );
            item->Accessory = 0xB1;
            ItemMngr.ItemToGarbage( item );
        }
    }
}

bool Item::ParseScript( const char* script, bool first_time )
{
    if( script && script[ 0 ] )
    {
        hash func_num = Script::BindScriptFuncNum( script, "void %s(Item&,bool)" );
        if( !func_num )
        {
            WriteLogF( _FUNC_, " - Script<%s> bind fail, item pid<%u>.\n", script, GetProtoId() );
            return false;
        }
        ScriptId = func_num;
    }

    if( ScriptId && Script::PrepareScriptFuncContext( ScriptId, _FUNC_, Str::FormatBuf( "Item id<%u>, pid<%u>", GetId(), GetProtoId() ) ) )
    {
        Script::SetArgObject( this );
        Script::SetArgBool( first_time );
        Script::RunPrepared();
    }
    return true;
}

bool Item::PrepareScriptFunc( int num_scr_func )
{
    if( num_scr_func >= ITEM_EVENT_MAX )
        return false;
    if( FuncId[ num_scr_func ] <= 0 )
        return false;
    return Script::PrepareContext( FuncId[ num_scr_func ], _FUNC_, Str::FormatBuf( "Item id<%u>, pid<%u>", GetId(), GetProtoId() ) );
}

void Item::EventFinish( bool deleted )
{
    if( PrepareScriptFunc( ITEM_EVENT_FINISH ) )
    {
        Script::SetArgObject( this );
        Script::SetArgBool( deleted );
        Script::RunPrepared();
    }
}

bool Item::EventAttack( Critter* cr, Critter* target )
{
    bool result = false;
    if( PrepareScriptFunc( ITEM_EVENT_ATTACK ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::SetArgObject( target );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

bool Item::EventUse( Critter* cr, Critter* on_critter, Item* on_item, MapObject* on_scenery )
{
    bool result = false;
    if( PrepareScriptFunc( ITEM_EVENT_USE ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::SetArgObject( on_critter );
        Script::SetArgObject( on_item );
        Script::SetArgObject( on_scenery );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

bool Item::EventUseOnMe( Critter* cr, Item* used_item )
{
    bool result = false;
    if( PrepareScriptFunc( ITEM_EVENT_USE_ON_ME ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::SetArgObject( used_item );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

bool Item::EventSkill( Critter* cr, int skill )
{
    bool result = false;
    if( PrepareScriptFunc( ITEM_EVENT_SKILL ) )
    {
        Script::SetArgObject( this );
        Script::SetArgObject( cr );
        Script::SetArgUInt( skill < 0 ? skill : SKILL_OFFSET( skill ) );
        if( Script::RunPrepared() )
            result = Script::GetReturnedBool();
    }
    return result;
}

void Item::EventDrop( Critter* cr )
{
    if( !PrepareScriptFunc( ITEM_EVENT_DROP ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );
    Script::RunPrepared();
}

void Item::EventMove( Critter* cr, uchar from_slot )
{
    if( !PrepareScriptFunc( ITEM_EVENT_MOVE ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );
    Script::SetArgUChar( from_slot );
    Script::RunPrepared();
}

void Item::EventWalk( Critter* cr, bool entered, uchar dir )
{
    if( !PrepareScriptFunc( ITEM_EVENT_WALK ) )
        return;
    Script::SetArgObject( this );
    Script::SetArgObject( cr );   // Saved in Act_Move
    Script::SetArgBool( entered );
    Script::SetArgUChar( dir );
    Script::RunPrepared();
}
#endif // FONLINE_SERVER

void Item::SetSortValue( ItemVec& items )
{
    ushort sort_value = 0x7FFF;
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item == this )
            continue;
        if( sort_value >= item->SortValue )
            sort_value = item->SortValue - 1;
    }
    SetPropertyValue< ushort >( Item::PropertySortValue, sort_value );
}

bool SortItemsFunc( const Item* l, const Item* r ) { return l->SortValue < r->SortValue; }
void Item::SortItems( ItemVec& items )
{
    std::sort( items.begin(), items.end(), SortItemsFunc );
}

void Item::ClearItems( ItemVec& items )
{
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
        ( *it )->Release();
    items.clear();
}

void Item::SetCount( int val )
{
    SetPropertyValue< uint >( Item::PropertyCount, val );
}

void Item::ChangeCount( int val )
{
    SetPropertyValue< uint >( Item::PropertyCount, Count + val );
}

#ifdef FONLINE_SERVER
void Item::Repair()
{
    if( FLAG( BrokenFlags, BI_BROKEN ) )
    {
        uchar flags = BrokenFlags;
        UNSETFLAG( flags, BI_BROKEN );
        SetPropertyValue< uchar >( Item::PropertyBrokenFlags, flags );
        SetPropertyValue< ushort >( Item::PropertyDeterioration, 0 );
    }
}
#endif

void Item::SetMode( uchar mode )
{
    if( !IsWeapon() )
    {
        mode &= 0xF;
        if( mode == USE_USE && !IsCanUse() && !IsCanUseOnSmth() )
            mode = USE_NONE;
        else if( IsCanUse() || IsCanUseOnSmth() )
            mode = USE_USE;
        else
            mode = USE_NONE;
    }
    else
    {
        uchar use = ( mode & 0xF );
        uchar aim = ( mode >> 4 );

        switch( use )
        {
        case USE_PRIMARY:
            if( Proto->Weapon_ActiveUses & 0x1 )
                break;
            use = 0xF;
            aim = 0;
            break;
        case USE_SECONDARY:
            if( Proto->Weapon_ActiveUses & 0x2 )
                break;
            use = USE_PRIMARY;
            aim = 0;
            break;
        case USE_THIRD:
            if( Proto->Weapon_ActiveUses & 0x4 )
                break;
            use = USE_PRIMARY;
            aim = 0;
            break;
        case USE_RELOAD:
            aim = 0;
            if( Proto->Weapon_MaxAmmoCount )
                break;
            use = USE_PRIMARY;
            break;
        case USE_USE:
            aim = 0;
            if( IsCanUseOnSmth() )
                break;
            use = USE_PRIMARY;
            break;
        default:
            use = USE_PRIMARY;
            aim = 0;
            break;
        }

        if( use < MAX_USES && aim && !Proto->Weapon_Aim[ use ] )
            aim = 0;
        mode = ( aim << 4 ) | ( use & 0xF );
    }

    SetPropertyValue< uchar >( Item::PropertyMode, mode );
}

uint Item::GetCost1st()
{
    uint cost = ( Cost ? Cost : Proto->Cost );
    if( IsWeapon() && AmmoCount )
    {
        ProtoItem* ammo = ItemMngr.GetProtoItem( AmmoPid );
        if( ammo )
            cost += ammo->Cost * AmmoCount;
    }
    if( !cost )
        cost = 1;
    return cost;
}

#ifdef FONLINE_SERVER
void Item::SetLexems( const char* lexems )
{
    if( lexems )
    {
        uint len = Str::Length( lexems );
        if( !len )
        {
            SAFEDELA( PLexems );
            return;
        }

        if( len >= LEXEMS_SIZE )
            len = LEXEMS_SIZE - 1;

        if( !PLexems )
        {
            MEMORY_PROCESS( MEMORY_ITEM, LEXEMS_SIZE );
            PLexems = new char[ LEXEMS_SIZE ];
            if( !PLexems )
                return;
        }
        memcpy( PLexems, lexems, len );
        PLexems[ len ] = 0;
    }
    else
    {
        if( PLexems )
            MEMORY_PROCESS( MEMORY_ITEM, -LEXEMS_SIZE );
        SAFEDELA( PLexems );
    }
}
#endif

void Item::WeapLoadHolder()
{
    if( !AmmoPid )
        SetPropertyValue< hash >( Item::PropertyAmmoPid, Proto->Weapon_DefaultAmmoPid );
    SetPropertyValue< ushort >( Item::PropertyAmmoCount, (ushort) Proto->Weapon_MaxAmmoCount );
}

#ifdef FONLINE_SERVER
void Item::ContAddItem( Item*& item, uint stack_id )
{
    if( !IsContainer() || !item )
        return;

    if( !ChildItems )
    {
        MEMORY_PROCESS( MEMORY_ITEM, sizeof( ItemMap ) );
        ChildItems = new ItemVec();
        if( !ChildItems )
            return;
    }

    if( item->IsStackable() )
    {
        Item* item_ = ContGetItemByPid( item->GetProtoId(), stack_id );
        if( item_ )
        {
            item_->ChangeCount( item->Count );
            ItemMngr.ItemToGarbage( item );
            item = item_;
            return;
        }
    }

    item->AccContainer.StackId = stack_id;
    item->SetSortValue( *ChildItems );
    ContSetItem( item );
}

void Item::ContSetItem( Item* item )
{
    if( !ChildItems )
    {
        MEMORY_PROCESS( MEMORY_ITEM, sizeof( ItemMap ) );
        ChildItems = new ItemVec();
        if( !ChildItems )
            return;
    }

    if( std::find( ChildItems->begin(), ChildItems->end(), item ) != ChildItems->end() )
    {
        WriteLogF( _FUNC_, " - Item already added!\n" );
        return;
    }

    ChildItems->push_back( item );
    item->Accessory = ITEM_ACCESSORY_CONTAINER;
    item->AccContainer.ContainerId = GetId();
}

void Item::ContEraseItem( Item* item )
{
    if( !IsContainer() )
    {
        WriteLogF( _FUNC_, " - Item is not container.\n" );
        return;
    }
    if( !ChildItems )
    {
        WriteLogF( _FUNC_, " - Container items null ptr.\n" );
        return;
    }
    if( !item )
    {
        WriteLogF( _FUNC_, " - Item null ptr.\n" );
        return;
    }

    auto it = std::find( ChildItems->begin(), ChildItems->end(), item );
    if( it != ChildItems->end() )
        ChildItems->erase( it );
    else
        WriteLogF( _FUNC_, " - Item not found, id<%u>, pid<%u>, container<%u>.\n", item->GetId(), item->GetProtoId(), GetId() );

    item->Accessory = 0xd3;

    if( ChildItems->empty() )
        SAFEDEL( ChildItems );
}

Item* Item::ContGetItem( uint item_id, bool skip_hide )
{
    if( !IsContainer() || !ChildItems || !item_id )
        return NULL;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetId() == item_id )
        {
            if( skip_hide && item->IsHidden() )
                return NULL;
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

void Item::ContGetAllItems( ItemVec& items, bool skip_hide, bool sync_lock )
{
    if( !IsContainer() || !ChildItems )
        return;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( !skip_hide || !item->IsHidden() )
            items.push_back( item );
    }

    # pragma MESSAGE("Recheck after synchronization.")
    if( sync_lock && LogicMT )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

# pragma MESSAGE("Add explicit sync lock.")
Item* Item::ContGetItemByPid( hash pid, uint stack_id )
{
    if( !IsContainer() || !ChildItems )
        return NULL;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetProtoId() == pid && ( stack_id == uint( -1 ) || item->AccContainer.StackId == stack_id ) )
        {
            SYNC_LOCK( item );
            return item;
        }
    }
    return NULL;
}

void Item::ContGetItems( ItemVec& items, uint stack_id, bool sync_lock )
{
    if( !IsContainer() || !ChildItems )
        return;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( stack_id == uint( -1 ) || item->AccContainer.StackId == stack_id )
            items.push_back( item );
    }

    # pragma MESSAGE("Recheck after synchronization.")
    if( sync_lock && LogicMT )
        for( auto it = items.begin(), end = items.end(); it != end; ++it )
            SYNC_LOCK( *it );
}

int Item::ContGetFreeVolume( uint stack_id )
{
    if( !IsContainer() )
        return 0;

    int cur_volume = 0;
    int max_volume = Proto->Container_Volume;
    if( !ChildItems )
        return max_volume;

    for( auto it = ChildItems->begin(), end = ChildItems->end(); it != end; ++it )
    {
        Item* item = *it;
        if( stack_id == uint( -1 ) || item->AccContainer.StackId == stack_id )
            cur_volume += item->GetVolume();
    }
    return max_volume - cur_volume;
}

bool Item::ContIsItems()
{
    return ChildItems && ChildItems->size();
}

Item* Item::GetChild( uint child_index )
{
    if( child_index >= ITEM_MAX_CHILDS || !Proto->ChildPid[ child_index ] )
        return NULL;

    if( Accessory == ITEM_ACCESSORY_HEX )
    {
        Map* map = MapMngr.GetMap( AccHex.MapId );
        if( !map )
            return NULL;
        return map->GetItemChild( AccHex.HexX, AccHex.HexY, Proto, child_index );
    }
    else if( Accessory == ITEM_ACCESSORY_CRITTER )
    {
        Critter* cr = CrMngr.GetCritter( AccCritter.Id, true );
        if( cr )
            return cr->GetItemByPid( Proto->ChildPid[ child_index ] );
    }
    else if( Accessory == ITEM_ACCESSORY_CONTAINER )
    {
        Item* cont = ItemMngr.GetItem( AccContainer.ContainerId, true );
        if( cont )
            return cont->ContGetItemByPid( Proto->ChildPid[ child_index ], AccContainer.StackId );
    }
    return NULL;
}
#endif // FONLINE_SERVER

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
# include "ResourceManager.h"
# include "SpriteManager.h"

uint ProtoItem::GetCurSprId()
{
    AnyFrames* anim = ResMngr.GetItemAnim( PicMap );
    if( !anim )
        return 0;

    uint beg = 0, end = 0;
    if( FLAG( Flags, ITEM_SHOW_ANIM ) )
        end = anim->CntFrm - 1;
    if( FLAG( Flags, ITEM_SHOW_ANIM_EXT ) )
    {
        beg = AnimStay[ 0 ];
        end = AnimStay[ 1 ];
    }

    if( beg >= anim->CntFrm )
        beg = anim->CntFrm - 1;
    if( end >= anim->CntFrm )
        end = anim->CntFrm - 1;
    if( beg > end )
        std::swap( beg, end );
    uint count = end - beg + 1;
    uint ticks = anim->Ticks / anim->CntFrm * count;
    return anim->Ind[ beg + ( ( Timer::GameTick() % ticks ) * 100 / ticks ) * count / 100 ];
}

uint Item::GetCurSprId()
{
    AnyFrames* anim = ResMngr.GetItemAnim( GetPicMap() );
    if( !anim )
        return 0;

    uint beg = 0, end = 0;
    if( IsShowAnim() )
        end = anim->CntFrm - 1;
    if( IsShowAnimExt() )
    {
        beg = Proto->AnimStay[ 0 ];
        end = Proto->AnimStay[ 1 ];
    }

    if( beg >= anim->CntFrm )
        beg = anim->CntFrm - 1;
    if( end >= anim->CntFrm )
        end = anim->CntFrm - 1;
    if( beg > end )
        std::swap( beg, end );
    uint count = end - beg + 1;
    uint ticks = anim->Ticks / anim->CntFrm * count;
    return anim->Ind[ beg + ( ( Timer::GameTick() % ticks ) * 100 / ticks ) * count / 100 ];
}
#endif
