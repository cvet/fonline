#include "StdAfx.h"
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

// Item default send data mask
char Item::ItemData::SendMask[ ITEM_DATA_MASK_MAX ][ 120 ] =
{
    // SortValue Info Indicator PicMapHash   PicInvHash   AnimWaitBase AnimStay[2] AnimShow[2] AnimHide[2] Flags        Rate               LightIntensity LightDistance LightFlags   LightColor   ScriptId  TrapValue        Count              Cost                    ScriptValues[10]                                                                                                                        Other 36 bytes
    // ITEM_DATA_MASK_CHOSEN                                                                                           ITEM_DATA_MASK_CHOSEN                                                                                                ITEM_DATA_MASK_CHOSEN
    {  -1, -1,     -1,    0,     -1, -1, -1, -1, -1, -1, -1, -1,      0, 0,       0, 0,        0, 0,        0, 0,     -1, -1, -1, -1,  -1,       -1,            -1,        -1,     -1, -1, -1, -1,   0, 0,     0, 0,    -1, -1, -1, -1,  -1, -1, -1, -1,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0 },
    // ITEM_DATA_MASK_CRITTER                                                                                          ITEM_DATA_MASK_CRITTER                                                                                               ITEM_DATA_MASK_CRITTER
    {    0, 0,     -1,    0,         0, 0, 0, 0,     0, 0, 0, 0,      0, 0,       0, 0,        0, 0,        0, 0,     -1, -1, -1, -1,  -1,       -1,            -1,        -1,     -1, -1, -1, -1,   0, 0,     0, 0,     0,  0,  0,  0,   0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0 },
    // ITEM_DATA_MASK_CRITTER_EXT                                                                                      ITEM_DATA_MASK_CRITTER_EXT                                                                                           ITEM_DATA_MASK_CRITTER_EXT
    {    0, 0,     -1,    0,         0, 0, 0, 0,     0, 0, 0, 0,      0, 0,       0, 0,        0, 0,        0, 0,     -1, -1, -1, -1,  -1,       -1,            -1,        -1,     -1, -1, -1, -1,   0, 0,     0, 0,    -1, -1, -1, -1,   0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0 },
    // ITEM_DATA_MASK_CONTAINER                                                                                        ITEM_DATA_MASK_CONTAINER                                                                                             ITEM_DATA_MASK_CONTAINER
    {  -1, -1,     -1,    0,         0, 0, 0, 0, -1, -1, -1, -1,      0, 0,       0, 0,        0, 0,        0, 0,     -1, -1, -1, -1,  -1,       -1,            -1,        -1,     -1, -1, -1, -1,   0, 0,     0, 0,    -1, -1, -1, -1,  -1, -1, -1, -1,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0 },
    // ITEM_DATA_MASK_MAP                                                                                              ITEM_DATA_MASK_MAP                                                                                                   ITEM_DATA_MASK_MAP
    {  -1, -1,     -1,    0,     -1, -1, -1, -1,     0, 0, 0, 0,    -1, -1,     -1, -1,      -1, -1,      -1, -1,     -1, -1, -1, -1,  -1,       -1,            -1,        -1,     -1, -1, -1, -1,   0, 0,     0, 0,     0,  0,  0,  0,   0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0 },
};

void Item::Init( ProtoItem* proto )
{
    if( !proto )
    {
        WriteLogF( _FUNC_, " - Proto is null ptr.\n" );
        return;
    }

    #ifdef FONLINE_SERVER
    ViewByCritter = NULL;
    ViewPlaceOnMap = false;
    ChildItems = NULL;
    From = 0;
    #endif

    memzero( &Data, sizeof( Data ) );
    Proto = proto;
    Accessory = ITEM_ACCESSORY_NONE;
    Data.SortValue = 0x7FFF;
    Data.AnimWaitBase = Proto->AnimWaitBase;
    Data.AnimStay[ 0 ] = Proto->AnimStay[ 0 ];
    Data.AnimStay[ 1 ] = Proto->AnimStay[ 1 ];
    Data.AnimShow[ 0 ] = Proto->AnimShow[ 0 ];
    Data.AnimShow[ 1 ] = Proto->AnimShow[ 1 ];
    Data.AnimHide[ 0 ] = Proto->AnimHide[ 0 ];
    Data.AnimHide[ 1 ] = Proto->AnimHide[ 1 ];
    Data.Flags = Proto->Flags;
    Data.Indicator = Proto->IndicatorStart;

    for( int i = 0; i < ITEM_MAX_SCRIPT_VALUES; i++ )
        Data.ScriptValues[ i ] = Proto->StartValue[ i ];

    switch( GetType() )
    {
    case ITEM_TYPE_WEAPON:
        Data.AmmoCount = Proto->Weapon_MaxAmmoCount;
        Data.AmmoPid = Proto->Weapon_DefaultAmmoPid;
        break;
    case ITEM_TYPE_DOOR:
        SETFLAG( Data.Flags, ITEM_GAG );
        if( !Proto->Door_NoBlockMove )
            UNSETFLAG( Data.Flags, ITEM_NO_BLOCK );
        if( !Proto->Door_NoBlockShoot )
            UNSETFLAG( Data.Flags, ITEM_SHOOT_THRU );
        if( !Proto->Door_NoBlockLight )
            UNSETFLAG( Data.Flags, ITEM_LIGHT_THRU );
        Data.LockerCondition = Proto->Locker_Condition;
        break;
    case  ITEM_TYPE_CONTAINER:
        Data.LockerCondition = Proto->Locker_Condition;
        break;
    default:
        break;
    }

    if( IsRadio() )
    {
        Data.RadioChannel = Proto->RadioChannel;
        Data.RadioFlags = Proto->RadioFlags;
        Data.RadioBroadcastSend = Proto->RadioBroadcastSend;
        Data.RadioBroadcastRecv = Proto->RadioBroadcastRecv;
    }

    if( IsHolodisk() )
    {
        Data.HolodiskNumber = Proto->HolodiskNum;
        #ifdef FONLINE_SERVER
        if( !Data.HolodiskNumber )
            Data.HolodiskNumber = Random( 1, 42 );
        #endif
    }

    #ifdef FONLINE_CLIENT
    Lexems = "";
    #endif
}

