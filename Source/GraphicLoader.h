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
    static Node*    LoadModel( const char* fname );
    static void     DestroyModel( Node* node );
    static AnimSet* LoadAnimation( const char* anim_fname, const char* anim_name );
    static bool     IsExtensionSupported( const char* ext );

private:
    static StrVec  processedFiles;
    static NodeVec loadedModels;
    static StrVec  loadedModelNames;
    static PtrVec  loadedAnimations;   // Pointers of AnimSet

    // Textures
public:
    static MeshTexture* LoadTexture( const char* texture_name, const char* model_path );
    static void         DestroyTextures();

private:
    static MeshTextureVec loadedMeshTextures;

    // Effects
public:
    static Effect* LoadEffect( const char* effect_name, bool use_in_2d, const char* defines = NULL, const char* model_path = NULL, EffectDefault* defaults = NULL, uint defaults_count = 0 );
    static void    EffectProcessVariables( Effect* effect, int pass, float anim_proc = 0.0f, float anim_time = 0.0f, MeshTexture** textures = NULL );
    static bool    LoadDefaultEffects();

private:
    static EffectVec loadedEffects;

    // Images
    // All input/output data is in RGBA format
public:
    static uchar* LoadPNG( const uchar* data, uint data_size, uint& result_width, uint& result_height );
    static void   SavePNG( const char* fname, uchar* data, uint width, uint height );
    static uchar* LoadTGA( const uchar* data, uint data_size, uint& result_width, uint& result_height );
};

#endif // __GRAPHIC_LOADER__
