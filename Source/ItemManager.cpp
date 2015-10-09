#include "Common.h"
#include "ItemManager.h"
#include "IniParser.h"
#include "MsgFiles.h"

#ifdef FONLINE_SERVER
# include "Critter.h"
# include "Map.h"
# include "CritterManager.h"
# include "MapManager.h"
# include "Script.h"
# include "EntityManager.h"
#endif

#ifdef FONLINE_MAPPER
# include "Script.h"
#endif

ItemManager ItemMngr;

bool ItemManager::Init()
{
    WriteLog( "Item manager initialization...\n" );

    Clear();
    allProtos.clear();

    WriteLog( "Item manager initialization complete.\n" );
    return true;
}

void ItemManager::Finish()
{
    WriteLog( "Item manager finish...\n" );

    Clear();
    allProtos.clear();

    WriteLog( "Item manager finish complete.\n" );
}

void ItemManager::Clear()
{
    #ifdef FONLINE_SERVER
    radioItems.clear();
    #endif
}

bool ItemManager::LoadProtos()
{
    WriteLog( "Load item prototypes...\n" );

    FilesCollection files = FilesCollection( "foitem" );
    uint            files_loaded = 0;
    int             errors = 0;
    while( files.IsNextFile() )
    {
        const char*  file_name;
        FileManager& file = files.GetNextFile( &file_name );
        if( !file.IsLoaded() )
        {
            WriteLog( "Unable to open file '%s'.\n", file_name );
            errors++;
            continue;
        }

        uint pid = Str::GetHash( file_name );
        if( allProtos.count( pid ) )
        {
            WriteLog( "Proto item '%s' already loaded.\n", file_name );
            errors++;
            continue;
        }

        ProtoItem* proto = new ProtoItem( pid );

        IniParser  foitem;
        foitem.LoadFilePtr( (char*) file.GetBuf(), file.GetFsize() );
        if( foitem.GotoNextApp( "Item" ) )
        {
            if( !proto->ItemPropsEntity.Props.LoadFromText( foitem.GetApp( "Item" ) ) )
                errors++;
        }
        if( foitem.GotoNextApp( "ProtoItem" ) )
        {
            if( !proto->Props.LoadFromText( foitem.GetApp( "ProtoItem" ) ) )
                errors++;
        }

        // Texts
        foitem.CacheApps();
        StrSet& apps = foitem.GetCachedApps();
        for( auto it = apps.begin(), end = apps.end(); it != end; ++it )
        {
            const string& app_name = *it;
            if( !( app_name.size() == 9 && app_name.find( "Text_" ) == 0 ) )
                continue;

            char* app_content = foitem.GetApp( app_name.c_str() );
            FOMsg temp_msg;
            temp_msg.LoadFromString( app_content, Str::Length( app_content ) );
            SAFEDELA( app_content );

            FOMsg* msg = new FOMsg();
            uint   str_num = 0;
            while( ( str_num = temp_msg.GetStrNumUpper( str_num ) ) )
            {
                uint count = temp_msg.Count( str_num );
                uint new_str_num = ITEM_STR_ID( proto->ProtoId, str_num );
                for( uint n = 0; n < count; n++ )
                    msg->AddStr( new_str_num, temp_msg.GetStr( str_num, n ) );
            }

            proto->TextsLang.push_back( *(uint*) app_name.substr( 5 ).c_str() );
            proto->Texts.push_back( msg );
        }

        #ifdef FONLINE_MAPPER
        switch( proto->GetType() )
        {
        case ITEM_TYPE_ARMOR:
            proto->CollectionName = "armor";
            break;
        case ITEM_TYPE_DRUG:
            proto->CollectionName = "drug";
            break;
        case ITEM_TYPE_WEAPON:
            proto->CollectionName = "weapon";
            break;
        case ITEM_TYPE_AMMO:
            proto->CollectionName = "ammo";
            break;
        case ITEM_TYPE_MISC:
            proto->CollectionName = "misc";
            break;
        case ITEM_TYPE_KEY:
            proto->CollectionName = "key";
            break;
        case ITEM_TYPE_CONTAINER:
            proto->CollectionName = "container";
            break;
        case ITEM_TYPE_DOOR:
            proto->CollectionName = "door";
            break;
        case ITEM_TYPE_GRID:
            proto->CollectionName = "grid";
            break;
        case ITEM_TYPE_GENERIC:
            proto->CollectionName = "generic";
            break;
        case ITEM_TYPE_WALL:
            proto->CollectionName = "wall";
            break;
        case ITEM_TYPE_CAR:
            proto->CollectionName = "car";
            break;
        default:
            proto->CollectionName = "other";
            break;
        }
        #endif

        allProtos.insert( PAIR( proto->ProtoId, proto ) );
        files_loaded++;
    }

    WriteLog( "Load item prototypes complete, count %u.\n", files_loaded );
    return errors == 0;
}

