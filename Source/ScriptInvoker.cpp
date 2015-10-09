#include "ScriptInvoker.h"
#include "Script.h"

ScriptInvoker::ScriptInvoker()
{
    lastDeferredCallId = 0;
}

uint ScriptInvoker::AddDeferredCall( uint delay, bool saved, asIScriptFunction* func, int* value, ScriptArray* values )
{
    if( !func )
        SCRIPT_ERROR_R0( "Function arg is null." );

    RUNTIME_ASSERT( func->GetReturnTypeId() == asTYPEID_VOID );
    uint func_num = Script::GetFuncNum( func );
    uint bind_id = Script::BindByFunc( func, false );
    RUNTIME_ASSERT( bind_id );

    DeferredCall call;
    call.Id = ++lastDeferredCallId;
    call.FireTick = Timer::FastTick() + delay;
    call.FuncNum = func_num;
    call.BindId = bind_id;
    call.Saved = saved;
    if( value )
    {
        call.IsValue = true;
        call.Value = *value;
    }
    else
    {
        call.IsValue = false;
        call.Value = 0;
    }
    if( values )
    {
        call.IsValues = true;
        Script::AssignScriptArrayInVector( call.Values, values );
        RUNTIME_ASSERT( values->GetElementTypeId() == asTYPEID_INT32 || values->GetElementTypeId() == asTYPEID_UINT32 );
        call.ValuesSigned = ( values->GetElementTypeId() == asTYPEID_INT32 );
    }
    else
    {
        call.IsValues = false;
        call.ValuesSigned = false;
    }

    if( delay == uint( -1 ) )
    {
        RunDeferredCall( call );
    }
    else
    {
        SCOPE_LOCK( deferredCallsLocker );
        deferredCalls.push_back( call );
    }
    return call.Id;
}

bool ScriptInvoker::IsDeferredCallPending( uint id )
{
    SCOPE_LOCK( deferredCallsLocker );

    for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
        if( it->Id == id )
            return true;
    return false;
}

bool ScriptInvoker::CancelDeferredCall( uint id )
{
    SCOPE_LOCK( deferredCallsLocker );

    for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
    {
        if( it->Id == id )
        {
            deferredCalls.erase( it );
            return true;
        }
    }
    return false;
}

bool ScriptInvoker::GetDeferredCallData( uint id, DeferredCall& data )
{
    SCOPE_LOCK( deferredCallsLocker );

    for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
    {
        DeferredCall& call = *it;
        if( call.Id == id )
        {
            data = call;
            return true;
        }
    }
    return false;
}

void ScriptInvoker::GetDeferredCallsList( IntVec& ids )
{
    SCOPE_LOCK( deferredCallsLocker );

    ids.reserve( deferredCalls.size() );
    for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
        ids.push_back( it->Id );
}

void ScriptInvoker::Process()
{
    bool done = false;
    while( !done )
    {
        done = true;

        deferredCallsLocker.Lock();

        uint tick = Timer::FastTick();
        for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
        {
            if( tick >= it->FireTick )
            {
                DeferredCall call = *it;
                it = deferredCalls.erase( it );
                deferredCallsLocker.Unlock();

                RunDeferredCall( call );
                done = false;
                break;
            }
        }

        if( done )
            deferredCallsLocker.Unlock();
    }
}

void ScriptInvoker::RunDeferredCall( DeferredCall& call )
{
    if( Script::PrepareContext( call.BindId, _FUNC_, "Invoker" ) )
    {
        if( call.IsValue )
        {
            Script::SetArgUInt( call.Value );
            Script::RunPrepared();
        }
        else if( call.IsValues )
        {
            ScriptArray* arr = Script::CreateArray( call.ValuesSigned ? "int[]" : "uint[]" );
            Script::AppendVectorToArray( call.Values, arr );
            Script::SetArgObject( arr );
            Script::RunPrepared();
            arr->Release();
        }
        else
        {
            Script::RunPrepared();
        }
    }
}

void ScriptInvoker::SaveDeferredCalls( void ( * save_func )( void*, size_t ) )
{
    SCOPE_LOCK( deferredCallsLocker );

    save_func( &lastDeferredCallId, sizeof( lastDeferredCallId ) );

    uint count = 0;
    for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
        if( it->Saved )
            count++;
    save_func( &count, sizeof( count ) );

    uint tick = Timer::FastTick();
    for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
    {
        DeferredCall& call = *it;
        if( !call.Saved )
            continue;

        save_func( &call.Id, sizeof( call.Id ) );
        save_func( &call.FuncNum, sizeof( call.FuncNum ) );
        uint delay = ( call.FireTick > tick ? call.FireTick - tick : 0 );
        save_func( &delay, sizeof( delay ) );
        save_func( &call.IsValue, sizeof( call.IsValue ) );
        if( call.IsValue )
            save_func( &call.Value, sizeof( call.Value ) );
        save_func( &call.IsValues, sizeof( call.IsValues ) );
        if( call.IsValues )
        {
            save_func( &call.ValuesSigned, sizeof( call.ValuesSigned ) );
            uint values_size = (uint) ( call.IsValues ? call.Values.size() : 0 );
            save_func( &values_size, sizeof( values_size ) );
            if( values_size )
                save_func( &call.Values[ 0 ], values_size * sizeof( int ) );
        }
    }
}

