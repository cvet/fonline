#ifndef __SCRIPT_INVOKER__
#define __SCRIPT_INVOKER__

#include "Common.h"

struct Invocation
{
    uint    Id;
    uint    FireTick;
    hash    FuncNum;
    uint    BindId;
    bool    Repeatable;
    IntVec* Values;
    uint    Rate;
    bool    Saved;
};
typedef vector< Invocation* > InvocationVec;

class ScriptInvoker
{
    friend class Script;

private:
    uint          lastInvocationId;
    InvocationVec deferredInvocations;
    Mutex         deferredInvocationsLocker;

    ScriptInvoker();
    uint   Invoke( uint delay, bool saved, asIScriptFunction* func, ScriptArray* values );
    uint   Invoke( uint delay, bool saved, hash func_num, uint bind_id, bool repeatable, IntVec* values );
    bool   IsInvoking( uint id );
    bool   CancelInvoke( uint id );
    bool   GetInvokeData( uint id, Invocation& data );
    void   GetInvokeList( IntVec& ids );
    void   Process();
    void   RunInvocation( Invocation* invocation );
    void   SaveInvocations( void ( * save_func )( void*, size_t ) );
    bool   LoadInvocations( void* f, uint version );
    string GetStatistics();

public:
    static uint Global_Invoke( asIScriptFunction* func, uint delay );
    static uint Global_InvokeWithValues( asIScriptFunction* func, ScriptArray* values, uint delay );
    static uint Global_SavedInvoke( asIScriptFunction* func, uint delay );
    static uint Global_SavedInvokeWithValues( asIScriptFunction* func, ScriptArray* values, uint delay );
    static bool Global_IsInvoking( uint id );
    static bool Global_CancelInvoke( uint id );
    static bool Global_GetInvokeData( uint id, uint& delay, ScriptArray* values );
    static uint Global_GetInvokeList( ScriptArray* ids );
};

#endif // __SCRIPT_INVOKER__
