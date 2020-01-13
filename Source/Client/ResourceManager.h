#pragma once

#include "Common.h"
#include "GraphicStructures.h"
#include "SpriteManager.h"

class Animation3d;

struct LoadedAnim
{
    AtlasType ResType {};
    AnyFrames* Anim {};
};
typedef map<hash, LoadedAnim> LoadedAnimMap;

class ResourceManager : public NonCopyable
{
public:
    ResourceManager(SpriteManager& spr_mngr);
    ~ResourceManager();

    void Refresh();
    void FreeResources(AtlasType atlas_type);
    void ReinitializeDynamicAtlas();

    AnyFrames* GetAnim(hash name_hash, AtlasType atlas_type);
    AnyFrames* GetIfaceAnim(hash name_hash) { return GetAnim(name_hash, AtlasType::Static); }
    AnyFrames* GetInvAnim(hash name_hash) { return GetAnim(name_hash, AtlasType::Static); }
    AnyFrames* GetSkDxAnim(hash name_hash) { return GetAnim(name_hash, AtlasType::Static); }
    AnyFrames* GetItemAnim(hash name_hash) { return GetAnim(name_hash, AtlasType::Dynamic); }

    AnyFrames* GetCrit2dAnim(hash model_name, uint anim1, uint anim2, int dir);
    Animation3d* GetCrit3dAnim(hash model_name, uint anim1, uint anim2, int dir, int* layers3d = nullptr);
    uint GetCritSprId(hash model_name, uint anim1, uint anim2, int dir, int* layers3d = nullptr);

    AnyFrames* GetRandomSplash();
    StrMap& GetSoundNames() { return soundNames; }
    SpriteManager& GetSpriteManager() { return sprMngr; }

    AnyFrames* ItemHexDefaultAnim {};
    AnyFrames* CritterDefaultAnim {};

private:
    AnyFrames* LoadFalloutAnim(hash model_name, uint anim1, uint anim2);
    AnyFrames* LoadFalloutAnimSpr(hash model_name, uint anim1, uint anim2);
    void FixAnimOffs(AnyFrames* frames_base, AnyFrames* stay_frm_base);
    void FixAnimOffsNext(AnyFrames* frames_base, AnyFrames* stay_frm_base);

    SpriteManager& sprMngr;
    vector<DataFile*> processedDats {};
    UIntStrMap namesHash {};
    LoadedAnimMap loadedAnims {};
    AnimMap critterFrames {};
    map<hash, Animation3d*> critter3d {};
    StrVec splashNames {};
    StrMap soundNames {};
};
