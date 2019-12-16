#pragma once

#include "Common.h"

class AnimSet;
using AnimSetVec = vector<AnimSet*>;
struct Bone;
using BoneVec = vector<Bone*>;
struct MeshTexture;
using MeshTextureVec = vector<MeshTexture*>;
struct Effect;
using EffectVec = vector<Effect*>;
struct EffectDefault;
struct EffectPass;
class File;
class SpriteManager;

class GraphicLoader
{
public:
    GraphicLoader(SpriteManager& spr_mngr);
    ~GraphicLoader();

    Bone* LoadModel(const string& fname);
    void DestroyModel(Bone* root_bone);
    AnimSet* LoadAnimation(const string& anim_fname, const string& anim_name);

    MeshTexture* LoadTexture(const string& texture_name, const string& model_path);
    void DestroyTextures();

    Effect* LoadEffect(const string& effect_name, bool use_in_2d, const string& defines = "",
        const string& model_path = "", EffectDefault* defaults = nullptr, uint defaults_count = 0);
    void EffectProcessVariables(EffectPass& effect_pass, bool start, float anim_proc = 0.0f, float anim_time = 0.0f,
        MeshTexture** textures = nullptr);
    bool LoadMinimalEffects();
    bool LoadDefaultEffects();
    bool Load3dEffects();

private:
    bool LoadEffectPass(Effect* effect, const string& fname, File& file, uint pass, bool use_in_2d,
        const string& defines, EffectDefault* defaults, uint defaults_count);

    SpriteManager& sprMngr;
    StrVec processedFiles;
    BoneVec loadedModels;
    StrVec loadedModelNames;
    AnimSetVec loadedAnimations;
    MeshTextureVec loadedMeshTextures;
    EffectVec loadedEffects;
};
