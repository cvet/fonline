#ifndef __ITEM_MANAGER__
#define __ITEM_MANAGER__

#include "Defines.h"
#include "Item.h"

#ifdef FONLINE_SERVER
class Critter;
class Map;
#endif

class ItemManager
{
private:
    bool         isActive;
    ProtoItem    allProto[ MAX_ITEM_PROTOTYPES ]; // All
    ProtoItemVec typeProto[ ITEM_MAX_TYPES ];     // By type
    uint         protoHash[ ITEM_MAX_TYPES ];     // Hash types protos
    char*        protoScript[ MAX_ITEM_PROTOTYPES ];

public:
    ProtoItemVec& GetProtos( int type )     { return typeProto[ type ]; }
    uint          GetProtosHash( int type ) { return protoHash[ type ]; }

    bool Init();
    bool IsInit() { return isActive; }
    void Finish();
    void Clear();

    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_OBJECT_EDITOR ) || defined ( FONLINE_MAPPER )
    bool LoadProtos();
    bool LoadProtos( ProtoItemVec& protos, const char* fname );
    #endif

    #ifdef FONLINE_OBJECT_EDITOR
    void SaveProtos( const char* full_path );
    void SaveProtos( ProtoItemVec& protos, const char* full_path );
    #endif

    void        ParseProtos( ProtoItemVec& protos, const char* collection_name = NULL );
    ProtoItem*  GetProtoItem( ushort pid );
    ProtoItem*  GetAllProtos();
    void        GetCopyAllProtos( ProtoItemVec& protos );
    bool        IsInitProto( ushort pid );
    const char* GetProtoScript( ushort pid );
    void        ClearProtos( int type = 0xFF ); // 0xFF - All
    void        ClearProto( ushort pid );

    #ifdef FONLINE_SERVER
private:
    ItemPtrMap gameItems;
    UIntVec    itemToDelete;
    UIntVec    itemToDeleteCount;
    uint       lastItemId;
    Mutex      itemLocker;

public:
    void SaveAllItemsFile( void ( * save_func )( void*, size_t ) );
    bool LoadAllItemsFile( FILE* f, int version );
    bool CheckProtoFunctions();
    void RunInitScriptItems();

    void GetGameItems( ItemPtrVec& items );
    uint GetItemsCount();
    void SetCritterItems( Critter* cr );
    void GetItemIds( UIntSet& item_ids );

    Item* CreateItem( ushort pid, uint count, uint item_id = 0 );
    Item* SplitItem( Item* item, uint count );
    Item* GetItem( uint item_id, bool sync_lock );

    void ItemToGarbage( Item* item );
    void ItemGarbager();

    void NotifyChangeItem( Item* item );

    void EraseItemHolder( Item* item );
    void MoveItem( Item* item, uint count, Critter* to_cr );
    void MoveItem( Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy );
    void MoveItem( Item* item, uint count, Item* to_cont, uint stack_id );

    Item* AddItemContainer( Item* cont, ushort pid, uint count, uint stack_id );
    Item* AddItemCritter( Critter* cr, ushort pid, uint count );
    bool  SubItemCritter( Critter* cr, ushort pid, uint count, ItemPtrVec* erased_items = NULL );
    bool  SetItemCritter( Critter* cr, ushort pid, uint count );
    bool  MoveItemCritters( Critter* from_cr, Critter* to_cr, uint item_id, uint count );
    bool  MoveItemCritterToCont( Critter* from_cr, Item* to_cont, uint item_id, uint count, uint stack_id );
    bool  MoveItemCritterFromCont( Item* from_cont, Critter* to_cr, uint item_id, uint count );
    bool  MoveItemsContainers( Item* from_cont, Item* to_cont, uint from_stack_id, uint to_stack_id );
    bool  MoveItemsContToCritter( Item* from_cont, Critter* to_cr, uint stack_id );

    // Radio
private:
    ItemPtrVec radioItems;
    Mutex      radioItemsLocker;

public:
    void RadioRegister( Item* radio, bool add );
    void RadioSendText( Critter* cr, const char* text, ushort text_len, bool unsafe_text, ushort text_msg, uint num_str, UShortVec& channels );
    void RadioSendTextEx( ushort channel, int broadcast_type, uint from_map_id, ushort from_wx, ushort from_wy, const char* text, ushort text_len, ushort intellect, bool unsafe_text, ushort text_msg, uint num_str, const char* lexems );
    #endif // FONLINE_SERVER

    // Items statistics
private:
    int64         itemCount[ MAX_ITEM_PROTOTYPES ];
    MutexSpinlock itemCountLocker;

public:
    void   AddItemStatistics( ushort pid, uint val );
    void   SubItemStatistics( ushort pid, uint val );
    int64  GetItemStatistics( ushort pid );
    string GetItemsStatistics();

    ItemManager(): isActive( false ) { MEMORY_PROCESS( MEMORY_STATIC, sizeof( ItemManager ) ); };
};

extern ItemManager ItemMngr;

#endif // __ITEM_MANAGER__
