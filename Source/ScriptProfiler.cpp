#include "ScriptProfiler.h"
#include "Script.h"

#ifndef FONLINE_CLIENT
static int ScriptsPath = PT_SERVER_SCRIPTS;
#else
static int ScriptsPath = PT_CACHE;
#endif

ScriptProfiler::ScriptProfiler()
{
    scriptEngine = NULL;
    curStage = ProfilerUninitialized;
    sampleInterval = 0;
    saveInterval = 0;
    saveFileHandle = NULL;
    lastSaveTime = 0;
    isDynamicDisplay = false;
    totalCallPaths = 0;
}

bool ScriptProfiler::Init( asIScriptEngine* engine, uint sample_time, uint save_time, bool dynamic_display )
{
    RUNTIME_ASSERT( curStage == ProfilerUninitialized );
    RUNTIME_ASSERT( engine );
    RUNTIME_ASSERT( sample_time > 0 );

    scriptEngine = engine;
    sampleInterval = sample_time;
    saveInterval = save_time;
    isDynamicDisplay = dynamic_display;

    if( !saveInterval && !isDynamicDisplay )
    {
        WriteLog( "Profiler may not be active with both saving and dynamic display disabled.\n" );
        return false;
    }

    if( saveInterval )
    {
        DateTimeStamp dt;
        Timer::GetCurrentDateTime( dt );

        char dump_file_path[ MAX_FOPATH ];
        Str::Copy( dump_file_path, FileManager::GetWritePath( "", PT_SERVER_PROFILER ) );

        char dump_file[ MAX_FOPATH ];
        Str::Format( dump_file, "%sProfiler_%04u.%02u.%02u_%02u-%02u-%02u.foprof",
                     dump_file_path, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second );

        saveFileHandle = FileOpen( dump_file, true );
        if( !saveFileHandle )
        {
            WriteLog( "Couldn't open profiler dump file '%s'.\n", dump_file );
            return false;
        }

        uint dummy = 0x10ADB10B;         // "Load blob"
        FileWrite( saveFileHandle, &dummy, 4 );
        dummy = 0;                       // Version
        FileWrite( saveFileHandle, &dummy, 4 );
    }

    curStage = ProfilerInitialized;
    return true;
}

void ScriptProfiler::AddModule( const char* module_name )
{
    RUNTIME_ASSERT( curStage == ProfilerInitialized );

    if( !saveInterval )
    {
        char fname_real[ MAX_FOPATH ];
        Str::Copy( fname_real, module_name );
        Str::Replacement( fname_real, '.', DIR_SLASH_C );
        Str::Append( fname_real, ".fosp" );
        FileManager::FormatPath( fname_real );

        FileManager file;
        file.LoadFile( fname_real, ScriptsPath );
        if( file.IsLoaded() )
        {
            FileWrite( saveFileHandle, module_name, Str::Length( module_name ) + 1 );
            FileWrite( saveFileHandle, file.GetBuf(), Str::Length( (char*) file.GetBuf() ) + 1 );
        }
    }
}

void ScriptProfiler::EndModules()
{
    RUNTIME_ASSERT( curStage == ProfilerInitialized );

    if( saveInterval )
    {
        int dummy = 0;
        FileWrite( saveFileHandle, &dummy, 1 );

        // TODO: Proper way
        for( int i = 1; i < 1000000; i++ )
        {
            asIScriptFunction* func = scriptEngine->GetFunctionById( i );
            if( !func )
                continue;

            char buf[ MAX_FOTEXT ] = { 0 };
            FileWrite( saveFileHandle, &i, 4 );

            Str::Format( buf, "%s", func->GetModuleName() );
            FileWrite( saveFileHandle, buf, Str::Length( buf ) + 1 );
            Str::Format( buf, "%s", func->GetDeclaration() );
            FileWrite( saveFileHandle, buf, Str::Length( buf ) + 1 );
        }

        dummy = -1;
        FileWrite( saveFileHandle, &dummy, 4 );
    }

    curStage = ProfilerWorking;
}

