#include "GraphicLoader.h"
#include "3dAnimation.h"
#include "Crypt.h"
#include "GraphicStructures.h"
#include "Log.h"
#include "ResourceManager.h"
#include "SpriteManager.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"
#include "Version_Include.h"

GraphicLoader::GraphicLoader(SpriteManager& spr_mngr) : sprMngr(spr_mngr)
{
}

GraphicLoader::~GraphicLoader()
{
    for (auto* model : loadedModels)
        delete model;
    for (auto* anim : loadedAnimations)
        delete anim;
    for (auto* tex : loadedMeshTextures)
        delete tex;
}

AnyFrames* GraphicLoader::CreateAnyFrames(uint frames, uint ticks)
{
    AnyFrames* anim = (AnyFrames*)anyFramesPool.Get();
    memzero(anim, sizeof(AnyFrames));
    anim->CntFrm = MIN(frames, MAX_FRAMES);
    anim->Ticks = (ticks ? ticks : frames * 100);
    anim->HaveDirs = false;
    return anim;
}

void GraphicLoader::CreateAnyFramesDirAnims(AnyFrames* anim)
{
    anim->HaveDirs = true;
    for (int dir = 0; dir < DIRS_COUNT - 1; dir++)
        anim->Dirs[dir] = CreateAnyFrames(anim->CntFrm, anim->Ticks);
}

void GraphicLoader::DestroyAnyFrames(AnyFrames* anim)
{
    if (!anim || anim == DummyAnim)
        return;

    for (int dir = 1; dir < anim->DirCount(); dir++)
        anyFramesPool.Put(anim->GetDir(dir));
    anyFramesPool.Put(anim);
}

Bone* GraphicLoader::LoadModel(const string& fname)
{
    // Find already loaded
    for (size_t i = 0, j = loadedModelNames.size(); i < j; i++)
        if (_str(loadedModelNames[i]).compareIgnoreCase(fname))
            return loadedModels[i];

    // Add to already processed
    for (size_t i = 0, j = processedFiles.size(); i < j; i++)
        if (_str(processedFiles[i]).compareIgnoreCase(fname))
            return nullptr;
    processedFiles.push_back(fname);

    // Load file data
    File file;
    if (!file.LoadFile(fname))
    {
        WriteLog("3d file '{}' not found.\n", fname);
        return nullptr;
    }

    // Load bones
    Bone* root_bone = new Bone();
    root_bone->Load(file);
    root_bone->FixAfterLoad(root_bone);

    // Load animations
    uint anim_sets_count = file.GetBEUInt();
    for (uint i = 0; i < anim_sets_count; i++)
    {
        AnimSet* anim_set = new AnimSet();
        anim_set->Load(file);
        loadedAnimations.push_back(anim_set);
    }

    // Add to collection
    loadedModels.push_back(root_bone);
    loadedModelNames.push_back(fname);
    return root_bone;
}

void GraphicLoader::DestroyModel(Bone* root_bone)
{
    for (size_t i = 0, j = loadedModels.size(); i < j; i++)
    {
        if (loadedModels[i] == root_bone)
        {
            processedFiles.erase(std::find(processedFiles.begin(), processedFiles.end(), loadedModelNames[i]));
            loadedModels.erase(loadedModels.begin() + i);
            loadedModelNames.erase(loadedModelNames.begin() + i);
            break;
        }
    }
    delete root_bone;
}

AnimSet* GraphicLoader::LoadAnimation(const string& anim_fname, const string& anim_name)
{
    // Find in already loaded
    bool take_first = anim_name == "Base";
    for (size_t i = 0; i < loadedAnimations.size(); i++)
    {
        AnimSet* anim = loadedAnimations[i];
        if (_str(anim->GetFileName()).compareIgnoreCase(anim_fname) &&
            (take_first || _str(anim->GetName()).compareIgnoreCase(anim_name)))
            return anim;
    }

    // Check maybe file already processed and nothing founded
    for (size_t i = 0; i < processedFiles.size(); i++)
        if (_str(processedFiles[i]).compareIgnoreCase(anim_fname))
            return nullptr;

    // File not processed, load and recheck animations
    if (LoadModel(anim_fname))
        return LoadAnimation(anim_fname, anim_name);

    return nullptr;
}

MeshTexture* GraphicLoader::LoadTexture(const string& texture_name, const string& model_path)
{
    if (texture_name.empty())
        return nullptr;

    // Try find already loaded texture
    for (auto it = loadedMeshTextures.begin(), end = loadedMeshTextures.end(); it != end; ++it)
    {
        MeshTexture* texture = *it;
        if (_str(texture->Name).compareIgnoreCase(texture_name))
            return texture && texture->Id ? texture : nullptr;
    }

    // Allocate structure
    MeshTexture* mesh_tex = new MeshTexture();
    mesh_tex->Name = texture_name;
    mesh_tex->Id = 0;
    loadedMeshTextures.push_back(mesh_tex);

    // First try load from textures folder
    sprMngr.PushAtlasType(RES_ATLAS_TEXTURES);
    AnyFrames* anim = sprMngr.LoadAnimation(_str(model_path).extractDir() + texture_name);
    sprMngr.PopAtlasType();
    if (!anim)
        return nullptr;

    SpriteInfo* si = sprMngr.GetSpriteInfo(anim->Ind[0]);
    mesh_tex->Id = si->Atlas->TextureOwner->Id;
    memcpy(mesh_tex->SizeData, si->Atlas->TextureOwner->SizeData, sizeof(mesh_tex->SizeData));
    mesh_tex->AtlasOffsetData[0] = si->SprRect[0];
    mesh_tex->AtlasOffsetData[1] = si->SprRect[1];
    mesh_tex->AtlasOffsetData[2] = si->SprRect[2] - si->SprRect[0];
    mesh_tex->AtlasOffsetData[3] = si->SprRect[3] - si->SprRect[1];
    DestroyAnyFrames(anim);
    return mesh_tex;
}

void GraphicLoader::DestroyTextures()
{
    for (auto it = loadedMeshTextures.begin(), end = loadedMeshTextures.end(); it != end; ++it)
        delete *it;
    loadedMeshTextures.clear();
}