ProtoItem* ItemManager::GetProtoItem( hash pid )
{
    auto it = allProtos.find( pid );
    return it != allProtos.end() ? it->second : nullptr;
}

ProtoItemMap& ItemManager::GetAllProtos()
{
    return allProtos;
}

void ItemManager::GetBinaryData( UCharVec& data )
{
    data.resize( 0 );
    WriteData( data, (uint) allProtos.size() );
    for( auto it = allProtos.begin(), end = allProtos.end(); it != end; ++it )
    {
        hash       proto_id = it->first;
        ProtoItem* proto_item = it->second;
        WriteData( data, proto_id );
        for( uint part = 0; part < 2; part++ )
        {
            Properties& props = ( part == 0 ? proto_item->Props : proto_item->ItemPropsEntity.Props );
            PUCharVec*  props_data;
            UIntVec*    props_data_sizes;
            props.StoreData( true, &props_data, &props_data_sizes );
            WriteData( data, (ushort) props_data->size() );
            for( size_t i = 0; i < props_data->size(); i++ )
            {
                uint cur_size = props_data_sizes->at( i );
                WriteData( data, cur_size );
                WriteDataArr( data, props_data->at( i ), cur_size );
            }
        }
    }

    Crypt.Compress( data );
}

void ItemManager::SetBinaryData( UCharVec& data )
{
    allProtos.clear();

    if( !Crypt.Uncompress( data, 15 ) )
        return;
    if( data.size() < sizeof( uint ) )
        return;

    PUCharVec props_data;
    UIntVec   props_data_sizes;
    uint      read_pos = 0;
    uint      protos_count = ReadData< uint >( data, read_pos );
    for( uint i = 0; i < protos_count; i++ )
    {
        hash       pid = ReadData< hash >( data, read_pos );
        ProtoItem* proto_item = new ProtoItem( pid );
        for( uint part = 0; part < 2; part++ )
        {
            Properties& props = ( part == 0 ? proto_item->Props : proto_item->ItemPropsEntity.Props );
            uint        data_count = ReadData< ushort >( data, read_pos );
            props_data.resize( data_count );
            props_data_sizes.resize( data_count );
            for( uint j = 0; j < data_count; j++ )
            {
                props_data_sizes[ j ] = ReadData< uint >( data, read_pos );
                props_data[ j ] = ReadDataArr< uchar >( data, props_data_sizes[ j ], read_pos );
            }
            props.RestoreData( props_data, props_data_sizes );
        }
        RUNTIME_ASSERT( !allProtos.count( proto_item->ProtoId ) );
        allProtos.insert( PAIR( proto_item->ProtoId, proto_item ) );
    }
}

#ifdef FONLINE_SERVER
void ItemManager::GetGameItems( ItemVec& items )
{
    EntityMngr.GetItems( items );
}

uint ItemManager::GetItemsCount()
{
    return EntityMngr.GetEntitiesCount( EntityType::Item );
}

void ItemManager::SetCritterItems( Critter* cr )
{
    ItemVec items;
    EntityMngr.GetCritterItems( cr->GetId(), items );

    for( auto it = items.begin(); it != items.end(); ++it )
    {
        Item* item = *it;
        SYNC_LOCK( item );

        cr->SetItem( item );
        if( item->GetIsRadio() )
            RadioRegister( item, true );
    }
}

