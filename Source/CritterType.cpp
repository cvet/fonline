#include "Common.h"
#include "CritterType.h"
#include "Text.h"
#include "FileManager.h"
#include "MsgFiles.h"

static CritTypeType CrTypes[ MAX_CRIT_TYPES ];
static int          MoveWalk[ MAX_CRIT_TYPES ][ 6 ];

bool CritType::IsEnabled( uint cr_type )
{
    return cr_type < MAX_CRIT_TYPES && CrTypes[ cr_type ].Enabled;
}

CritTypeType& CritType::GetCritType( uint cr_type )
{
    return cr_type < MAX_CRIT_TYPES && CrTypes[ cr_type ].Enabled ? CrTypes[ cr_type ] : CrTypes[ 0 ];
}

CritTypeType& CritType::GetRealCritType( uint cr_type )
{
    return CrTypes[ cr_type ];
}

const char* CritType::GetName( uint cr_type )
{
    return GetCritType( cr_type ).Name;
}

const char* CritType::GetSoundName( uint cr_type )
{
    CritTypeType& type = GetCritType( cr_type );
    if( type.SoundName[ 0 ] )
        return type.SoundName;
    return type.Name;
}

uint CritType::GetAlias( uint cr_type )
{
    return GetCritType( cr_type ).Alias;
}

uint CritType::GetMultihex( uint cr_type )
{
    return GetCritType( cr_type ).Multihex;
}

int CritType::GetAnimType( uint cr_type )
{
    return GetCritType( cr_type ).AnimType;
}

bool CritType::IsCanWalk( uint cr_type )
{
    return GetCritType( cr_type ).CanWalk;
}

bool CritType::IsCanRun( uint cr_type )
{
    return GetCritType( cr_type ).CanRun;
}

bool CritType::IsCanAim( uint cr_type )
{
    return GetCritType( cr_type ).CanAim;
}

bool CritType::IsCanArmor( uint cr_type )
{
    return GetCritType( cr_type ).CanArmor;
}

bool CritType::IsCanRotate( uint cr_type )
{
    return GetCritType( cr_type ).CanRotate;
}

bool CritType::IsAnim1( uint cr_type, uint anim1 )
{
    if( anim1 >= 37 )
        return false;
    return GetCritType( cr_type ).Anim1[ anim1 ];
}

void CritType::SetWalkParams( uint cr_type, uint time_walk, uint time_run, uint step0, uint step1, uint step2, uint step3 )
{
    if( cr_type >= MAX_CRIT_TYPES )
        return;
    MoveWalk[ cr_type ][ 0 ] = step0;
    MoveWalk[ cr_type ][ 1 ] = step1;
    MoveWalk[ cr_type ][ 2 ] = step2;
    MoveWalk[ cr_type ][ 3 ] = step3;
    MoveWalk[ cr_type ][ 4 ] = time_walk;
    MoveWalk[ cr_type ][ 5 ] = time_run;
}

uint CritType::GetTimeWalk( uint cr_type )
{
    return cr_type < MAX_CRIT_TYPES && CrTypes[ cr_type ].Enabled ? MoveWalk[ cr_type ][ 4 ] : 0;
}

uint CritType::GetTimeRun( uint cr_type )
{
    return cr_type < MAX_CRIT_TYPES && CrTypes[ cr_type ].Enabled ? MoveWalk[ cr_type ][ 5 ] : 0;
}

int CritType::GetWalkFrmCnt( uint cr_type, uint step )
{
    return cr_type < MAX_CRIT_TYPES && CrTypes[ cr_type ].Enabled && step < 4 ? MoveWalk[ cr_type ][ step ] : 0;
}

int CritType::GetRunFrmCnt( uint cr_type, uint step )
{
    return 0;
/*
        return cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled && step<4?MoveWalk[cr_type][step]:0;

        if(cr_type>=MAX_CRIT_TYPES) return 0;
        if(step>3) return 0;

        return 0;*/
}
