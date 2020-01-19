#pragma once

#include "Common.h"

#include "Entity.h"

class ProtoManager;
class EntityManager;
class MapManager;
class CritterManager;

class ItemManager
{
public:
    ItemManager(ProtoManager& proto_mngr, EntityManager& entity_mngr, MapManager& map_mngr, CritterManager& cr_mngr);

    void GetGameItems(ItemVec& items);
    uint GetItemsCount();
    void SetCritterItems(Critter* cr);

    Item* CreateItem(hash pid, uint count = 0, Properties* props = nullptr);
    bool RestoreItem(uint id, hash proto_id, const DataBase::Document& doc);
    void DeleteItem(Item* item);

    Item* SplitItem(Item* item, uint count);
    Item* GetItem(uint item_id);

    void MoveItem(Item* item, uint count, Critter* to_cr, bool skip_checks);
    void MoveItem(Item* item, uint count, Map* to_map, ushort to_hx, ushort to_hy, bool skip_checks);
    void MoveItem(Item* item, uint count, Item* to_cont, uint stack_id, bool skip_checks);

    Item* AddItemContainer(Item* cont, hash pid, uint count, uint stack_id);
    Item* AddItemCritter(Critter* cr, hash pid, uint count);
    bool SubItemCritter(Critter* cr, hash pid, uint count, ItemVec* erased_items = nullptr);
    bool SetItemCritter(Critter* cr, hash pid, uint count);

    void AddItemToContainer(Item* cont, Item*& item, uint stack_id);
    void EraseItemFromContainer(Item* cont, Item* item);
    void SetItemToContainer(Item* cont, Item* item);

    bool ItemCheckMove(Item* item, uint count, Entity* from, Entity* to);
    bool MoveItemCritters(Critter* from_cr, Critter* to_cr, Item* item, uint count);
    bool MoveItemCritterToCont(Critter* from_cr, Item* to_cont, Item* item, uint count, uint stack_id);
    bool MoveItemCritterFromCont(Item* from_cont, Critter* to_cr, Item* item, uint count);

    void RadioClear();
    void RadioRegister(Item* radio, bool add);
    void RadioSendText(
        Critter* cr, const string& text, bool unsafe_text, ushort text_msg, uint num_str, UShortVec& channels);
    void RadioSendTextEx(ushort channel, int broadcast_type, uint from_map_id, ushort from_wx, ushort from_wy,
        const string& text, bool unsafe_text, ushort text_msg, uint num_str, const char* lexems);

    void ChangeItemStatistics(hash pid, int val);
    int64 GetItemStatistics(hash pid);
    string GetItemsStatistics();

private:
    Entity* GetItemHolder(Item* item);
    void EraseItemHolder(Item* item, Entity* parent);

    ProtoManager& protoMngr;
    EntityManager& entityMngr;
    MapManager& mapMngr;
    CritterManager& crMngr;
    ItemVec radioItems;
};