Item* ItemManager::CreateItem( hash pid, uint count /* = 0 */ )
{
    ProtoItem* proto = GetProtoItem( pid );
    if( !proto )
    {
        WriteLogF( _FUNC_, " - Proto item '%s' not found.\n", Str::GetName( pid ) );
        return nullptr;
    }

    Item* item = new Item( 0, proto );
    SYNC_LOCK( item );

    // Main collection
    EntityMngr.RegisterEntity( item );

    // Count
    if( count )
        item->SetCount( count );

    // Radio collection
    if( item->GetIsRadio() )
        RadioRegister( item, true );

    // Prototype script
    if( proto->ScriptName.length() > 0 )         // Only for new items
    {
        item->SetScript( proto->ScriptName.c_str(), true );
        if( item->IsDestroyed )
        {
            WriteLogF( _FUNC_, " - Item destroyed after prototype '%s' initialization.\n", Str::GetName( pid ) );
            return nullptr;
        }
    }
    return item;
}

bool ItemManager::RestoreItem( uint id, hash pid, Properties& props, uchar acc, char* acc_buf )
{
    ProtoItem* proto = GetProtoItem( pid );
    if( !proto )
    {
        WriteLogF( _FUNC_, " - Proto item '%s' not found.\n", Str::GetName( pid ) );
        return false;
    }

    Item* item = new Item( id, proto );
    SYNC_LOCK( item );

    // Restore accessory
    item->Props = props;
    item->Accessory = acc;
    memcpy( item->AccBuffer, acc_buf, sizeof( item->AccBuffer ) );

    // Main collection
    EntityMngr.RegisterEntity( item );

    return true;
}

void ItemManager::DeleteItem( Item* item )
{
    // Synchronize
    SYNC_LOCK( item );

    // Redundant calls
    if( item->IsDestroying || item->IsDestroyed )
        return;
    item->IsDestroying = true;

    // Finish events
    item->EventFinish( true );

    // Delete children
    for( int i = 0; i < ITEM_MAX_CHILDS; i++ )
    {
        Item* child = item->GetChild( i );
        if( child )
            DeleteItem( child );
    }

    // Tear off from environment
    while( item->Accessory != ITEM_ACCESSORY_NONE || item->ContIsItems() )
    {
        // Delete from owner
        EraseItemHolder( item );

        // Delete container items
        if( item->IsContainer() )
            item->ContDeleteItems();
    }

    // Erase from statistics
    ChangeItemStatistics( item->GetProtoId(), -(int) item->GetCount() );

    // Erase from radio collection
    if( item->GetIsRadio() )
        RadioRegister( item, false );

    // Erase from main collection
    EntityMngr.UnregisterEntity( item );

    // Invalidate for use
    item->IsDestroyed = true;

    // Release after some time
    Job::DeferredRelease( item );
}

Item* ItemManager::SplitItem( Item* item, uint count )
{
    if( !item->IsStackable() )
    {
        WriteLogF( _FUNC_, " - Splitted item '%s' is not stackable, id %u.\n", item->GetName(), item->GetId() );
        return nullptr;
    }

    uint item_count = item->GetCount();
    if( !count || count >= item_count )
    {
        WriteLogF( _FUNC_, " - Invalid item '%s' count, id %u, count %u, split count %u.\n", item->GetName(), item->GetId(), item_count, count );
        return nullptr;
    }

    Item* new_item = CreateItem( item->GetProtoId() ); // Ignore init script
    if( !new_item )
    {
        WriteLogF( _FUNC_, " - Create item '%s' fail, count %u.\n", item->GetName(), count );
        return nullptr;
    }

    item->ChangeCount( -(int) count );
    new_item->Props = item->Props;
    new_item->SetCount( count );

    // Radio collection
    if( new_item->GetIsRadio() )
        RadioRegister( new_item, true );

    return new_item;
}

