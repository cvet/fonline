#ifndef __GRAPHIC_LOADER__
#define __GRAPHIC_LOADER__

#include "Common.h"
#include "Defines.h"
#include "FileManager.h"
#include "GraphicStructures.h"

class AnimSet;

struct aiNode;
struct aiScene;

class GraphicLoader
{
    // Models
public:
    static Frame*   LoadModel( Device_ device, const char* fname, bool calc_tangent );
    static AnimSet* LoadAnimation( Device_ device, const char* anim_fname, const char* anim_name );
    static void     Free( Frame* frame );
    static bool     IsExtensionSupported( const char* ext );

private:
    static PCharVec processedFiles;
    static FrameVec loadedModels;
    static PCharVec loadedAnimationsFNames;
    static PtrVec   loadedAnimations;   // Pointers of AnimSet

    static Frame* FillNode( Device_ device, const aiNode* node, const aiScene* scene, bool with_tangent );

    // Textures
public:
    static Texture* LoadTexture( Device_ device, const char* texture_name, const char* model_path );
    static void     FreeTexture( Texture* texture );   // If texture is NULL than free all textures

private:
    static TextureVec loadedTextures;

    // Effects
public:
    static Effect* LoadEffect( Device_ device, const char* effect_name );
    static Effect* LoadEffect( Device_ device, EffectInstance* effect_inst, const char* model_path );
    static void    EffectProcessVariables( Effect* effect, int pass, float anim_proc = 0.0f, float anim_time = 0.0f, Texture** textures = NULL );
    static bool    EffectsPreRestore();
    static bool    EffectsPostRestore();

private:
    static EffectVec loadedEffects;
};

#endif // __GRAPHIC_LOADER__
