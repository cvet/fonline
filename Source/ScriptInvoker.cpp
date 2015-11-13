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

void ScriptInvoker::SaveDeferredCalls( IniParser& data )
{
    SCOPE_LOCK( deferredCallsLocker );

    data.SetStr( "GeneralSettings", "LastDeferredCallId", Str::UItoA( lastDeferredCallId ) );

    uint tick = Timer::FastTick();
    for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
    {
        DeferredCall& call = *it;
        if( !call.Saved )
            continue;

        StrMap& kv = data.SetApp( "DeferredCall" );

        kv[ "Id" ] = Str::UItoA( call.Id );
        kv[ "Script" ] = Str::GetName( call.FuncNum );
        kv[ "Delay" ] = Str::UItoA( call.FireTick > tick ? call.FireTick - tick : 0 );

        if( call.IsValue )
            kv[ "Value" ] = Str::ItoA( call.Value );

        if( call.IsValues )
        {
            kv[ "ValuesSigned" ] = ( call.ValuesSigned ? "true" : "false" );
            string values;
            for( int v : call.Values )
                values.append( Str::ItoA( v ) ).append( " " );
            kv[ "Values" ] = values;
        }
    }
}

bool ScriptInvoker::LoadDeferredCalls( IniParser& data )
{
    SCOPE_LOCK( deferredCallsLocker );

    WriteLog( "Load deferred calls...\n" );

    lastDeferredCallId = Str::AtoUI( data.GetStr( "GeneralSettings", "LastDeferredCallId" ) );

    PStrMapVec   deferred_calls;
    data.GetApps( "DeferredCall", deferred_calls );
    uint         tick = Timer::FastTick();
    DeferredCall call;
    for( auto& pkv : deferred_calls )
    {
        auto& kv = *pkv;

        call.Id = Str::AtoUI( kv[ "Id" ].c_str() );
        call.FuncNum = Script::BindScriptFuncNumByScriptName( kv[ "Script" ].c_str(), "TODO" );
        call.FireTick = tick + Str::AtoUI( kv[ "Delay" ].c_str() );

        if( kv.count( "Value" ) )
            call.Value = Str::AtoI( kv[ "Value" ].c_str() );
        else
            call.Value = 0;

        if( kv.count( "Values" ) )
        {
            call.ValuesSigned = Str::CompareCase( kv[ "ValuesSigned" ].c_str(), "true" );
            Str::ParseLine( kv[ "Values" ].c_str(), ' ', call.Values, Str::AtoI );
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

    WriteLog( "Load deferred calls complete, count %u.\n", (uint) deferredCalls.size() );
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
