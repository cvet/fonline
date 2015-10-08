#include "Common.h"
#include "ResourceManager.h"
#include "FileManager.h"
#include "DataFile.h"
#include "CritterType.h"
#include "Script.h"
#include "Crypt.h"

ResourceManager ResMngr;

void ResourceManager::Refresh()
{
    // Dat files, packed data
    DataFileVec& data_files = FileManager::GetDataFiles();
    for( auto it = data_files.begin(), end = data_files.end(); it != end; ++it )
    {
        DataFile* data_file = *it;
        if( std::find( processedDats.begin(), processedDats.end(), data_file ) == processedDats.end() )
        {
            // All names
            StrVec file_names;
            data_file->GetFileNames( FileManager::GetDataPath( "", PT_DATA ), true, NULL, file_names );
            for( auto it = file_names.begin(), end = file_names.end(); it != end; ++it )
                Str::GetHash( it->c_str() );

            // Splashes
            StrVec splashes;
            data_file->GetFileNames( FileManager::GetDataPath( "", PT_ART_SPLASH ), true, "rix", splashes );
            data_file->GetFileNames( FileManager::GetDataPath( "", PT_ART_SPLASH ), true, "png", splashes );
            data_file->GetFileNames( FileManager::GetDataPath( "", PT_ART_SPLASH ), true, "jpg", splashes );
            for( auto it = splashes.begin(), end = splashes.end(); it != end; ++it )
                if( std::find( splashNames.begin(), splashNames.end(), *it ) == splashNames.end() )
                    splashNames.push_back( *it );

            // Sound names
            StrVec sounds;
            data_file->GetFileNames( FileManager::GetDataPath( "", PT_SND_SFX ), true, "wav", sounds );
            data_file->GetFileNames( FileManager::GetDataPath( "", PT_SND_SFX ), true, "acm", sounds );
            data_file->GetFileNames( FileManager::GetDataPath( "", PT_SND_SFX ), true, "ogg", sounds );
            char fname[ MAX_FOPATH ];
            char name[ MAX_FOPATH ];
            for( auto it = sounds.begin(), end = sounds.end(); it != end; ++it )
            {
                FileManager::ExtractFileName( ( *it ).c_str(), fname );
                Str::Copy( name, fname );
                Str::Upper( name );
                char* ext = (char*) FileManager::GetExtension( name );
                if( !ext )
                    continue;
                *( --ext ) = 0;
                if( name[ 0 ] )
                    soundNames.insert( PAIR( string( name ), string( fname ) ) );
            }

            processedDats.push_back( data_file );
        }
    }
}

void ResourceManager::Finish()
{
    WriteLog( "Resource manager finish...\n" );
    loadedAnims.clear();
    WriteLog( "Resource manager finish complete.\n" );
}

void ResourceManager::FreeResources( int type )
{
    SprMngr.DestroyAtlases( type );
    for( auto it = loadedAnims.begin(); it != loadedAnims.end();)
    {
        int res_type = ( *it ).second.ResType;
        if( res_type == type )
        {
            AnyFrames::Destroy( ( *it ).second.Anim );
            it = loadedAnims.erase( it );
        }
        else
        {
            ++it;
        }
    }

    if( type == RES_ATLAS_STATIC )
    {
        SprMngr.ClearFonts();
    }

    if( type == RES_ATLAS_DYNAMIC )
    {
        // 2D critters
        for( auto it = critterFrames.begin(), end = critterFrames.end(); it != end; ++it )
            AnyFrames::Destroy( ( *it ).second );
        critterFrames.clear();

        // 3D textures
        #ifndef RES_ATLAS_TEXTURES
        GraphicLoader::DestroyTextures();
        #endif
    }
}