void ScriptProfiler::Process( asIScriptContext* ctx, uint& tick )
{
    RUNTIME_ASSERT( curStage == ProfilerWorking );

    if( ctx->GetState() != asEXECUTION_ACTIVE )
        return;

    uint cur_tick = Timer::FastTick();
    if( cur_tick - tick < sampleInterval )
        return;
    tick = cur_tick;

    CallStack*         stack = new CallStack();
    asIScriptFunction* func;
    int                line, column = 0;
    uint               stack_size = ctx->GetCallstackSize();
    for( uint j = 0; j < stack_size; j++ )
    {
        func = ctx->GetFunction( j );
        if( func )
        {
            line = ctx->GetLineNumber( j );
            stack->push_back( Call( func->GetId(), line ) );
        }
        else
        {
            stack->push_back( Call( 0, 0 ) );
        }
    }

    if( isDynamicDisplay )
        ProcessStack( stack );
    if( saveInterval )
        stacksToSave.push_back( stack );
    else
        delete stack;

    if( saveInterval && cur_tick - lastSaveTime >= saveInterval )
    {
        uint time = Timer::FastTick();
        uint size = (uint) stacksToSave.size();

        for( auto it = stacksToSave.begin(), end = stacksToSave.end(); it != end; ++it )
        {
            CallStack* stack = *it;
            for( auto it2 = stack->begin(), end2 = stack->end(); it2 != end2; ++it2 )
            {
                FileWrite( saveFileHandle, &it2->Id, 4 );
                FileWrite( saveFileHandle, &it2->Line, 4 );
            }
            static int dummy = -1;
            FileWrite( saveFileHandle, &dummy, 4 );
            delete *it;
        }

        stacksToSave.clear();
        lastSaveTime = cur_tick;
    }
}

void ScriptProfiler::ProcessStack( CallStack* stack )
{
    SCOPE_LOCK( callPathsLocker );

    totalCallPaths++;
    int       top_id = stack->back().Id;
    CallPath* path;

    auto      itp = callPaths.find( top_id );
    if( itp == callPaths.end() )
    {
        path = new CallPath( top_id );
        callPaths[ top_id ] = path;
    }
    else
    {
        path = itp->second;
        path->Incl++;
    }

    for( CallStack::reverse_iterator it = stack->rbegin() + 1, end = stack->rend(); it != end; ++it )
        path = path->AddChild( it->Id );

    path->StackEnd();
}

void ScriptProfiler::Finish()
{
    RUNTIME_ASSERT( curStage == ProfilerWorking );

    if( saveInterval )
    {
        FileClose( saveFileHandle );
        saveFileHandle = NULL;
    }

    curStage = ProfilerUninitialized;
}

// Helper
struct OutputLine
{
    string FuncName;
    uint   Depth;
    float  Incl;
    float  Excl;
    OutputLine( char* text, uint depth, float incl, float excl ): FuncName( text ), Depth( depth ), Incl( incl ), Excl( excl ) {}
};

static void TraverseCallPaths( asIScriptEngine* engine, CallPath* path, vector< OutputLine >& lines, uint depth, uint& max_depth, uint& max_len, float total_call_paths )
{
    asIScriptFunction* func = engine->GetFunctionById( path->Id );
    char               name[ MAX_FOTEXT ] = { 0 };
    if( func )
        Str::Format( name, "%s : %s", func->GetModuleName(), func->GetDeclaration() );
    else
        Str::Copy( name, 4, "???\0" );

    lines.push_back( OutputLine( name, depth, 100.0f * (float) path->Incl / total_call_paths, 100.0f * (float) path->Excl / total_call_paths ) );

    uint len = Str::Length( name ) + depth;
    if( len > max_len )
        max_len = len;
    depth += 2;
    if( depth > max_depth )
        max_depth = depth;

    for( auto it = path->Children.begin(), end = path->Children.end(); it != end; ++it )
        TraverseCallPaths( engine, it->second, lines, depth, max_depth, max_len, total_call_paths );
}

string ScriptProfiler::GetStatistics()
{
    if( !isDynamicDisplay )
        return "Dynamic display is disabled.";

    SCOPE_LOCK( callPathsLocker );

    if( !totalCallPaths )
        return "No calls recorded.";

    string               result;

    vector< OutputLine > lines;
    uint                 max_depth = 0;
    uint                 max_len = 0;
    for( auto it = callPaths.begin(), end = callPaths.end(); it != end; ++it )
        TraverseCallPaths( scriptEngine, it->second, lines, 0, max_depth, max_len, (float) totalCallPaths );

    char buf[ MAX_FOTEXT ] = { 0 };
    Str::Format( buf, "%-*s Inclusive %%  Exclusive %%\n\n", max_len, "" );
    result += buf;

    for( uint i = 0; i < lines.size(); i++ )
    {
        Str::Format( buf, "%*s%-*s   %6.2f       %6.2f\n", lines[ i ].Depth, "",
                     max_len - lines[ i ].Depth, lines[ i ].FuncName.c_str(),
                     lines[ i ].Incl, lines[ i ].Excl );
        result += buf;
    }

    return result;
}

CallPath* CallPath::AddChild( int id )
{
    auto it = Children.find( id );
    if( it != Children.end() )
        return it->second;

    CallPath* child = new CallPath( id );
    Children[ id ] = child;
    return child;
}

void CallPath::StackEnd()
{
    Excl++;
}
