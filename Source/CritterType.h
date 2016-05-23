#ifndef __CRITTER_TYPE__
#define __CRITTER_TYPE__

#include "Defines.h"

#define ANIM_TYPE_FALLOUT    ( 0 )
#define ANIM_TYPE_3D         ( 1 )

class FOMsg;

struct CritTypeType
{
    bool Enabled;
    char Name[ 64 ];
    char SoundName[ 64 ];
    uint Alias;
    uint Multihex;
    int  AnimType;

    bool CanWalk;
    bool CanRun;
    bool CanAim;
    bool CanArmor;
    bool CanRotate;

    bool Anim1[ 37 ];   // A..Z 0..9
};

namespace CritType
{
    bool          IsEnabled( uint cr_type );
    CritTypeType& GetCritType( uint cr_type );
    CritTypeType& GetRealCritType( uint cr_type );
    const char*   GetName( uint cr_type );
    const char*   GetSoundName( uint cr_type );
    uint          GetAlias( uint cr_type );
    uint          GetMultihex( uint cr_type );
    int           GetAnimType( uint cr_type );
    bool          IsCanWalk( uint cr_type );
    bool          IsCanRun( uint cr_type );
    bool          IsCanAim( uint cr_type );
    bool          IsCanArmor( uint cr_type );
    bool          IsCanRotate( uint cr_type );
    bool          IsAnim1( uint cr_type, uint anim1 );

    void SetWalkParams( uint cr_type, uint time_walk, uint time_run, uint step0, uint step1, uint step2, uint step3 );
    uint GetTimeWalk( uint cr_type );
    uint GetTimeRun( uint cr_type );
    int  GetWalkFrmCnt( uint cr_type, uint step );
    int  GetRunFrmCnt( uint cr_type, uint step );
}

#endif // __CRITTER_TYPE__
