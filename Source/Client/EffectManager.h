#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "GraphicStructures.h"

struct EffectCollection : public NonCopyable
{
    Effect* Contour {};
    Effect* ContourDefault {};
    Effect* Generic {};
    Effect* GenericDefault {};
    Effect* Critter {};
    Effect* CritterDefault {};
    Effect* Tile {};
    Effect* TileDefault {};
    Effect* Roof {};
    Effect* RoofDefault {};
    Effect* Rain {};
    Effect* RainDefault {};
    Effect* Iface {};
    Effect* IfaceDefault {};
    Effect* Primitive {};
    Effect* PrimitiveDefault {};
    Effect* Light {};
    Effect* LightDefault {};
    Effect* Fog {};
    Effect* FogDefault {};
    Effect* FlushRenderTarget {};
    Effect* FlushRenderTargetDefault {};
    Effect* FlushRenderTargetMS {};
    Effect* FlushRenderTargetMSDefault {};
    Effect* FlushPrimitive {};
    Effect* FlushPrimitiveDefault {};
    Effect* FlushMap {};
    Effect* FlushMapDefault {};
    Effect* FlushLight {};
    Effect* FlushLightDefault {};
    Effect* FlushFog {};
    Effect* FlushFogDefault {};
    Effect* Font {};
    Effect* FontDefault {};
    Effect* Skinned3d {};
    Effect* Skinned3dDefault {};
};

class EffectManager : public NonCopyable
{
public:
    EffectManager(FileManager& file_mngr);
    Effect* LoadEffect(const string& effect_name, bool use_in_2d, const string& defines = "",
        const string& model_path = "", EffectDefault* defaults = nullptr, uint defaults_count = 0);
    void EffectProcessVariables(EffectPass& effect_pass, bool start, float anim_proc = 0.0f, float anim_time = 0.0f,
        MeshTexture** textures = nullptr);
    bool LoadMinimalEffects();
    bool LoadDefaultEffects();
    bool Load3dEffects();

    EffectCollection Effects {};
    uint MaxBones {};

private:
    bool LoadEffectPass(Effect* effect, const string& fname, File& file, uint pass, bool use_in_2d,
        const string& defines, EffectDefault* defaults, uint defaults_count);

    FileManager& fileMngr;
    vector<unique_ptr<Effect>> loadedEffects {};
};
