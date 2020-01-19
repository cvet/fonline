#pragma once

#include "Common.h"

#include "Entity.h"

class Item : public Entity
{
    friend class Entity;
    friend class ItemManager;

    ItemVec* childItems = nullptr;

public:
    bool ViewPlaceOnMap = false;
    uint SceneryScriptBindId = 0;
    Critter* ViewByCritter = nullptr;

    PROPERTIES_HEADER();
    CLASS_PROPERTY(bool, Stackable);
    CLASS_PROPERTY(bool, Opened);
    CLASS_PROPERTY(int, Corner);
    CLASS_PROPERTY(uchar, Slot);
    CLASS_PROPERTY(bool, DisableEgg);
    CLASS_PROPERTY(ushort, AnimWaitBase);
    CLASS_PROPERTY(ushort, AnimWaitRndMin);
    CLASS_PROPERTY(ushort, AnimWaitRndMax);
    CLASS_PROPERTY(uchar, AnimStay0);
    CLASS_PROPERTY(uchar, AnimStay1);
    CLASS_PROPERTY(uchar, AnimShow0);
    CLASS_PROPERTY(uchar, AnimShow1);
    CLASS_PROPERTY(uchar, AnimHide0);
    CLASS_PROPERTY(uchar, AnimHide1);
    CLASS_PROPERTY(uchar, SpriteCut);
    CLASS_PROPERTY(char, DrawOrderOffsetHexY);
    CLASS_PROPERTY(CScriptArray*, BlockLines);
    CLASS_PROPERTY(hash, ScriptId);
    CLASS_PROPERTY(int, Accessory); // enum ItemOwnership
    CLASS_PROPERTY(uint, MapId);
    CLASS_PROPERTY(ushort, HexX);
    CLASS_PROPERTY(ushort, HexY);
    CLASS_PROPERTY(uint, CritId);
    CLASS_PROPERTY(uchar, CritSlot);
    CLASS_PROPERTY(uint, ContainerId);
    CLASS_PROPERTY(uint, ContainerStack);
    CLASS_PROPERTY(bool, IsStatic);
    CLASS_PROPERTY(bool, IsScenery);
    CLASS_PROPERTY(bool, IsWall);
    CLASS_PROPERTY(bool, IsCanOpen);
    CLASS_PROPERTY(bool, IsScrollBlock);
    CLASS_PROPERTY(bool, IsHidden);
    CLASS_PROPERTY(bool, IsHiddenPicture);
    CLASS_PROPERTY(bool, IsHiddenInStatic);
    CLASS_PROPERTY(bool, IsFlat);
    CLASS_PROPERTY(bool, IsNoBlock);
    CLASS_PROPERTY(bool, IsShootThru);
    CLASS_PROPERTY(bool, IsLightThru);
    CLASS_PROPERTY(bool, IsAlwaysView);
    CLASS_PROPERTY(bool, IsBadItem);
    CLASS_PROPERTY(bool, IsNoHighlight);
    CLASS_PROPERTY(bool, IsShowAnim);
    CLASS_PROPERTY(bool, IsShowAnimExt);
    CLASS_PROPERTY(bool, IsLight);
    CLASS_PROPERTY(bool, IsGeck);
    CLASS_PROPERTY(bool, IsTrap);
    CLASS_PROPERTY(bool, IsTrigger);
    CLASS_PROPERTY(bool, IsNoLightInfluence);
    CLASS_PROPERTY(bool, IsGag);
    CLASS_PROPERTY(bool, IsColorize);
    CLASS_PROPERTY(bool, IsColorizeInv);
    CLASS_PROPERTY(bool, IsCanTalk);
    CLASS_PROPERTY(bool, IsRadio);
    CLASS_PROPERTY(short, SortValue);
    CLASS_PROPERTY(hash, PicMap);
    CLASS_PROPERTY(hash, PicInv);
    CLASS_PROPERTY(uchar, Mode);
    CLASS_PROPERTY(char, LightIntensity);
    CLASS_PROPERTY(uchar, LightDistance);
    CLASS_PROPERTY(uchar, LightFlags);
    CLASS_PROPERTY(uint, LightColor);
    CLASS_PROPERTY(uint, Count);
    CLASS_PROPERTY(short, TrapValue);
    CLASS_PROPERTY(ushort, RadioChannel);
    CLASS_PROPERTY(ushort, RadioFlags);
    CLASS_PROPERTY(uchar, RadioBroadcastSend);
    CLASS_PROPERTY(uchar, RadioBroadcastRecv);
    CLASS_PROPERTY(short, OffsetX);
    CLASS_PROPERTY(short, OffsetY);
    CLASS_PROPERTY(float, FlyEffectSpeed);

    Item(uint id, ProtoItem* proto);
    ~Item();

    ProtoItem* GetProtoItem() { return (ProtoItem*)Proto; }
    void SetProto(ProtoItem* proto);
    bool SetScript(asIScriptFunction* func, bool first_time);
    void SetSortValue(ItemVec& items);
    void ChangeCount(int val);

    Item* ContGetItem(uint item_id, bool skip_hide);
    void ContGetAllItems(ItemVec& items, bool skip_hide);
    Item* ContGetItemByPid(hash pid, uint stack_id);
    void ContGetItems(ItemVec& items, uint stack_id);
    bool ContIsItems();

    bool IsStatic() { return GetIsStatic(); }
    bool IsAnyScenery() { return IsScenery() || IsWall(); }
    bool IsScenery() { return GetIsScenery(); }
    bool IsWall() { return GetIsWall(); }
    bool RadioIsSendActive() { return !FLAG(GetRadioFlags(), RADIO_DISABLE_SEND); }
    bool RadioIsRecvActive() { return !FLAG(GetRadioFlags(), RADIO_DISABLE_RECV); }
};
