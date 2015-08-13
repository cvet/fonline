#ifndef __GRAPHIC_LOADER__
#define __GRAPHIC_LOADER__

#include "Common.h"
#include "Defines.h"
#include "FileManager.h"
#include "GraphicStructures.h"

class AnimSet;

class GraphicLoader
{
    // Models
public:
    static Bone*    LoadModel( const char* fname );
    static void     DestroyModel( Bone* root_bone );
    static AnimSet* LoadAnimation( const char* anim_fname, const char* anim_name );

private:
    static StrVec  processedFiles;
    static BoneVec loadedModels;
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
    static void    EffectProcessVariables( EffectPass& effect_pass, bool start, float anim_proc = 0.0f, float anim_time = 0.0f, MeshTexture** textures = NULL );
    static bool    LoadDefaultEffects();
    static bool    Load3dEffects();

private:
    static EffectVec loadedEffects;

    static bool LoadEffectPass( Effect* effect, const char* fname, FileManager& file, uint pass, bool use_in_2d, const char* defines, EffectDefault* defaults, uint defaults_count );

    // Images
    // All input/output data is in RGBA format
public:
    static uchar* LoadPNG( const uchar* data, uint data_size, uint& result_width, uint& result_height );
    static void   SavePNG( const char* fname, uchar* data, uint width, uint height );
    static uchar* LoadTGA( const uchar* data, uint data_size, uint& result_width, uint& result_height );
};

#endif // __GRAPHIC_LOADER__
