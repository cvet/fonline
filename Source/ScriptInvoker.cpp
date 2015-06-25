#include "ScriptInvoker.h"
#include "Script.h"

#ifndef FONLINE_SCRIPT_COMPILER

ScriptInvoker::ScriptInvoker()
{
    lastInvocationId = 0;
}

uint ScriptInvoker::Invoke( uint delay, bool saved, asIScriptFunction* func, ScriptArray* values )
{
    uint    func_num = Script::GetFuncNum( func );
    uint    bind_id = Script::Bind( func, false );
    RUNTIME_ASSERT( bind_id );
    IntVec* values_ = NULL;
    if( values )
    {
        values_ = new IntVec();
        Script::AssignScriptArrayInVector( *values_, values );
    }
    return Invoke( delay, saved, func_num, bind_id, func->GetReturnTypeId() != asTYPEID_VOID, values_ );
}

uint ScriptInvoker::Invoke( uint delay, bool saved, hash func_num, uint bind_id, bool repeatable, IntVec* values )
{
    Invocation* invocation = new Invocation();
    invocation->Id = ++lastInvocationId;
    invocation->FireTick = Timer::FastTick() + delay;
    invocation->FuncNum = func_num;
    invocation->BindId = bind_id;
    invocation->Repeatable = repeatable;
    invocation->Values = values;
    invocation->Rate = 0;
    invocation->Saved = saved;

    if( delay > 0 )
    {
        SCOPE_LOCK( deferredInvocationsLocker );
        deferredInvocations.push_back( invocation );
    }
    else
    {
        RunInvocation( invocation );
    }
    return invocation->Id;
}

bool ScriptInvoker::IsInvoking( uint id )
{
    SCOPE_LOCK( deferredInvocationsLocker );

    for( auto it = deferredInvocations.begin(); it != deferredInvocations.end(); ++it )
    {
        Invocation* invocation = *it;
        if( invocation->Id == id )
            return true;
    }
    return false;
}

bool ScriptInvoker::CancelInvoke( uint id )
{
    SCOPE_LOCK( deferredInvocationsLocker );

    for( auto it = deferredInvocations.begin(); it != deferredInvocations.end(); ++it )
    {
        Invocation* invocation = *it;
        if( invocation->Id == id )
        {
            delete invocation;
            deferredInvocations.erase( it );
            return true;
        }
    }
    return false;
}

bool ScriptInvoker::GetInvokeData( uint id, Invocation& data )
{
    SCOPE_LOCK( deferredInvocationsLocker );

    for( auto it = deferredInvocations.begin(); it != deferredInvocations.end(); ++it )
    {
        if( ( *it )->Id == id )
        {
            data = **it;
            return true;
        }
    }
    return false;
}

void ScriptInvoker::GetInvokeList( IntVec& ids )
{
    SCOPE_LOCK( deferredInvocationsLocker );

    ids.reserve( deferredInvocations.size() );
    for( auto it = deferredInvocations.begin(); it != deferredInvocations.end(); ++it )
        ids.push_back( ( *it )->Id );
}

void ScriptInvoker::Process()
{
    InvocationVec fired_entries;
    {
        SCOPE_LOCK( deferredInvocationsLocker );

        uint tick = Timer::FastTick();
        for( auto it = deferredInvocations.begin(); it != deferredInvocations.end();)
        {
            Invocation* invocation = *it;
            if( Timer::FastTick() >= invocation->FireTick )
            {
                fired_entries.push_back( invocation );
                it = deferredInvocations.erase( it );
            }
            else
            {
                ++it;
            }
        }
    }

    for( auto it = fired_entries.begin(); it != fired_entries.end(); ++it )
        RunInvocation( *it );
}

void ScriptInvoker::RunInvocation( Invocation* invocation )
{
    asIScriptContext* ctx = Script::RequestContext();
    if( Script::PrepareContext( invocation->BindId, _FUNC_, "Invoker" ) )
    {
        if( invocation->Values )
            Script::SetArgObject( invocation->Values );

        if( Script::RunPrepared() )
        {
            invocation->Rate++;
            if( invocation->Repeatable )
            {
                uint new_delay = Script::GetReturnedUInt();
                if( new_delay > 0 )
                {
                    invocation->FireTick = Timer::FastTick() + new_delay;
                    SCOPE_LOCK( deferredInvocationsLocker );
                    deferredInvocations.push_back( invocation );
                    return;
                }
            }
        }
    }
    delete invocation;
}