void ResourceManager::ReinitializeDynamicAtlas()
{
    FreeResources( RES_ATLAS_DYNAMIC );
    SprMngr.PushAtlasType( RES_ATLAS_DYNAMIC );
    SprMngr.InitializeEgg( "egg.png" );
    AnyFrames::Destroy( CritterDefaultAnim );
    AnyFrames::Destroy( ItemHexDefaultAnim );
    CritterDefaultAnim = SprMngr.LoadAnimation( "art/misc/CritterStub.png", PT_DATA, true );
    ItemHexDefaultAnim = SprMngr.LoadAnimation( "art/misc/ItemStub.png", PT_DATA, true );
    SprMngr.PopAtlasType();
}

AnyFrames* ResourceManager::GetAnim( hash name_hash, int res_type )
{
    // Find already loaded
    auto it = loadedAnims.find( name_hash );
    if( it != loadedAnims.end() )
        return ( *it ).second.Anim;

    // Load new animation
    const char* fname = Str::GetName( name_hash );
    if( !fname )
        return NULL;

    SprMngr.PushAtlasType( res_type );
    AnyFrames* anim = SprMngr.LoadAnimation( fname, PT_DATA, false, true );
    SprMngr.PopAtlasType();

    loadedAnims.insert( PAIR( name_hash, LoadedAnim( res_type, anim ) ) );
    return anim;
}

uint AnimMapId( uint crtype, uint anim1, uint anim2, bool is_fallout )
{
    uint dw[ 4 ] = { crtype, anim1, anim2, is_fallout ? uint( -1 ) : 1 };
    return Crypt.Crc32( (uchar*) &dw[ 0 ], sizeof( dw ) );
}

