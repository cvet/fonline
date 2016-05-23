#ifndef __AI__
#define __AI__

#include "Common.h"

#define NPC_GO_HOME_WAIT_TICK        ( Random( 4000, 6000 ) )

#define AI_PLANE_MISC                ( 0 )
#define AI_PLANE_ATTACK              ( 1 )
#define AI_PLANE_WALK                ( 2 )
#define AI_PLANE_PICK                ( 3 )
#define AI_PLANE_PATROL              ( 4 )
#define AI_PLANE_COURIER             ( 5 )

#define AI_PLANE_MISC_PRIORITY       ( 10 )
#define AI_PLANE_ATTACK_PRIORITY     ( 50 )
#define AI_PLANE_WALK_PRIORITY       ( 20 )
#define AI_PLANE_PICK_PRIORITY       ( 35 )
#define AI_PLANE_PATROL_PRIORITY     ( 25 )
#define AI_PLANE_COURIER_PRIORITY    ( 30 )

struct AIDataPlane
{
    int          Type;
    uint         Priority;
    int          Identifier;
    uint         IdentifierExt;
    AIDataPlane* ChildPlane;
    bool         IsMove;

    union
    {
        struct
        {
            bool IsRun;
            uint WaitSecond;
            uint ScriptBindId;
        } Misc;

        struct
        {
            bool   IsRun;
            uint   TargId;
            int    MinHp;
            bool   IsGag;
            ushort GagHexX, GagHexY;
            ushort LastHexX, LastHexY;
        } Attack;

        struct
        {
            bool   IsRun;
            ushort HexX;
            ushort HexY;
            uchar  Dir;
            uint   Cut;
        } Walk;

        struct
        {
            bool   IsRun;
            ushort HexX;
            ushort HexY;
            ushort Pid;
            uint   UseItemId;
            bool   ToOpen;
        } Pick;

        struct
        {
            uint Buffer[ 8 ];
        } Buffer;
    };

    struct
    {
        uint   PathNum;
        uint   Iter;
        bool   IsRun;
        uint   TargId;
        ushort HexX;
        ushort HexY;
        uint   Cut;
        uint   Trace;
    } Move;

    bool         Assigned;
    int          RefCounter;

    AIDataPlane* GetCurPlane()
    {
        return ChildPlane ? ChildPlane->GetCurPlane() : this;
    }

    bool IsSelfOrHas( int type )
    {
        return Type == type || ( ChildPlane ? ChildPlane->IsSelfOrHas( type ) : false );
    }

    void DeleteLast()
    {
        if( ChildPlane )
        {
            if( ChildPlane->ChildPlane ) ChildPlane->DeleteLast();
            else SAFEREL( ChildPlane );
        }
    }

    AIDataPlane* GetCopy()
    {
        AIDataPlane* copy = new AIDataPlane( Type, Priority );
        if( !copy ) return nullptr;
        memcpy( copy->Buffer.Buffer, Buffer.Buffer, sizeof( Buffer.Buffer ) );
        AIDataPlane* result = copy;
        AIDataPlane* plane_child = ChildPlane;
        while( plane_child )
        {
            copy->ChildPlane = new AIDataPlane( plane_child->Type, plane_child->Priority );
            if( !copy->ChildPlane ) return nullptr;
            copy->ChildPlane->Assigned = true;
            memcpy( copy->ChildPlane->Buffer.Buffer, plane_child->Buffer.Buffer, sizeof( plane_child->Buffer.Buffer ) );
            plane_child = plane_child->ChildPlane;
            copy = copy->ChildPlane;
        }
        return result;
    }

    void AddRef()
    {
        RefCounter++;
    }

    void Release()
    {
        if( !--RefCounter )
            delete this;
    }

    AIDataPlane( uint type, uint priority ): Type( type ), Priority( priority ), Identifier( 0 ), IdentifierExt( 0 ), ChildPlane( nullptr ), IsMove( false ), Assigned( false ), RefCounter( 1 )
    {
        memzero( &Buffer, sizeof( Buffer ) );
        memzero( &Move, sizeof( Move ) );
        MEMORY_PROCESS( MEMORY_NPC_PLANE, sizeof( AIDataPlane ) );
    }

    ~AIDataPlane()
    {
        SAFEREL( ChildPlane );
        MEMORY_PROCESS( MEMORY_NPC_PLANE, -(int) sizeof( AIDataPlane ) );
    }

private:
    // Disable default constructor
    AIDataPlane() {}
};
typedef vector< AIDataPlane* > AIDataPlaneVec;

// Plane begin/end/run reasons
// Begin
#define REASON_GO_HOME                 ( 10 )
#define REASON_FOUND_IN_ENEMY_STACK    ( 11 )
#define REASON_FROM_DIALOG             ( 12 )
#define REASON_FROM_SCRIPT             ( 13 )
#define REASON_RUN_AWAY                ( 14 )
// End
#define REASON_SUCCESS                 ( 30 )
#define REASON_HEX_TOO_FAR             ( 31 )
#define REASON_HEX_BUSY                ( 32 )
#define REASON_HEX_BUSY_RING           ( 33 )
#define REASON_DEADLOCK                ( 34 )
#define REASON_TRACE_FAIL              ( 35 )
#define REASON_POSITION_NOT_FOUND      ( 36 )
#define REASON_FIND_PATH_ERROR         ( 37 )
#define REASON_CANT_WALK               ( 38 )
#define REASON_TARGET_DISAPPEARED      ( 39 )
#define REASON_USE_ITEM_NOT_FOUND      ( 40 )
#define REASON_GAG_CRITTER             ( 41 )
#define REASON_GAG_ITEM                ( 42 )
#define REASON_NO_UNARMED              ( 43 )
// Run
#define REASON_ATTACK_TARGET           ( 50 )
#define REASON_ATTACK_WEAPON           ( 51 )
#define REASON_ATTACK_DISTANTION       ( 52 )
#define REASON_ATTACK_USE_AIM          ( 53 )

#endif // __AI__