void ScriptInvoker::SaveInvocations( void ( * save_func )( void*, size_t ) )
{
    SCOPE_LOCK( deferredInvocationsLocker );

    save_func( &lastInvocationId, sizeof( lastInvocationId ) );

    uint count = 0;
    for( auto it = deferredInvocations.begin(); it != deferredInvocations.end();)
    {
        Invocation* invocation = *it;
        if( invocation->Saved )
            count++;
    }
    save_func( &count, sizeof( count ) );

    uint tick = Timer::FastTick();
    for( auto it = deferredInvocations.begin(); it != deferredInvocations.end();)
    {
        Invocation* invocation = *it;
        if( !invocation->Saved )
            continue;

        save_func( &invocation->Id, sizeof( invocation->Id ) );
        save_func( &invocation->FuncNum, sizeof( invocation->FuncNum ) );
        uint delay = ( invocation->FireTick > tick ? invocation->FireTick - tick : 0 );
        save_func( &delay, sizeof( delay ) );
        save_func( &invocation->Rate, sizeof( invocation->Rate ) );
        save_func( &invocation->Repeatable, sizeof( invocation->Repeatable ) );
        uint values_size = (uint) ( invocation->Values ? invocation->Values->size() : 0 );
        save_func( &values_size, sizeof( values_size ) );
        if( values_size )
            save_func( &invocation->Values->at( 0 ), values_size * sizeof( int ) );
    }
}

bool ScriptInvoker::LoadInvocations( void* f, uint version )
{
    SCOPE_LOCK( deferredInvocationsLocker );

    WriteLog( "Load invocations...\n" );

    if( !FileRead( f, &lastInvocationId, sizeof( lastInvocationId ) ) )
        return false;

    uint count = 0;
    if( !FileRead( f, &count, sizeof( count ) ) )
        return false;

    uint tick = Timer::FastTick();
    for( uint i = 0; i < count; i++ )
    {
        uint id;
        if( !FileRead( f, &id, sizeof( id ) ) )
            return false;
        hash func_num;
        if( !FileRead( f, &func_num, sizeof( func_num ) ) )
            return false;
        uint delay;
        if( !FileRead( f, &delay, sizeof( delay ) ) )
            return false;
        uint rate;
        if( !FileRead( f, &rate, sizeof( rate ) ) )
            return false;
        bool repeatable;
        if( !FileRead( f, &repeatable, sizeof( repeatable ) ) )
            return false;

        IntVec values;
        uint   values_size;
        if( !FileRead( f, &values_size, sizeof( values_size ) ) )
            return false;
        if( values_size )
        {
            values.resize( values_size );
            if( !FileRead( f, &values[ 0 ], values_size * sizeof( int ) ) )
                return false;
        }

        uint bind_id = Script::Bind( func_num, false );
        if( !bind_id )
        {
            WriteLog( "Unable to bind script function '%s' for event %u. Skip event.\n", HASH_STR( func_num ), id );
            continue;
        }

        Invocation* invocation = new Invocation();
        invocation->Id = id;
        invocation->FireTick = tick + delay;
        invocation->FuncNum = func_num;
        invocation->BindId = bind_id;
        invocation->Repeatable = repeatable;
        invocation->Rate = rate;
        invocation->Saved = true;
        if( values_size )
        {
            invocation->Values = new IntVec();
            *invocation->Values = values;
        }

        deferredInvocations.push_back( invocation );
    }

    WriteLog( "Load invocations complete, count %u.\n", count );
    return true;

}