AnyFrames* ResourceManager::GetCrit2dAnim( uint crtype, uint anim1, uint anim2, int dir )
{
    // Check for 3d
    uint anim_type = CritType::GetAnimType( crtype );
    if( anim_type == ANIM_TYPE_3D )
        return NULL;

    // Process dir
    dir = CLAMP( dir, 0, DIRS_COUNT - 1 );
    if( !CritType::IsCanRotate( crtype ) )
        dir = 0;

    // Make animation id
    uint id = AnimMapId( crtype, anim1, anim2, false );

    // Check already loaded
    auto it = critterFrames.find( id );
    if( it != critterFrames.end() )
        return ( *it ).second ? ( *it ).second->GetDir( dir ) : NULL;

    // Process loading
    uint       crtype_base = crtype, anim1_base = anim1, anim2_base = anim2;
    AnyFrames* anim = NULL;
    while( true )
    {
        // Load
        if( anim_type == ANIM_TYPE_FALLOUT )
        {
            // Hardcoded
            anim = LoadFalloutAnim( crtype, anim1, anim2 );
        }
        else
        {
            // Script specific
            uint pass_base = 0;
            #ifdef FONLINE_CLIENT
            while( Script::PrepareContext( ClientFunctions.CritterAnimation, _FUNC_, "Anim" ) )
            #else // FONLINE_MAPPER
            while( Script::PrepareContext( MapperFunctions.CritterAnimation, _FUNC_, "Anim" ) )
            #endif
            {
                #define ANIM_FLAG_FIRST_FRAME    ( 0x01 )
                #define ANIM_FLAG_LAST_FRAME     ( 0x02 )

                uint pass = pass_base;
                uint flags = 0;
                int ox = 0, oy = 0;
                Script::SetArgUInt( anim_type );
                Script::SetArgUInt( crtype );
                Script::SetArgUInt( anim1 );
                Script::SetArgUInt( anim2 );
                Script::SetArgAddress( &pass );
                Script::SetArgAddress( &flags );
                Script::SetArgAddress( &ox );
                Script::SetArgAddress( &oy );
                if( Script::RunPrepared() )
                {
                    ScriptString* str = (ScriptString*) Script::GetReturnedObject();
                    if( str )
                    {
                        if( str->length() )
                        {
                            SprMngr.PushAtlasType( RES_ATLAS_DYNAMIC );
                            anim = SprMngr.LoadAnimation( str->c_str(), PT_DATA );
                            SprMngr.PopAtlasType();

                            // Fix by dirs
                            for( int d = 0; anim && d < anim->DirCount(); d++ )
                            {
                                AnyFrames* dir_anim = anim->GetDir( d );

                                // Process flags
                                if( flags )
                                {
                                    if( FLAG( flags, ANIM_FLAG_FIRST_FRAME ) || FLAG( flags, ANIM_FLAG_LAST_FRAME ) )
                                    {
                                        bool first = FLAG( flags, ANIM_FLAG_FIRST_FRAME );

                                        // Append offsets
                                        if( !first )
                                        {
                                            for( uint i = 0; i < dir_anim->CntFrm - 1; i++ )
                                            {
                                                dir_anim->NextX[ dir_anim->CntFrm - 1 ] += dir_anim->NextX[ i ];
                                                dir_anim->NextY[ dir_anim->CntFrm - 1 ] += dir_anim->NextY[ i ];
                                            }
                                        }

                                        // Change size
                                        dir_anim->Ind[ 0 ] = ( first ? dir_anim->Ind[ 0 ] : dir_anim->Ind[ dir_anim->CntFrm - 1 ] );
                                        dir_anim->NextX[ 0 ] = ( first ? dir_anim->NextX[ 0 ] : dir_anim->NextX[ dir_anim->CntFrm - 1 ] );
                                        dir_anim->NextY[ 0 ] = ( first ? dir_anim->NextY[ 0 ] : dir_anim->NextY[ dir_anim->CntFrm - 1 ] );
                                        dir_anim->CntFrm = 1;
                                    }
                                }

                                // Add offsets
                                if( ( ox || oy ) && false )
                                {
                                    for( uint i = 0; i < dir_anim->CntFrm; i++ )
                                    {
                                        uint spr_id = dir_anim->Ind[ i ];
                                        bool fixed = false;
                                        for( uint j = 0; j < i; j++ )
                                        {
                                            if( dir_anim->Ind[ j ] == spr_id )
                                            {
                                                fixed = true;
                                                break;
                                            }
                                        }
                                        if( !fixed )
                                        {
                                            SpriteInfo* si = SprMngr.GetSpriteInfo( spr_id );
                                            si->OffsX += ox;
                                            si->OffsY += oy;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // If pass changed and animation not loaded than try again
                    if( !anim && pass != pass_base )
                    {
                        pass_base = pass;
                        continue;
                    }

                    // Done
                    break;
                }
            }
        }

        // Find substitute animation
        #ifdef FONLINE_CLIENT
        if( !anim && Script::PrepareContext( ClientFunctions.CritterAnimationSubstitute, _FUNC_, "Anim" ) )
        #else // FONLINE_MAPPER
        if( !anim && Script::PrepareContext( MapperFunctions.CritterAnimationSubstitute, _FUNC_, "Anim" ) )
        #endif
        {
            uint crtype_ = crtype, anim1_ = anim1, anim2_ = anim2;
            Script::SetArgUInt( anim_type );
            Script::SetArgUInt( crtype_base );
            Script::SetArgUInt( anim1_base );
            Script::SetArgUInt( anim2_base );
            Script::SetArgAddress( &crtype );
            Script::SetArgAddress( &anim1 );
            Script::SetArgAddress( &anim2 );
            if( Script::RunPrepared() && Script::GetReturnedBool() &&
                ( crtype_ != crtype || anim1 != anim1_ || anim2 != anim2_ ) )
                continue;
        }

        // Exit from loop
        break;
    }

    // Store resulted animation indices
    if( anim )
    {
        for( int d = 0; d < anim->DirCount(); d++ )
        {
            anim->GetDir( d )->Anim1 = anim1;
            anim->GetDir( d )->Anim2 = anim2;
        }
    }

    // Store
    critterFrames.insert( PAIR( id, anim ) );
    return anim ? anim->GetDir( dir ) : NULL;
}

AnyFrames* ResourceManager::LoadFalloutAnim( uint crtype, uint anim1, uint anim2 )
{
    // Convert from common to fallout specific
    #ifdef FONLINE_CLIENT
    if( Script::PrepareContext( ClientFunctions.CritterAnimationFallout, _FUNC_, "Anim" ) )
    #else // FONLINE_MAPPER
    if( Script::PrepareContext( MapperFunctions.CritterAnimationFallout, _FUNC_, "Anim" ) )
    #endif
    {
        uint anim1ex = 0, anim2ex = 0, flags = 0;
        Script::SetArgUInt( crtype );
        Script::SetArgAddress( &anim1 );
        Script::SetArgAddress( &anim2 );
        Script::SetArgAddress( &anim1ex );
        Script::SetArgAddress( &anim2ex );
        Script::SetArgAddress( &flags );
        if( Script::RunPrepared() && Script::GetReturnedBool() )
        {
            // Load
            AnyFrames* anim = LoadFalloutAnimSpr( crtype, anim1, anim2 );
            if( !anim )
                return NULL;

            // Merge
            if( anim1ex && anim2ex )
            {
                AnyFrames* animex = LoadFalloutAnimSpr( crtype, anim1ex, anim2ex );
                if( !animex )
                    return NULL;

                AnyFrames* anim_merge_base = AnyFrames::Create( anim->CntFrm + animex->CntFrm, anim->Ticks + animex->Ticks );
                for( int d = 0; d < anim->DirCount(); d++ )
                {
                    if( d == 1 )
                        anim_merge_base->CreateDirAnims();
                    AnyFrames* anim_merge = anim_merge_base->GetDir( d );
                    AnyFrames* anim_ = anim->GetDir( d );
                    AnyFrames* animex_ = animex->GetDir( d );
                    memcpy( anim_merge->Ind, anim_->Ind, anim_->CntFrm * sizeof( uint ) );
                    memcpy( anim_merge->NextX, anim_->NextX, anim_->CntFrm * sizeof( short ) );
                    memcpy( anim_merge->NextY, anim_->NextY, anim_->CntFrm * sizeof( short ) );
                    memcpy( anim_merge->Ind + anim_->CntFrm, animex_->Ind, animex_->CntFrm * sizeof( uint ) );
                    memcpy( anim_merge->NextX + anim_->CntFrm, animex_->NextX, animex_->CntFrm * sizeof( short ) );
                    memcpy( anim_merge->NextY + anim_->CntFrm, animex_->NextY, animex_->CntFrm * sizeof( short ) );
                    short ox = 0, oy = 0;
                    for( uint i = 0; i < anim_->CntFrm; i++ )
                    {
                        ox += anim_->NextX[ i ];
                        oy += anim_->NextY[ i ];
                    }
                    anim_merge->NextX[ anim_->CntFrm ] -= ox;
                    anim_merge->NextY[ anim_->CntFrm ] -= oy;
                }
                return anim_merge_base;
            }

            // Clone
            if( anim )
            {
                AnyFrames* anim_clone_base = AnyFrames::Create( !FLAG( flags, ANIM_FLAG_FIRST_FRAME | ANIM_FLAG_LAST_FRAME ) ? anim->CntFrm : 1, anim->Ticks );
                for( int d = 0; d < anim->DirCount(); d++ )
                {
                    if( d == 1 )
                        anim_clone_base->CreateDirAnims();
                    AnyFrames* anim_clone = anim_clone_base->GetDir( d );
                    AnyFrames* anim_ = anim->GetDir( d );
                    if( !FLAG( flags, ANIM_FLAG_FIRST_FRAME | ANIM_FLAG_LAST_FRAME ) )
                    {
                        memcpy( anim_clone->Ind, anim_->Ind, anim_->CntFrm * sizeof( uint ) );
                        memcpy( anim_clone->NextX, anim_->NextX, anim_->CntFrm * sizeof( short ) );
                        memcpy( anim_clone->NextY, anim_->NextY, anim_->CntFrm * sizeof( short ) );
                    }
                    else
                    {
                        anim_clone->Ind[ 0 ] = anim_->Ind[ FLAG( flags, ANIM_FLAG_FIRST_FRAME ) ? 0 : anim_->CntFrm - 1 ];
                        anim_clone->NextX[ 0 ] = anim_->NextX[ FLAG( flags, ANIM_FLAG_FIRST_FRAME ) ? 0 : anim_->CntFrm - 1 ];
                        anim_clone->NextY[ 0 ] = anim_->NextY[ FLAG( flags, ANIM_FLAG_FIRST_FRAME ) ? 0 : anim_->CntFrm - 1 ];

                        // Append offsets
                        if( FLAG( flags, ANIM_FLAG_LAST_FRAME ) )
                        {
                            for( uint i = 0; i < anim_->CntFrm - 1; i++ )
                            {
                                anim_clone->NextX[ 0 ] += anim_->NextX[ i ];
                                anim_clone->NextY[ 0 ] += anim_->NextY[ i ];
                            }
                        }
                    }
                }
                return anim_clone_base;
            }
            return NULL;
        }
    }
    return NULL;
}

void FixAnimOffs( AnyFrames* frames_base, AnyFrames* stay_frm_base )
{
    if( !stay_frm_base )
        return;
    for( int d = 0; d < stay_frm_base->DirCount(); d++ )
    {
        AnyFrames* frames = frames_base->GetDir( d );
        AnyFrames* stay_frm = stay_frm_base->GetDir( d );
        SpriteInfo* stay_si = SprMngr.GetSpriteInfo( stay_frm->Ind[ 0 ] );
        if( !stay_si )
            return;
        for( uint i = 0; i < frames->CntFrm; i++ )
        {
            SpriteInfo* si = SprMngr.GetSpriteInfo( frames->Ind[ i ] );
            if( !si )
                continue;
            si->OffsX += stay_si->OffsX;
            si->OffsY += stay_si->OffsY;
        }
    }
}

void FixAnimOffsNext( AnyFrames* frames_base, AnyFrames* stay_frm_base )
{
    if( !stay_frm_base )
        return;
    for( int d = 0; d < stay_frm_base->DirCount(); d++ )
    {
        AnyFrames* frames = frames_base->GetDir( d );
        AnyFrames* stay_frm = stay_frm_base->GetDir( d );
        SpriteInfo* stay_si = SprMngr.GetSpriteInfo( stay_frm->Ind[ 0 ] );
        if( !stay_si )
            return;
        short ox = 0;
        short oy = 0;
        for( uint i = 0; i < stay_frm->CntFrm; i++ )
        {
            ox += stay_frm->NextX[ i ];
            oy += stay_frm->NextY[ i ];
        }
        for( uint i = 0; i < frames->CntFrm; i++ )
        {
            SpriteInfo* si = SprMngr.GetSpriteInfo( frames->Ind[ i ] );
            if( !si )
                continue;
            si->OffsX += ox;
            si->OffsY += oy;
        }
    }
}

AnyFrames* ResourceManager::LoadFalloutAnimSpr( uint crtype, uint anim1, uint anim2 )
{
    auto it = critterFrames.find( AnimMapId( crtype, anim1, anim2, true ) );
    if( it != critterFrames.end() )
        return ( *it ).second;

    // Load file
    static char frm_ind[] = "_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char spr_name[ MAX_FOPATH ];
    SprMngr.PushAtlasType( RES_ATLAS_DYNAMIC );

    // Try load fofrm
    const char* name = CritType::GetName( crtype );
    Str::Format( spr_name, "%s%c%c.fofrm", name, frm_ind[ anim1 ], frm_ind[ anim2 ] );
    AnyFrames* frames = SprMngr.LoadAnimation( spr_name, PT_ART_CRITTERS );

    // Try load fallout frames
    if( !frames )
    {
        Str::Format( spr_name, "%s%c%c.frm", name, frm_ind[ anim1 ], frm_ind[ anim2 ] );
        frames = SprMngr.LoadAnimation( spr_name, PT_ART_CRITTERS );
    }
    SprMngr.PopAtlasType();

    critterFrames.insert( PAIR( AnimMapId( crtype, anim1, anim2, true ), frames ) );
    if( !frames )
        return NULL;

    #define LOADSPR_ADDOFFS( a1, a2 )         FixAnimOffs( frames, LoadFalloutAnimSpr( crtype, a1, a2 ) )
    #define LOADSPR_ADDOFFS_NEXT( a1, a2 )    FixAnimOffsNext( frames, LoadFalloutAnimSpr( crtype, a1, a2 ) )

    // Fallout animations
    #define ANIM1_FALLOUT_UNARMED               ( 1 )
    #define ANIM1_FALLOUT_DEAD                  ( 2 )
    #define ANIM1_FALLOUT_KNOCKOUT              ( 3 )
    #define ANIM1_FALLOUT_KNIFE                 ( 4 )
    #define ANIM1_FALLOUT_CLUB                  ( 5 )
    #define ANIM1_FALLOUT_HAMMER                ( 6 )
    #define ANIM1_FALLOUT_SPEAR                 ( 7 )
    #define ANIM1_FALLOUT_PISTOL                ( 8 )
    #define ANIM1_FALLOUT_UZI                   ( 9 )
    #define ANIM1_FALLOUT_SHOOTGUN              ( 10 )
    #define ANIM1_FALLOUT_RIFLE                 ( 11 )
    #define ANIM1_FALLOUT_MINIGUN               ( 12 )
    #define ANIM1_FALLOUT_ROCKET_LAUNCHER       ( 13 )
    #define ANIM1_FALLOUT_AIM                   ( 14 )
    #define ANIM2_FALLOUT_STAY                  ( 1 )
    #define ANIM2_FALLOUT_WALK                  ( 2 )
    #define ANIM2_FALLOUT_SHOW                  ( 3 )
    #define ANIM2_FALLOUT_HIDE                  ( 4 )
    #define ANIM2_FALLOUT_DODGE_WEAPON          ( 5 )
    #define ANIM2_FALLOUT_THRUST                ( 6 )
    #define ANIM2_FALLOUT_SWING                 ( 7 )
    #define ANIM2_FALLOUT_PREPARE_WEAPON        ( 8 )
    #define ANIM2_FALLOUT_TURNOFF_WEAPON        ( 9 )
    #define ANIM2_FALLOUT_SHOOT                 ( 10 )
    #define ANIM2_FALLOUT_BURST                 ( 11 )
    #define ANIM2_FALLOUT_FLAME                 ( 12 )
    #define ANIM2_FALLOUT_THROW_WEAPON          ( 13 )
    #define ANIM2_FALLOUT_DAMAGE_FRONT          ( 15 )
    #define ANIM2_FALLOUT_DAMAGE_BACK           ( 16 )
    #define ANIM2_FALLOUT_KNOCK_FRONT           ( 1 )  // Only with ANIM1_FALLOUT_DEAD
    #define ANIM2_FALLOUT_KNOCK_BACK            ( 2 )
    #define ANIM2_FALLOUT_STANDUP_BACK          ( 8 )  // Only with ANIM1_FALLOUT_KNOCKOUT
    #define ANIM2_FALLOUT_STANDUP_FRONT         ( 10 )
    #define ANIM2_FALLOUT_PICKUP                ( 11 ) // Only with ANIM1_FALLOUT_UNARMED
    #define ANIM2_FALLOUT_USE                   ( 12 )
    #define ANIM2_FALLOUT_DODGE_EMPTY           ( 14 )
    #define ANIM2_FALLOUT_PUNCH                 ( 17 )
    #define ANIM2_FALLOUT_KICK                  ( 18 )
    #define ANIM2_FALLOUT_THROW_EMPTY           ( 19 )
    #define ANIM2_FALLOUT_RUN                   ( 20 )
    #define ANIM2_FALLOUT_DEAD_FRONT            ( 1 ) // Only with ANIM1_FALLOUT_DEAD
    #define ANIM2_FALLOUT_DEAD_BACK             ( 2 )
    #define ANIM2_FALLOUT_DEAD_BLOODY_SINGLE    ( 4 )
    #define ANIM2_FALLOUT_DEAD_BURN             ( 5 )
    #define ANIM2_FALLOUT_DEAD_BLOODY_BURST     ( 6 )
    #define ANIM2_FALLOUT_DEAD_BURST            ( 7 )
    #define ANIM2_FALLOUT_DEAD_PULSE            ( 8 )
    #define ANIM2_FALLOUT_DEAD_LASER            ( 9 )
    #define ANIM2_FALLOUT_DEAD_BURN2            ( 10 )
    #define ANIM2_FALLOUT_DEAD_PULSE_DUST       ( 11 )
    #define ANIM2_FALLOUT_DEAD_EXPLODE          ( 12 )
    #define ANIM2_FALLOUT_DEAD_FUSED            ( 13 )
    #define ANIM2_FALLOUT_DEAD_BURN_RUN         ( 14 )
    #define ANIM2_FALLOUT_DEAD_FRONT2           ( 15 )
    #define ANIM2_FALLOUT_DEAD_BACK2            ( 16 )
// ////////////////////////////////////////////////////////////////////////

    if( anim1 == ANIM1_FALLOUT_AIM )
        return frames;                              // Aim, 'N'

    // Empty offsets
    if( anim1 == ANIM1_FALLOUT_UNARMED )
    {
        if( anim2 == ANIM2_FALLOUT_STAY || anim2 == ANIM2_FALLOUT_WALK || anim2 == ANIM2_FALLOUT_RUN )
            return frames;
        LOADSPR_ADDOFFS( ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY );
    }

    // Weapon offsets
    if( anim1 >= ANIM1_FALLOUT_KNIFE && anim1 <= ANIM1_FALLOUT_ROCKET_LAUNCHER )
    {
        if( anim2 == ANIM2_FALLOUT_SHOW )
            LOADSPR_ADDOFFS( ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY );
        else if( anim2 == ANIM2_FALLOUT_WALK )
            return frames;
        else if( anim2 == ANIM2_FALLOUT_STAY )
        {
            LOADSPR_ADDOFFS( ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY );
            LOADSPR_ADDOFFS_NEXT( anim1, ANIM2_FALLOUT_SHOW );
        }
        else if( anim2 == ANIM2_FALLOUT_SHOOT || anim2 == ANIM2_FALLOUT_BURST || anim2 == ANIM2_FALLOUT_FLAME )
        {
            LOADSPR_ADDOFFS( anim1, ANIM2_FALLOUT_PREPARE_WEAPON );
            LOADSPR_ADDOFFS_NEXT( anim1, ANIM2_FALLOUT_PREPARE_WEAPON );
        }
        else if( anim2 == ANIM2_FALLOUT_TURNOFF_WEAPON )
        {
            if( anim1 == ANIM1_FALLOUT_MINIGUN )
            {
                LOADSPR_ADDOFFS( anim1, ANIM2_FALLOUT_BURST );
                LOADSPR_ADDOFFS_NEXT( anim1, ANIM2_FALLOUT_BURST );
            }
            else
            {
                LOADSPR_ADDOFFS( anim1, ANIM2_FALLOUT_SHOOT );
                LOADSPR_ADDOFFS_NEXT( anim1, ANIM2_FALLOUT_SHOOT );
            }
        }
        else
            LOADSPR_ADDOFFS( anim1, ANIM2_FALLOUT_STAY );
    }

    // Dead & Ko offsets
    if( anim1 == ANIM1_FALLOUT_DEAD )
    {
        if( anim2 == ANIM2_FALLOUT_DEAD_FRONT2 )
        {
            LOADSPR_ADDOFFS( ANIM1_FALLOUT_DEAD, ANIM2_FALLOUT_DEAD_FRONT );
            LOADSPR_ADDOFFS_NEXT( ANIM1_FALLOUT_DEAD, ANIM2_FALLOUT_DEAD_FRONT );
        }
        else if( anim2 == ANIM2_FALLOUT_DEAD_BACK2 )
        {
            LOADSPR_ADDOFFS( ANIM1_FALLOUT_DEAD, ANIM2_FALLOUT_DEAD_BACK );
            LOADSPR_ADDOFFS_NEXT( ANIM1_FALLOUT_DEAD, ANIM2_FALLOUT_DEAD_BACK );
        }
        else
        {
            LOADSPR_ADDOFFS( ANIM1_FALLOUT_UNARMED, ANIM2_FALLOUT_STAY );
        }
    }

    // Ko rise offsets
    if( anim1 == ANIM1_FALLOUT_KNOCKOUT )
    {
        uchar anim2_ = ANIM2_FALLOUT_KNOCK_FRONT;
        if( anim2 == ANIM2_FALLOUT_STANDUP_BACK )
            anim2_ = ANIM2_FALLOUT_KNOCK_BACK;
        LOADSPR_ADDOFFS( ANIM1_FALLOUT_DEAD, anim2_ );
        LOADSPR_ADDOFFS_NEXT( ANIM1_FALLOUT_DEAD, anim2_ );
    }

    return frames;
}

Animation3d* ResourceManager::GetCrit3dAnim( uint crtype, uint anim1, uint anim2, int dir, int* layers3d /* = NULL */ )
{
    if( CritType::GetAnimType( crtype ) != ANIM_TYPE_3D )
        return NULL;

    if( !CritType::IsCanRotate( crtype ) )
        dir = 0;

    if( crtype < critter3d.size() && critter3d[ crtype ] )
    {
        critter3d[ crtype ]->SetAnimation( anim1, anim2, layers3d, ANIMATION_STAY | ANIMATION_NO_SMOOTH );
        critter3d[ crtype ]->SetDir( dir );
        return critter3d[ crtype ];
    }

    char name[ MAX_FOPATH ];
    Str::Format( name, "%s.fo3d", CritType::GetName( crtype ) );
    SprMngr.PushAtlasType( RES_ATLAS_DYNAMIC );
    Animation3d* anim3d = SprMngr.LoadPure3dAnimation( name, PT_ART_CRITTERS, true );
    SprMngr.PopAtlasType();
    if( !anim3d )
        return NULL;

    if( crtype >= critter3d.size() )
        critter3d.resize( crtype + 1 );
    critter3d[ crtype ] = anim3d;

    anim3d->SetAnimation( anim1, anim2, layers3d, ANIMATION_STAY | ANIMATION_NO_SMOOTH );
    anim3d->SetDir( dir );
    anim3d->StartMeshGeneration();
    return anim3d;
}

uint ResourceManager::GetCritSprId( uint crtype, uint anim1, uint anim2, int dir, int* layers3d /* = NULL */ )
{
    uint spr_id = 0;
    if( CritType::GetAnimType( crtype ) != ANIM_TYPE_3D )
    {
        AnyFrames* anim = GetCrit2dAnim( crtype, anim1, anim2, dir );
        spr_id = ( anim ? anim->GetSprId( 0 ) : 0 );
    }
    else
    {
        Animation3d* anim3d = GetCrit3dAnim( crtype, anim1, anim2, dir, layers3d );
        spr_id = ( anim3d ? anim3d->SprId : 0 );
    }
    return spr_id;
}

AnyFrames* ResourceManager::GetRandomSplash()
{
    if( splashNames.empty() )
        return 0;
    int rnd = Random( 0, (int) splashNames.size() - 1 );
    static AnyFrames* splash = NULL;
    SprMngr.PushAtlasType( RES_ATLAS_SPLASH, true );
    splash = SprMngr.ReloadAnimation( splash, splashNames[ rnd ].c_str(), PT_DATA );
    SprMngr.PopAtlasType();
    return splash;
}
