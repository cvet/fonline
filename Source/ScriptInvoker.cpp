#include "ScriptInvoker.h"
#include "Script.h"

ScriptInvoker::ScriptInvoker()
{
    lastDeferredCallId = 0;
}

uint ScriptInvoker::AddDeferredCall( uint delay, bool saved, asIScriptFunction* func, int* value, CScriptArray* values )
{
    if( !func )
        SCRIPT_ERROR_R0( "Function arg is null." );

    RUNTIME_ASSERT( func->GetReturnTypeId() == asTYPEID_VOID );
    uint func_num = Script::GetFuncNum( func );
    uint bind_id = Script::BindByFunc( func, false );
    RUNTIME_ASSERT( bind_id );

    DeferredCall call;
    call.Id = 0;
    call.FireTick = ( delay ? Timer::FastTick() + delay : 0 );
    call.FuncNum = func_num;
    call.BindId = bind_id;
    call.Saved = saved;
    if( value )
    {
        call.IsValue = true;
        call.Value = *value;
        int value_type_id = 0;
        func->GetParam( 0, &value_type_id );
        RUNTIME_ASSERT( value_type_id == asTYPEID_INT32 || value_type_id == asTYPEID_UINT32 );
        call.ValueSigned = ( value_type_id == asTYPEID_INT32 );
    }
    else
    {
        call.IsValue = false;
        call.Value = 0;
        call.ValueSigned = false;
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

    if( delay == 0 )
    {
        RunDeferredCall( call );
    }
    else
    {
        call.Id = ++lastDeferredCallId;
        deferredCalls.push_back( call );
    }
    return call.Id;
}

bool ScriptInvoker::IsDeferredCallPending( uint id )
{
    for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
        if( it->Id == id )
            return true;
    return false;
}

bool ScriptInvoker::CancelDeferredCall( uint id )
{
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

        uint tick = Timer::FastTick();
        for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
        {
            RUNTIME_ASSERT( it->FireTick != 0 );
            if( tick >= it->FireTick )
            {
                DeferredCall call = *it;
                it = deferredCalls.erase( it );
                RunDeferredCall( call );
                done = false;
                break;
            }
        }
    }
}

void ScriptInvoker::RunDeferredCall( DeferredCall& call )
{
    Script::PrepareContext( call.BindId, "Invoker" );

    CScriptArray* arr = nullptr;

    if( call.IsValue )
    {
        Script::SetArgUInt( call.Value );
    }
    else if( call.IsValues )
    {
        arr = Script::CreateArray( call.ValuesSigned ? "int[]" : "uint[]" );
        Script::AppendVectorToArray( call.Values, arr );
        Script::SetArgObject( arr );
    }

    if( call.FireTick == 0 )
        Script::RunPreparedSuspend();
    else
        Script::RunPrepared();

    if( arr )
        arr->Release();

}

void ScriptInvoker::SaveDeferredCalls( IniParser& data )
{
    data.SetStr( "GeneralSettings", "LastDeferredCallId", _str( "{}", lastDeferredCallId ).c_str() );

    uint tick = Timer::FastTick();
    for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
    {
        DeferredCall& call = *it;
        RUNTIME_ASSERT( call.FireTick != 0 );
        if( !call.Saved )
            continue;

        StrMap& kv = data.SetApp( "DeferredCall" );

        kv[ "Id" ] = _str( "{}", call.Id );
        kv[ "Script" ] = _str().parseHash( call.FuncNum );
        kv[ "Delay" ] = _str( "{}", call.FireTick > tick ? call.FireTick - tick : 0 );

        if( call.IsValue )
        {
            kv[ "ValueSigned" ] = ( call.ValueSigned ? "true" : "false" );
            kv[ "Value" ] = _str( "{}", call.Value );
        }

        if( call.IsValues )
        {
            kv[ "ValuesSigned" ] = ( call.ValuesSigned ? "true" : "false" );
            string values;
            for( int v : call.Values )
                values.append( _str( "{}", v ) ).append( " " );
            kv[ "Values" ] = values;
        }
    }
}

