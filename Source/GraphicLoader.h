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
    static Frame*   LoadModel( const char* fname );
    static AnimSet* LoadAnimation( const char* anim_fname, const char* anim_name );
    static void     Free( Frame* frame );
    static bool     IsExtensionSupported( const char* ext );

private:
    static PCharVec processedFiles;
    static FrameVec loadedModels;
    static PCharVec loadedAnimationsFNames;
    static PtrVec   loadedAnimations;   // Pointers of AnimSet

    static Frame* FillNode( aiScene* scene, aiNode* node );
    static void   FixFrame( Frame* root_frame, Frame* frame, aiScene* scene, aiNode* node );

    // Textures
public:
    static Texture* LoadTexture( const char* texture_name, const char* model_path );
    static void     FreeTexture( Texture* texture );   // If texture is NULL than free all textures

private:
    static TextureVec loadedTextures;

    // Effects
public:
    static Effect* LoadEffect( const char* effect_name, bool use_in_2d, const char* defines = NULL, const char* model_path = NULL, EffectDefault* defaults = NULL, uint defaults_count = 0 );
    static void    EffectProcessVariables( Effect* effect, int pass, float anim_proc = 0.0f, float anim_time = 0.0f, Texture** textures = NULL );
    static bool    EffectsPreRestore();
    static bool    EffectsPostRestore();
    static bool    LoadDefaultEffects();

private:
    static EffectVec loadedEffects;
};

#endif // __GRAPHIC_LOADER__