Item* Item::Clone()
{
    Item* clone = new Item();
    clone->Id = Id;
    clone->Proto = Proto;
    clone->Accessory = Accessory;
    memcpy( clone->AccBuffer, AccBuffer, sizeof( AccBuffer ) );
    clone->Data = Data;

    #ifdef FONLINE_SERVER
    clone->ViewByCritter = NULL;
    clone->ViewPlaceOnMap = false;
    clone->ChildItems = NULL;
    clone->PLexems = NULL;
    #endif
    #ifdef FONLINE_CLIENT
    clone->Lexems = Lexems;
    #endif

    return clone;
}


#ifdef FONLINE_SERVER
void Item::FullClear()
{
    IsNotValid = true;
    Accessory = 0x20 + GetType();

    if( IsContainer() && ChildItems )
    {
        MEMORY_PROCESS( MEMORY_ITEM, -(int) sizeof( ItemPtrMap ) );

        ItemPtrVec del_items = *ChildItems;
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
        uint func_num = Script::GetScriptFuncNum( script, "void %s(Item&,bool)" );
        if( !func_num )
        {
            WriteLogF( _FUNC_, " - Script<%s> bind fail, item pid<%u>.\n", script, GetProtoId() );
            return false;
        }
        Data.ScriptId = func_num;
    }

    if( Data.ScriptId && Script::PrepareContext( Script::GetScriptFuncBindId( Data.ScriptId ), _FUNC_, Str::FormatBuf( "Item id<%u>, pid<%u>", GetId(), GetProtoId() ) ) )
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

void Item::SetSortValue( ItemPtrVec& items )
{
    ushort sort_value = 0x7FFF;
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item == this )
            continue;
        if( sort_value >= item->GetSortValue() )
            sort_value = item->GetSortValue() - 1;
    }
    Data.SortValue = sort_value;
}

bool SortItemsFunc( const Item& l, const Item& r ) { return l.Data.SortValue < r.Data.SortValue; }
void Item::SortItems( ItemVec& items )             { std::sort( items.begin(), items.end(), SortItemsFunc ); }

uint Item::GetCount()
{
    return IsStackable() ? Data.Count : 1;
}

void Item::Count_Set( uint val )
{
    if( !IsStackable() )
        return;

    if( Data.Count > val )
        ItemMngr.SubItemStatistics( GetProtoId(), Data.Count - val );
    else if( Data.Count < val )
        ItemMngr.AddItemStatistics( GetProtoId(), val - Data.Count );
    Data.Count = val;
}

void Item::Count_Add( uint val )
{
    if( !IsStackable() )
        return;

    Data.Count += val;
    ItemMngr.AddItemStatistics( GetProtoId(), val );
}

void Item::Count_Sub( uint val )
{
    if( !IsStackable() )
        return;

    if( val > Data.Count )
        val = Data.Count;
    Data.Count -= val;
    ItemMngr.SubItemStatistics( GetProtoId(), val );
}

#ifdef FONLINE_SERVER
void Item::Repair()
{
    uchar&  flags = Data.BrokenFlags;
    uchar&  broken_count = Data.BrokenCount;
    ushort& deterioration = Data.Deterioration;

    if( FLAG( flags, BI_BROKEN ) )
    {
        UNSETFLAG( flags, BI_BROKEN );
        deterioration = 0;
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
    Data.Mode = mode;
}

uint Item::GetCost1st()
{
    uint cost = ( Data.Cost ? Data.Cost : Proto->Cost );
    // if(IsDeteriorable()) cost-=cost*GetWearProc()/100;
    if( IsWeapon() && Data.AmmoCount )
    {
        ProtoItem* ammo = ItemMngr.GetProtoItem( Data.AmmoPid );
        if( ammo )
            cost += ammo->Cost * Data.AmmoCount;
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
    if( !Data.AmmoPid )
        Data.AmmoPid = Proto->Weapon_DefaultAmmoPid;
    Data.AmmoCount = Proto->Weapon_MaxAmmoCount;
}

#ifdef FONLINE_SERVER
void Item::ContAddItem( Item*& item, uint stack_id )
{
    if( !IsContainer() || !item )
        return;

    if( !ChildItems )
    {
        MEMORY_PROCESS( MEMORY_ITEM, sizeof( ItemPtrMap ) );
        ChildItems = new ItemPtrVec();
        if( !ChildItems )
            return;
    }

    if( item->IsStackable() )
    {
        Item* item_ = ContGetItemByPid( item->GetProtoId(), stack_id );
        if( item_ )
        {
            item_->Count_Add( item->GetCount() );
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
        MEMORY_PROCESS( MEMORY_ITEM, sizeof( ItemPtrMap ) );
        ChildItems = new ItemPtrVec();
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

void Item::ContGetAllItems( ItemPtrVec& items, bool skip_hide, bool sync_lock )
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
Item* Item::ContGetItemByPid( ushort pid, uint stack_id )
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

void Item::ContGetItems( ItemPtrVec& items, uint stack_id, bool sync_lock )
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
        beg = Data.AnimStay[ 0 ];
        end = Data.AnimStay[ 1 ];
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