Item* ItemManager::GetItem( uint item_id, bool sync_lock )
{
    Item* item = (Item*) EntityMngr.GetEntity( item_id, EntityType::Item );
    if( item && sync_lock )
        SYNC_LOCK( item );
    return item;
}

void ItemManager::EraseItemHolder( Item* item )
{
    switch( item->Accessory )
    {
    case ITEM_ACCESSORY_CRITTER:
    {
        Critter* cr = CrMngr.GetCritter( item->AccCritter.Id, true );
        if( cr )
            cr->EraseItem( item, true );
        else if( item->GetIsRadio() )
            ItemMngr.RadioRegister( item, true );
    }
    break;
    case ITEM_ACCESSORY_HEX:
    {
        Map* map = MapMngr.GetMap( item->AccHex.MapId, true );
        if( map )
            map->EraseItem( item->GetId() );
    }
    break;
    case ITEM_ACCESSORY_CONTAINER:
    {
        Item* cont = ItemMngr.GetItem( item->AccContainer.ContainerId, true );
        if( cont )
            cont->ContEraseItem( item );
    }
    break;
    default:
        break;
    }
    item->Accessory = ITEM_ACCESSORY_NONE;
}

void ItemManager::MoveItem( Item* item, uint count, Critter* to_cr )
{
    if( item->Accessory == ITEM_ACCESSORY_CRITTER && item->AccCritter.Id == to_cr->GetId() )
        return;

    if( count >= item->GetCount() || !item->IsStackable() )
    {
        EraseItemHolder( item );
        to_cr->AddItem( item, true );
    }
    else
    {
        Item* item_ = ItemMngr.SplitItem( item, count );
        if( item_ )
            to_cr->AddItem( item_, true );
    }
}

void ItemManager::MoveItem( Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy )
{
    if( item->Accessory == ITEM_ACCESSORY_HEX && item->AccHex.MapId == to_map->GetId() && item->AccHex.HexX == to_hx && item->AccHex.HexY == to_hy )
        return;

    if( count >= item->GetCount() || !item->IsStackable() )
    {
        EraseItemHolder( item );
        to_map->AddItem( item, to_hx, to_hy );
    }
    else
    {
        Item* item_ = ItemMngr.SplitItem( item, count );
        if( item_ )
            to_map->AddItem( item_, to_hx, to_hy );
    }
}

void ItemManager::MoveItem( Item* item, uint count, Item* to_cont, uint stack_id )
{
    if( item->Accessory == ITEM_ACCESSORY_CONTAINER && item->AccContainer.ContainerId == to_cont->GetId() && item->AccContainer.StackId == stack_id )
        return;

    if( count >= item->GetCount() || !item->IsStackable() )
    {
        EraseItemHolder( item );
        to_cont->ContAddItem( item, stack_id );
    }
    else
    {
        Item* item_ = ItemMngr.SplitItem( item, count );
        if( item_ )
            to_cont->ContAddItem( item_, stack_id );
    }
}

Item* ItemManager::AddItemContainer( Item* cont, hash pid, uint count, uint stack_id )
{
    if( !cont || !cont->IsContainer() )
        return nullptr;

    Item* item = cont->ContGetItemByPid( pid, stack_id );
    Item* result = nullptr;

    if( item )
    {
        if( item->IsStackable() )
        {
            item->ChangeCount( count );
            result = item;
        }
        else
        {
            if( count > MAX_ADDED_NOGROUP_ITEMS )
                count = MAX_ADDED_NOGROUP_ITEMS;
            for( uint i = 0; i < count; ++i )
            {
                item = ItemMngr.CreateItem( pid );
                if( !item )
                    continue;
                cont->ContAddItem( item, stack_id );
                result = item;
            }
        }
    }
    else
    {
        ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
        if( !proto_item )
            return result;

        if( proto_item->GetStackable() )
        {
            item = ItemMngr.CreateItem( pid, count );
            if( !item )
                return result;
            cont->ContAddItem( item, stack_id );
            result = item;
        }
        else
        {
            if( count > MAX_ADDED_NOGROUP_ITEMS )
                count = MAX_ADDED_NOGROUP_ITEMS;
            for( uint i = 0; i < count; ++i )
            {
                item = ItemMngr.CreateItem( pid );
                if( !item )
                    continue;
                cont->ContAddItem( item, stack_id );
                result = item;
            }
        }
    }

    return result;
}

