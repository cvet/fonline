#ifndef __RESOURCE_MANAGER__
#define __RESOURCE_MANAGER__

#include "Defines.h"
#include "SpriteManager.h"
#include "FileManager.h"

#define RES_ATLAS_STATIC           ( 1 )
#define RES_ATLAS_DYNAMIC          ( 2 )
#define RES_ATLAS_SPLASH           ( 3 )
#define RES_ATLAS_TEXTURES         ( 4 )

class SpriteManager;
struct SpriteInfo;
struct AnyFrames;

struct LoadedAnim
{
    int        ResType;
    AnyFrames* Anim;
    LoadedAnim( int res_type, AnyFrames* anim ): ResType( res_type ), Anim( anim ) {}
};
typedef map< uint, LoadedAnim, less< uint > > LoadedAnimMap;

class ResourceManager
{
private:
    PtrVec         processedDats;
    UIntStrMap     namesHash;
    LoadedAnimMap  loadedAnims;
    AnimMap        critterFrames;
    Animation3dVec critter3d;
    StrVec         splashNames;
    StrMap         soundNames;

    void       AddNamesHash( StrVec& names );
    void       RegisterCritterAnim( AnyFrames* anim, uint crtype, int anim1, int anim2, bool fallout_spr );
    AnyFrames* LoadFalloutAnim( uint crtype, uint anim1, uint anim2 );
    AnyFrames* LoadFalloutAnimSpr( uint crtype, uint anim1, uint anim2 );

public:
    AnyFrames* ItemHexDefaultAnim;
    AnyFrames* CritterDefaultAnim;

    void Refresh();
    void Finish();
    void FreeResources( int type );
    void ReinitializeDynamicAtlas();

    AnyFrames* GetAnim( uint name_hash, int res_type );
    AnyFrames* GetIfaceAnim( uint name_hash ) { return GetAnim( name_hash, RES_ATLAS_STATIC ); }
    AnyFrames* GetInvAnim( uint name_hash )   { return GetAnim( name_hash, RES_ATLAS_STATIC ); }
    AnyFrames* GetSkDxAnim( uint name_hash )  { return GetAnim( name_hash, RES_ATLAS_STATIC ); }
    AnyFrames* GetItemAnim( uint name_hash )  { return GetAnim( name_hash, RES_ATLAS_DYNAMIC ); }

    AnyFrames*   GetCrit2dAnim( uint crtype, uint anim1, uint anim2, int dir );
    Animation3d* GetCrit3dAnim( uint crtype, uint anim1, uint anim2, int dir, int* layers3d = NULL );
    uint         GetCritSprId( uint crtype, uint anim1, uint anim2, int dir, int* layers3d = NULL );

    AnyFrames* GetRandomSplash();

    StrMap& GetSoundNames() { return soundNames; }
};

extern ResourceManager ResMngr;


#define SKILLDEX_PARAM( index )             ( index )
#define SKILLDEX_PERKS             ( 1000 )
#define SKILLDEX_KILLS             ( 1001 )
#define SKILLDEX_KARMA             ( 1002 )
#define SKILLDEX_TRAITS            ( 1003 )
#define SKILLDEX_REPUTATION        ( 1004 )
#define SKILLDEX_SKILLS            ( 1005 )
#define SKILLDEX_NEXT_LEVEL        ( 1006 )
#define SKILLDEX_DRUG_ADDICT       ( 1007 )
#define SKILLDEX_ALCOHOL_ADDICT    ( 1008 )
#define SKILLDEX_REPUTATION_RATIO( val )    ( 2100 + ( ( val ) >= GameOpt.ReputationLoved ? 0 : ( ( val ) >= GameOpt.ReputationLiked ? 1 : ( ( val ) >= GameOpt.ReputationAccepted ? 2 : ( ( val ) >= GameOpt.ReputationNeutral ? 3 : ( ( val ) >= GameOpt.ReputationAntipathy ? 4 : ( ( val ) >= GameOpt.ReputationHated ? 5 : 6 ) ) ) ) ) ) )

#endif // __RESOURCE_MANAGER__
