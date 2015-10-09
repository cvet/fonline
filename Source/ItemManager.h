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
    ProtoItemMap allProtos;

public:
    bool Init();
    void Finish();
    void Clear();
    bool LoadProtos();

    #ifdef FONLINE_OBJECT_EDITOR
    void SaveProtos( const char* full_path );
    void SaveProtos( ProtoItemVec& protos, const char* full_path );
    #endif

    ProtoItem*    GetProtoItem( hash pid );
    ProtoItemMap& GetAllProtos();
    void          GetBinaryData( UCharVec& data );
    void          SetBinaryData( UCharVec& data );

    #ifdef FONLINE_SERVER
public:
    void GetGameItems( ItemVec& items );
    uint GetItemsCount();
    void SetCritterItems( Critter* cr );

    Item* CreateItem( hash pid, uint count = 0 );
    bool  RestoreItem( uint id, hash pid, Properties& props, uchar acc, char* acc_buf );
    void  DeleteItem( Item* item );

    Item* SplitItem( Item* item, uint count );
    Item* GetItem( uint item_id, bool sync_lock );

    void EraseItemHolder( Item* item );
    void MoveItem( Item* item, uint count, Critter* to_cr );
    void MoveItem( Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy );
    void MoveItem( Item* item, uint count, Item* to_cont, uint stack_id );

    Item* AddItemContainer( Item* cont, hash pid, uint count, uint stack_id );
    Item* AddItemCritter( Critter* cr, hash pid, uint count );
    bool  SubItemCritter( Critter* cr, hash pid, uint count, ItemVec* erased_items = nullptr );
    bool  SetItemCritter( Critter* cr, hash pid, uint count );
    bool  MoveItemCritters( Critter* from_cr, Critter* to_cr, uint item_id, uint count );
    bool  MoveItemCritterToCont( Critter* from_cr, Item* to_cont, uint item_id, uint count, uint stack_id );
    bool  MoveItemCritterFromCont( Item* from_cont, Critter* to_cr, uint item_id, uint count );
    bool  MoveItemsContainers( Item* from_cont, Item* to_cont, uint from_stack_id, uint to_stack_id );
    bool  MoveItemsContToCritter( Item* from_cont, Critter* to_cr, uint stack_id );

    // Radio
private:
    ItemVec radioItems;
    Mutex   radioItemsLocker;

public:
    void RadioRegister( Item* radio, bool add );
    void RadioSendText( Critter* cr, const char* text, ushort text_len, bool unsafe_text, ushort text_msg, uint num_str, UShortVec& channels );
    void RadioSendTextEx( ushort channel, int broadcast_type, uint from_map_id, ushort from_wx, ushort from_wy, const char* text, ushort text_len, ushort intellect, bool unsafe_text, ushort text_msg, uint num_str, const char* lexems );
    #endif // FONLINE_SERVER

    // Items statistics
private:
    MutexSpinlock itemCountLocker;

public:
    void   ChangeItemStatistics( hash pid, int val );
    int64  GetItemStatistics( hash pid );
    string GetItemsStatistics();

    ItemManager() { MEMORY_PROCESS( MEMORY_STATIC, sizeof( ItemManager ) ); };
};

extern ItemManager ItemMngr;

#endif // __ITEM_MANAGER__