Item* ItemManager::AddItemCritter( Critter* cr, hash pid, uint count )
{
    if( !count )
        return nullptr;

    Item* item = cr->GetItemByPid( pid );
    Item* result = nullptr;

    if( item )
    {
        if( item->IsStackable() )
        {
            item->ChangeCount( count );
            result = item;
        }
        else
        {
            if( count > MAX_ADDED_NOGROUP_ITEMS )
                count = MAX_ADDED_NOGROUP_ITEMS;
            for( uint i = 0; i < count; ++i )
            {
                item = ItemMngr.CreateItem( pid );
                if( !item )
                    break;
                cr->AddItem( item, true );
                result = item;
            }
        }
    }
    else
    {
        ProtoItem* proto_item = ItemMngr.GetProtoItem( pid );
        if( !proto_item )
            return result;

        if( proto_item->GetStackable() )
        {
            item = ItemMngr.CreateItem( pid, count );
            if( !item )
                return result;
            cr->AddItem( item, true );
            result = item;
        }
        else
        {
            if( count > MAX_ADDED_NOGROUP_ITEMS )
                count = MAX_ADDED_NOGROUP_ITEMS;
            for( uint i = 0; i < count; ++i )
            {
                item = ItemMngr.CreateItem( pid );
                if( !item )
                    break;
                cr->AddItem( item, true );
                result = item;
            }
        }
    }

    return result;
}

bool ItemManager::SubItemCritter( Critter* cr, hash pid, uint count, ItemVec* erased_items )
{
    if( !count )
        return true;

    Item* item = cr->GetItemByPidInvPriority( pid );
    if( !item )
        return true;

    if( item->IsStackable() )
    {
        if( count >= item->GetCount() )
        {
            cr->EraseItem( item, true );
            if( !erased_items )
                ItemMngr.DeleteItem( item );
            else
                erased_items->push_back( item );
        }
        else
        {
            if( erased_items )
            {
                Item* item_ = ItemMngr.SplitItem( item, count );
                if( item_ )
                    erased_items->push_back( item_ );
            }
            else
            {
                item->ChangeCount( -(int) count );
            }
        }
    }
    else
    {
        for( uint i = 0; i < count; ++i )
        {
            cr->EraseItem( item, true );
            if( !erased_items )
                ItemMngr.DeleteItem( item );
            else
                erased_items->push_back( item );
            item = cr->GetItemByPidInvPriority( pid );
            if( !item )
                return true;
        }
    }

    return true;
}

bool ItemManager::SetItemCritter( Critter* cr, hash pid, uint count )
{
    uint cur_count = cr->CountItemPid( pid );
    if( cur_count > count )
        return SubItemCritter( cr, pid, cur_count - count );
    else if( cur_count < count )
        return AddItemCritter( cr, pid, count - cur_count ) != nullptr;
    return true;
}

bool ItemManager::MoveItemCritters( Critter* from_cr, Critter* to_cr, uint item_id, uint count )
{
    Item* item = from_cr->GetItem( item_id, false );
    if( !item )
        return false;

    if( !count || count > item->GetCount() )
        count = item->GetCount();

    if( item->IsStackable() && item->GetCount() > count )
    {
        Item* item_ = to_cr->GetItemByPid( item->GetProtoId() );
        if( !item_ )
        {
            item_ = ItemMngr.CreateItem( item->GetProtoId(), count );
            if( !item_ )
            {
                WriteLogF( _FUNC_, " - Create item '%s' fail.\n", item->GetName() );
                return false;
            }

            to_cr->AddItem( item_, true );
        }
        else
        {
            item_->ChangeCount( count );
        }

        item->ChangeCount( -(int) count );
    }
    else
    {
        from_cr->EraseItem( item, true );
        to_cr->AddItem( item, true );
    }

    return true;
}

