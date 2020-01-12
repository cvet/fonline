#pragma once

#include "Common.h"
#include "GraphicStructures.h"

class AnimSet;
using AnimSetVec = vector<AnimSet*>;
class SpriteManager;

class GraphicLoader
{
public:
    GraphicLoader(SpriteManager& spr_mngr);
    ~GraphicLoader();

    AnyFrames* CreateAnyFrames(uint frames, uint ticks);
    void CreateAnyFramesDirAnims(AnyFrames* anim);
    void DestroyAnyFrames(AnyFrames* anim);

    Bone* LoadModel(const string& fname);
    void DestroyModel(Bone* root_bone);
    AnimSet* LoadAnimation(const string& anim_fname, const string& anim_name);

    MeshTexture* LoadTexture(const string& texture_name, const string& model_path);
    void DestroyTextures();

    AnyFrames* DummyAnim {};

private:
    SpriteManager& sprMngr;
    MemoryPool<sizeof(AnyFrames), ANY_FRAMES_POOL_SIZE> anyFramesPool {};
    StrVec processedFiles {};
    BoneVec loadedModels {};
    StrVec loadedModelNames {};
    AnimSetVec loadedAnimations {};
    MeshTextureVec loadedMeshTextures {};
};