bool ScriptInvoker::LoadDeferredCalls( IniParser& data )
{
    WriteLog( "Load deferred calls...\n" );

    lastDeferredCallId = _str( data.GetStr( "GeneralSettings", "LastDeferredCallId" ) ).toUInt();

    int          errors = 0;
    PStrMapVec   deferred_calls;
    data.GetApps( "DeferredCall", deferred_calls );
    uint         tick = Timer::FastTick();
    DeferredCall call;
    for( auto& pkv : deferred_calls )
    {
        auto& kv = *pkv;

        call.Id = _str( kv[ "Id" ] ).toUInt();
        call.FireTick = tick + _str( kv[ "Delay" ] ).toUInt();
        RUNTIME_ASSERT( call.FireTick != 0 );

        call.IsValue = ( kv.count( "Value" ) > 0 );
        if( call.IsValue )
        {
            call.ValueSigned = Str::CompareCase( kv[ "ValueSigned" ].c_str(), "true" );
            call.Value = _str( kv[ "Value" ] ).toInt();
        }
        else
        {
            call.ValueSigned = false;
            call.Value = 0;
        }

        call.IsValues = ( kv.count( "Values" ) > 0 );
        if( call.IsValues )
        {
            call.ValuesSigned = Str::CompareCase( kv[ "ValuesSigned" ].c_str(), "true" );
            call.Values = Str::SplitToInt( kv[ "Values" ], ' ' );
        }
        else
        {
            call.ValuesSigned = false;
            call.Values.clear();
        }

        if( call.IsValue && call.IsValues )
        {
            WriteLog( "Deferred call {} have value and values.\n", call.Id );
            errors++;
            continue;
        }

        const char* decl;
        if( call.IsValue && call.ValueSigned )
            decl = "void %s(int)";
        else if( call.IsValue && !call.ValueSigned )
            decl = "void %s(uint)";
        else if( call.IsValues && call.ValuesSigned )
            decl = "void %s(int[]&)";
        else if( call.IsValues && !call.ValuesSigned )
            decl = "void %s(uint[]&)";
        else
            decl = "void %s()";

        call.FuncNum = Script::BindScriptFuncNumByFuncName( kv[ "Script" ].c_str(), decl );
        if( !call.FuncNum )
        {
            WriteLog( "Unable to find function '{}' with declaration '{}' for deferred call {}.\n", kv[ "Script" ].c_str(), decl, call.Id );
            errors++;
            continue;
        }

        call.BindId = Script::BindByFuncNum( call.FuncNum, false );
        if( !call.BindId )
        {
            WriteLog( "Unable to bind script function '{}' for deferred call {}.\n", _str().parseHash( call.FuncNum ), call.Id );
            errors++;
            continue;
        }

        call.Saved = true;
        deferredCalls.push_back( call );
    }

    WriteLog( "Load deferred calls complete, count {}.\n", (uint) deferredCalls.size() );
    return errors == 0;
}

string ScriptInvoker::GetStatistics()
{
    uint   tick = Timer::FastTick();
    string result = _str( "Deferred calls count: {}\n", (uint) deferredCalls.size() );
    result += "Id         Delay      Saved    Function                                                              Values\n";
    for( auto it = deferredCalls.begin(); it != deferredCalls.end(); ++it )
    {
        DeferredCall& call = *it;
        string        func_name = Script::GetBindFuncName( call.BindId );
        result += _str( "{:<10} {:<10} {:<8} {:<70}", call.Id, tick < call.FireTick ? call.FireTick - tick : 0,
                        call.Saved ? "true" : "false", func_name.c_str() );
        if( call.IsValue )
        {
            result += "Single:";
            result += _str( " {}", call.Value );
        }
        else if( call.IsValues )
        {
            result += "Multiple:";
            for( size_t i = 0; i < call.Values.size(); i++ )
                result += _str( " {}", call.Values[ i ] );
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

uint ScriptInvoker::Global_DeferredCallWithValues( uint delay, asIScriptFunction* func, CScriptArray* values )
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

uint ScriptInvoker::Global_SavedDeferredCallWithValues( uint delay, asIScriptFunction* func, CScriptArray* values )
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

bool ScriptInvoker::Global_GetDeferredCallData( uint id, uint& delay, CScriptArray* values )
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
                values->Resize( 0 );
                Script::AppendVectorToArray( call.Values, values );
            }
        }
        return true;
    }
    return false;
}

uint ScriptInvoker::Global_GetDeferredCallsList( CScriptArray* ids )
{
    ScriptInvoker* self = Script::GetInvoker();
    IntVec         ids_;
    self->GetDeferredCallsList( ids_ );
    if( ids )
        Script::AppendVectorToArray( ids_, ids );
    return (uint) ids_.size();
}