bool ItemManager::MoveItemCritterToCont( Critter* from_cr, Item* to_cont, uint item_id, uint count, uint stack_id )
{
    if( !to_cont->IsContainer() )
        return false;

    Item* item = from_cr->GetItem( item_id, false );
    if( !item )
        return false;

    if( !count || count > item->GetCount() )
        count = item->GetCount();

    if( item->IsStackable() && item->GetCount() > count )
    {
        Item* item_ = to_cont->ContGetItemByPid( item->GetProtoId(), stack_id );
        if( !item_ )
        {
            item_ = ItemMngr.CreateItem( item->GetProtoId(), count );
            if( !item_ )
            {
                WriteLogF( _FUNC_, " - Create item '%s' fail.\n", item->GetName() );
                return false;
            }

            item_->AccContainer.StackId = stack_id;
            to_cont->ContSetItem( item_ );
        }
        else
        {
            item_->ChangeCount( count );
        }

        item->ChangeCount( -(int) count );
    }
    else
    {
        from_cr->EraseItem( item, true );
        to_cont->ContAddItem( item, stack_id );
    }

    return true;
}

bool ItemManager::MoveItemCritterFromCont( Item* from_cont, Critter* to_cr, uint item_id, uint count )
{
    if( !from_cont->IsContainer() )
        return false;

    Item* item = from_cont->ContGetItem( item_id, false );
    if( !item )
        return false;

    if( !count || count > item->GetCount() )
        count = item->GetCount();

    if( item->IsStackable() && item->GetCount() > count )
    {
        Item* item_ = to_cr->GetItemByPid( item->GetProtoId() );
        if( !item_ )
        {
            item_ = ItemMngr.CreateItem( item->GetProtoId(), count );
            if( !item_ )
            {
                WriteLogF( _FUNC_, " - Create item '%s' fail.\n", item->GetName() );
                return false;
            }

            to_cr->AddItem( item_, true );
        }
        else
        {
            item_->ChangeCount( count );
        }

        item->ChangeCount( -(int) count );
    }
    else
    {
        from_cont->ContEraseItem( item );
        to_cr->AddItem( item, true );
    }

    return true;
}

bool ItemManager::MoveItemsContainers( Item* from_cont, Item* to_cont, uint from_stack_id, uint to_stack_id )
{
    if( !from_cont->IsContainer() || !to_cont->IsContainer() )
        return false;
    if( !from_cont->ContIsItems() )
        return true;

    ItemVec items;
    from_cont->ContGetItems( items, from_stack_id, true );
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        from_cont->ContEraseItem( item );
        to_cont->ContAddItem( item, to_stack_id );
    }

    return true;
}

bool ItemManager::MoveItemsContToCritter( Item* from_cont, Critter* to_cr, uint stack_id )
{
    if( !from_cont->IsContainer() )
        return false;
    if( !from_cont->ContIsItems() )
        return true;

    ItemVec items;
    from_cont->ContGetItems( items, stack_id, true );
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        from_cont->ContEraseItem( item );
        to_cr->AddItem( item, true );
    }

    return true;
}

void ItemManager::RadioRegister( Item* radio, bool add )
{
    SCOPE_LOCK( radioItemsLocker );

    auto it = std::find( radioItems.begin(), radioItems.end(), radio );

    if( add )
    {
        if( it == radioItems.end() )
            radioItems.push_back( radio );
    }
    else
    {
        if( it != radioItems.end() )
            radioItems.erase( it );
    }
}

void ItemManager::RadioSendText( Critter* cr, const char* text, ushort text_len, bool unsafe_text, ushort text_msg, uint num_str, UShortVec& channels )
{
    ItemVec radios;
    ItemVec items = cr->GetItemsNoLock();
    for( auto it = items.begin(), end = items.end(); it != end; ++it )
    {
        Item* item = *it;
        if( item->GetIsRadio() && item->RadioIsSendActive() &&
            std::find( channels.begin(), channels.end(), item->GetRadioChannel() ) == channels.end() )
        {
            channels.push_back( item->GetRadioChannel() );
            radios.push_back( item );

            if( radios.size() > 100 )
                break;
        }
    }

    for( uint i = 0, j = (uint) radios.size(); i < j; i++ )
    {
        RadioSendTextEx( channels[ i ],
                         radios[ i ]->GetRadioBroadcastSend(), cr->GetMapId(), cr->Data.WorldX, cr->Data.WorldY,
                         text, text_len, cr->IntellectCacheValue, unsafe_text, text_msg, num_str, nullptr );
    }
}