string ScriptInvoker::GetStatistics()
{
    SCOPE_LOCK( deferredInvocationsLocker );

    char   buf[ MAX_FOTEXT ];
    uint   tick = Timer::FastTick();
    string result = Str::Format( buf, "Invocations count: %u\n", (uint) deferredInvocations.size() );
    result += "Id         Delay      Rate Saved Function                            Values\n";
    for( auto it = deferredInvocations.begin(); it != deferredInvocations.end(); ++it )
    {
        Invocation* invocation = *it;
        string      func_name = Script::GetBindFuncName( invocation->BindId );
        result += Str::Format( buf, "%-10u %-10u %-4u %-5s %-35s", invocation->Id, tick < invocation->FireTick ? invocation->FireTick - tick : 0,
                               invocation->Rate, invocation->Saved ? "true" : "false", func_name.c_str() );
        if( invocation->Values )
        {
            for( size_t i = 0; i < invocation->Values->size(); i++ )
                result += Str::Format( buf, " %u", invocation->Values->at( i ) );
        }
        result += "\n";
    }
    return result;
}

uint ScriptInvoker::Global_Invoke( asIScriptFunction* func, uint delay )
{
    ScriptInvoker* self = Script::GetInvoker();
    return self->Invoke( delay, false, func, NULL );
}

uint ScriptInvoker::Global_InvokeWithValues( asIScriptFunction* func, ScriptArray* values, uint delay )
{
    ScriptInvoker* self = Script::GetInvoker();
    return self->Invoke( delay, false, func, values );
}

uint ScriptInvoker::Global_SavedInvoke( asIScriptFunction* func, uint delay )
{
    ScriptInvoker* self = Script::GetInvoker();
    RUNTIME_ASSERT( func->GetReturnTypeId() == asTYPEID_VOID );
    return self->Invoke( delay, true, func, NULL );
}

uint ScriptInvoker::Global_SavedInvokeWithValues( asIScriptFunction* func, ScriptArray* values, uint delay )
{
    ScriptInvoker* self = Script::GetInvoker();
    RUNTIME_ASSERT( func->GetReturnTypeId() == asTYPEID_VOID );
    return self->Invoke( delay, true, func, values );
}

bool ScriptInvoker::Global_IsInvoking( uint id )
{
    ScriptInvoker* self = Script::GetInvoker();
    return self->IsInvoking( id );
}

bool ScriptInvoker::Global_CancelInvoke( uint id )
{
    ScriptInvoker* self = Script::GetInvoker();
    return self->CancelInvoke( id );
}

bool ScriptInvoker::Global_GetInvokeData( uint id, uint& delay, ScriptArray* values )
{
    ScriptInvoker* self = Script::GetInvoker();
    Invocation     invocation;
    if( self->GetInvokeData( id, invocation ) )
    {
        uint tick = Timer::FastTick();
        delay = ( invocation.FireTick > tick ? invocation.FireTick - tick : 0 );
        if( values && invocation.Values )
        {
            values->Clear();
            Script::AppendVectorToArray( *invocation.Values, values );
        }
        return true;
    }
    return false;
}

uint ScriptInvoker::Global_GetInvokeList( ScriptArray* ids )
{
    ScriptInvoker* self = Script::GetInvoker();
    IntVec         ids_;
    self->GetInvokeList( ids_ );
    if( ids )
        Script::AppendVectorToArray( ids_, ids );
    return (uint) ids_.size();
}

#else

uint ScriptInvoker::Global_Invoke( asIScriptFunction* func, uint delay )
{
    return 0;
}

uint ScriptInvoker::Global_InvokeWithValues( asIScriptFunction* func, ScriptArray* values, uint delay )
{
    return 0;
}

uint ScriptInvoker::Global_SavedInvoke( asIScriptFunction* func, uint delay )
{
    return 0;
}

uint ScriptInvoker::Global_SavedInvokeWithValues( asIScriptFunction* func, ScriptArray* values, uint delay )
{
    return 0;
}

bool ScriptInvoker::Global_IsInvoking( uint id )
{
    return false;
}

bool ScriptInvoker::Global_CancelInvoke( uint id )
{
    return false;
}

bool ScriptInvoker::Global_GetInvokeData( uint id, uint& delay, ScriptArray* values )
{
    return false;
}

uint ScriptInvoker::Global_GetInvokeList( ScriptArray* ids )
{
    return 0;
}

#endif
