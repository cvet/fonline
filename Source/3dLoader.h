#ifndef __3D_LOADER__
#define __3D_LOADER__

#include "Common.h"
#include "Defines.h"
#include "FileManager.h"
#include "3dMeshHierarchy.h"
#include "3dMeshStructures.h"

struct aiNode;
struct aiScene;

class Loader3d
{
    // Models
public:
    static Frame*    LoadModel( Device_ device, const char* fname, bool calc_tangent );
    static AnimSet_* LoadAnimation( Device_ device, const char* anim_fname, const char* anim_name );
    static void      Free( Frame* frame );
    static bool      IsExtensionSupported( const char* ext );

private:
    static PCharVec   processedFiles;
    static FrameVec   loadedModels;
    static PCharVec   loadedAnimationsFNames;
    static AnimSetVec loadedAnimations;

    static Frame* FillNode( Device_ device, const aiNode* node, const aiScene* scene, bool with_tangent );
    static Frame* LoadX( Device_ device, FileManager& fm, const char* fname );

    // Textures
public:
    static Texture* LoadTexture( Device_ device, const char* texture_name, const char* model_path );
    static void     FreeTexture( Texture* texture );   // If texture is NULL than free all textures

private:
    static TextureVec loadedTextures;

    // Effects
public:
    static Effect* LoadEffect( Device_ device, const char* effect_name );
    static Effect* LoadEffect( Device_ device, EffectInstance_* effect_inst, const char* model_path );
    static void    EffectProcessVariables( Effect* effect_ex, int pass, float anim_proc = 0.0f, float anim_time = 0.0f, Texture** textures = NULL );
    static bool    EffectsPreRestore();
    static bool    EffectsPostRestore();

private:
    static EffectExVec loadedEffects;
};

#endif // __3D_LOADER__