void ItemManager::RadioSendTextEx( ushort channel, int broadcast_type, uint from_map_id, ushort from_wx, ushort from_wy,
                                   const char* text, ushort text_len, ushort intellect, bool unsafe_text,
                                   ushort text_msg, uint num_str, const char* lexems )
{
    // Broadcast
    if( broadcast_type != RADIO_BROADCAST_FORCE_ALL && broadcast_type != RADIO_BROADCAST_WORLD &&
        broadcast_type != RADIO_BROADCAST_MAP && broadcast_type != RADIO_BROADCAST_LOCATION &&
        !( broadcast_type >= 101 && broadcast_type <= 200 ) /*RADIO_BROADCAST_ZONE*/ )
        return;
    if( ( broadcast_type == RADIO_BROADCAST_MAP || broadcast_type == RADIO_BROADCAST_LOCATION ) && !from_map_id )
        return;

    int  broadcast = 0;
    uint broadcast_map_id = 0;
    uint broadcast_loc_id = 0;

    // Get copy of all radios
    radioItemsLocker.Lock();
    ItemVec radio_items = radioItems;
    radioItemsLocker.Unlock();

    // Multiple sending controlling
    // Not thread safe, but this not so important in this case
    static uint msg_count = 0;
    msg_count++;

    // Send
    for( auto it = radio_items.begin(), end = radio_items.end(); it != end; ++it )
    {
        Item* radio = *it;

        if( radio->GetRadioChannel() == channel && radio->RadioIsRecvActive() )
        {
            if( broadcast_type != RADIO_BROADCAST_FORCE_ALL && radio->GetRadioBroadcastRecv() != RADIO_BROADCAST_FORCE_ALL )
            {
                if( broadcast_type == RADIO_BROADCAST_WORLD )
                    broadcast = radio->GetRadioBroadcastRecv();
                else if( radio->GetRadioBroadcastRecv() == RADIO_BROADCAST_WORLD )
                    broadcast = broadcast_type;
                else
                    broadcast = MIN( broadcast_type, radio->GetRadioBroadcastRecv() );

                if( broadcast == RADIO_BROADCAST_WORLD )
                    broadcast = RADIO_BROADCAST_FORCE_ALL;
                else if( broadcast == RADIO_BROADCAST_MAP || broadcast == RADIO_BROADCAST_LOCATION )
                {
                    if( !broadcast_map_id )
                    {
                        Map* map = MapMngr.GetMap( from_map_id, false );
                        if( !map )
                            continue;
                        broadcast_map_id = map->GetId();
                        broadcast_loc_id = map->GetLocation( false )->GetId();
                    }
                }
                else if( !( broadcast >= 101 && broadcast <= 200 ) /*RADIO_BROADCAST_ZONE*/ )
                    continue;
            }
            else
            {
                broadcast = RADIO_BROADCAST_FORCE_ALL;
            }

            if( radio->Accessory == ITEM_ACCESSORY_CRITTER )
            {
                Client* cl = CrMngr.GetPlayer( radio->AccCritter.Id, false );
                if( cl && cl->RadioMessageSended != msg_count )
                {
                    if( broadcast != RADIO_BROADCAST_FORCE_ALL )
                    {
                        if( broadcast == RADIO_BROADCAST_MAP )
                        {
                            if( broadcast_map_id != cl->GetMapId() )
                                continue;
                        }
                        else if( broadcast == RADIO_BROADCAST_LOCATION )
                        {
                            Map* map = MapMngr.GetMap( cl->GetMapId(), false );
                            if( !map || broadcast_loc_id != map->GetLocation( false )->GetId() )
                                continue;
                        }
                        else if( broadcast >= 101 && broadcast <= 200 )                   // RADIO_BROADCAST_ZONE
                        {
                            if( !MapMngr.IsIntersectZone( from_wx, from_wy, 0, cl->Data.WorldX, cl->Data.WorldY, 0, broadcast - 101 ) )
                                continue;
                        }
                        else
                            continue;
                    }

                    if( text )
                        cl->Send_TextEx( radio->GetId(), text, text_len, SAY_RADIO, intellect, unsafe_text );
                    else if( lexems )
                        cl->Send_TextMsgLex( radio->GetId(), num_str, SAY_RADIO, text_msg, lexems );
                    else
                        cl->Send_TextMsg( radio->GetId(), num_str, SAY_RADIO, text_msg );

                    cl->RadioMessageSended = msg_count;
                }
            }
            else if( radio->Accessory == ITEM_ACCESSORY_HEX )
            {
                if( broadcast == RADIO_BROADCAST_MAP && broadcast_map_id != radio->AccHex.MapId )
                    continue;

                Map* map = MapMngr.GetMap( radio->AccHex.MapId, false );
                if( map )
                {
                    if( broadcast != RADIO_BROADCAST_FORCE_ALL && broadcast != RADIO_BROADCAST_MAP )
                    {
                        if( broadcast == RADIO_BROADCAST_LOCATION )
                        {
                            Location* loc = map->GetLocation( false );
                            if( broadcast_loc_id != loc->GetId() )
                                continue;
                        }
                        else if( broadcast >= 101 && broadcast <= 200 )                   // RADIO_BROADCAST_ZONE
                        {
                            Location* loc = map->GetLocation( false );
                            if( !MapMngr.IsIntersectZone( from_wx, from_wy, 0, loc->Data.WX, loc->Data.WY, loc->GetRadius(), broadcast - 101 ) )
                                continue;
                        }
                        else
                            continue;
                    }

                    if( text )
                        map->SetText( radio->AccHex.HexX, radio->AccHex.HexY, 0xFFFFFFFE, text, text_len, intellect, unsafe_text );
                    else if( lexems )
                        map->SetTextMsgLex( radio->AccHex.HexX, radio->AccHex.HexY, 0xFFFFFFFE, text_msg, num_str, lexems, Str::Length( lexems ) );
                    else
                        map->SetTextMsg( radio->AccHex.HexX, radio->AccHex.HexY, 0xFFFFFFFE, text_msg, num_str );
                }
            }
        }
    }
}
#endif // FONLINE_SERVER