bool ScriptInvoker::LoadDeferredCalls( void* f, uint version )
{
    SCOPE_LOCK( deferredCallsLocker );

    WriteLog( "Load deferred calls...\n" );

    if( !FileRead( f, &lastDeferredCallId, sizeof( lastDeferredCallId ) ) )
        return false;

    uint count = 0;
    if( !FileRead( f, &count, sizeof( count ) ) )
        return false;

    uint         tick = Timer::FastTick();
    DeferredCall call;
    for( uint i = 0; i < count; i++ )
    {
        if( !FileRead( f, &call.Id, sizeof( call.Id ) ) )
            return false;
        if( !FileRead( f, &call.FuncNum, sizeof( call.FuncNum ) ) )
            return false;

        if( !FileRead( f, &call.FireTick, sizeof( call.FireTick ) ) )
            return false;
        call.FireTick += tick;

        if( !FileRead( f, &call.IsValue, sizeof( call.IsValue ) ) )
            return false;
        if( call.IsValue )
        {
            if( !FileRead( f, &call.Value, sizeof( call.Value ) ) )
                return false;
        }
        else
        {
            call.Value = 0;
        }

        if( !FileRead( f, &call.IsValues, sizeof( call.IsValues ) ) )
            return false;
        if( call.IsValues )
        {
            if( !FileRead( f, &call.ValuesSigned, sizeof( call.ValuesSigned ) ) )
                return false;
            uint values_size;
            if( !FileRead( f, &values_size, sizeof( values_size ) ) )
                return false;
            if( values_size )
            {
                call.Values.resize( values_size );
                if( !FileRead( f, &call.Values[ 0 ], values_size * sizeof( int ) ) )
                    return false;
            }
        }
        else
        {
            call.ValuesSigned = false;
            call.Values.clear();
        }

        call.BindId = Script::BindByFuncNum( call.FuncNum, false );
        if( !call.BindId )
        {
            WriteLog( "Unable to bind script function '%s' for event %u. Skip event.\n", Str::GetName( call.FuncNum ), call.Id );
            continue;
        }

        deferredCalls.push_back( call );
    }

    WriteLog( "Load deferred calls complete, count %u.\n", count );
    return true;
}

string ScriptInvoker::GetStatistics()
{
    SCOPE_LOCK( deferredCallsLocker );

    char   buf[ MAX_FOTEXT ];
    uint   tick = Timer::FastTick();
    string result = Str::Format( buf, "Deferred calls count: %u\n", (uint) deferredCalls.size() );
    result += "Id         Delay      Saved    Function                                                              Values\n";
    for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
    {
        DeferredCall& call = *it;
        string        func_name = Script::GetBindFuncName( call.BindId );
        result += Str::Format( buf, "%-10u %-10u %-8s %-70s", call.Id, tick < call.FireTick ? call.FireTick - tick : 0,
                               call.Saved ? "true" : "false", func_name.c_str() );
        if( call.IsValue )
        {
            result += "Single:";
            result += Str::Format( buf, " %u", call.Value );
        }
        else if( call.IsValues )
        {
            result += "Multiple:";
            for( size_t i = 0; i < call.Values.size(); i++ )
                result += Str::Format( buf, " %u", call.Values[ i ] );
        }
        else
        {
            result += "None";
        }
        result += "\n";
    }
    return result;
}

uint ScriptInvoker::Global_DeferredCall( uint delay, asIScriptFunction* func )
{
    #pragma MESSAGE( "Take Invoker from func." )
    return Script::GetInvoker()->AddDeferredCall( delay, false, func, nullptr, nullptr );
}

uint ScriptInvoker::Global_DeferredCallWithValue( uint delay, asIScriptFunction* func, int value )
{
    return Script::GetInvoker()->AddDeferredCall( delay, false, func, &value, nullptr );
}

uint ScriptInvoker::Global_DeferredCallWithValues( uint delay, asIScriptFunction* func, ScriptArray* values )
{
    return Script::GetInvoker()->AddDeferredCall( delay, false, func, nullptr, values );
}

uint ScriptInvoker::Global_SavedDeferredCall( uint delay, asIScriptFunction* func )
{
    return Script::GetInvoker()->AddDeferredCall( delay, true, func, nullptr, nullptr );
}

uint ScriptInvoker::Global_SavedDeferredCallWithValue( uint delay, asIScriptFunction* func, int value )
{
    return Script::GetInvoker()->AddDeferredCall( delay, true, func, &value, nullptr );
}

uint ScriptInvoker::Global_SavedDeferredCallWithValues( uint delay, asIScriptFunction* func, ScriptArray* values )
{
    return Script::GetInvoker()->AddDeferredCall( delay, true, func, nullptr, values );
}

bool ScriptInvoker::Global_IsDeferredCallPending( uint id )
{
    return Script::GetInvoker()->IsDeferredCallPending( id );
}

bool ScriptInvoker::Global_CancelDeferredCall( uint id )
{
    return Script::GetInvoker()->CancelDeferredCall( id );
}

bool ScriptInvoker::Global_GetDeferredCallData( uint id, uint& delay, ScriptArray* values )
{
    ScriptInvoker* self = Script::GetInvoker();
    DeferredCall   call;
    if( self->GetDeferredCallData( id, call ) )
    {
        uint tick = Timer::FastTick();
        delay = ( call.FireTick > tick ? call.FireTick - tick : 0 );
        if( values )
        {
            if( call.IsValue )
            {
                values->Resize( 1 );
                *(int*) values->At( 0 ) = call.Value;
            }
            else if( call.IsValues )
            {
                values->Clear();
                Script::AppendVectorToArray( call.Values, values );
            }
        }
        return true;
    }
    return false;
}

uint ScriptInvoker::Global_GetDeferredCallsList( ScriptArray* ids )
{
    ScriptInvoker* self = Script::GetInvoker();
    IntVec         ids_;
    self->GetDeferredCallsList( ids_ );
    if( ids )
        Script::AppendVectorToArray( ids_, ids );
    return (uint) ids_.size();
}