void ItemManager::ChangeItemStatistics( hash pid, int val )
{
    SCOPE_LOCK( itemCountLocker );

    ProtoItem* proto = GetProtoItem( pid );
    if( proto )
        proto->InstanceCount += (int64) val;
}

int64 ItemManager::GetItemStatistics( hash pid )
{
    SCOPE_LOCK( itemCountLocker );

    ProtoItem* proto = GetProtoItem( pid );
    return proto ? proto->InstanceCount : 0;
}

string ItemManager::GetItemsStatistics()
{
    itemCountLocker.Lock();

    vector< ProtoItem* > protos;
    protos.reserve( allProtos.size() );
    for( auto it = allProtos.begin(), end = allProtos.end(); it != end; ++it )
        protos.push_back( it->second );

    itemCountLocker.Unlock();

    std::sort( protos.begin(), protos.end(), [] ( ProtoItem * p1, ProtoItem * p2 )
               {
                   return strcmp( p1->GetName(), p2->GetName() ) < 0;
               } );

    string result = "Name                                     Count\n";
    char   str[ MAX_FOTEXT ];
    for( auto it = protos.begin(), end = protos.end(); it != end; ++it )
    {
        ProtoItem* proto_item = *it;
        if( proto_item->IsItem() )
        {
            const char* s = Str::GetName( proto_item->ProtoId );
            Str::Format( str, "%-40s %-20s\n", s ? s : Str::ItoA( proto_item->ProtoId ), Str::I64toA( proto_item->InstanceCount ) );
            result += str;
        }
    }
    return result;
}
