#include "ScriptProfiler.h"
#include "DiskFileSystem.h"
#include "FileSystem.h"
#include "Log.h"
#include "Script.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"

ScriptProfiler::ScriptProfiler()
{
    scriptEngine = nullptr;
    curStage = ProfilerUninitialized;
    sampleInterval = 0;
    saveFileHandle = nullptr;
    isDynamicDisplay = false;
    totalCallPaths = 0;
}

bool ScriptProfiler::Init(asIScriptEngine* engine, uint sample_time, bool save_to_file, bool dynamic_display)
{
    RUNTIME_ASSERT(curStage == ProfilerUninitialized);
    RUNTIME_ASSERT(engine);
    RUNTIME_ASSERT(sample_time > 0);

    if (!save_to_file && !dynamic_display)
    {
        WriteLog("Profiler may not be active with both saving and dynamic display disabled.\n");
        return false;
    }

    if (save_to_file)
    {
        DateTimeStamp dt;
        Timer::GetCurrentDateTime(dt);

        string dump_file = File::GetWritePath(_str(
            "Profiler/Profiler_{}.{}.{}_{}-{}-{}.foprof", dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second));

        saveFileHandle = DiskFileSystem::OpenFile(dump_file, true);
        if (!saveFileHandle)
        {
            WriteLog("Couldn't open profiler dump file '{}'.\n", dump_file);
            return false;
        }

        uint dummy = 0x10ADB10B; // "Load blob"
        DiskFileSystem::WriteFile(saveFileHandle, &dummy, 4);
        dummy = 0; // Version
        DiskFileSystem::WriteFile(saveFileHandle, &dummy, 4);
    }

    scriptEngine = engine;
    sampleInterval = sample_time;
    isDynamicDisplay = dynamic_display;
    curStage = ProfilerInitialized;
    return true;
}

void ScriptProfiler::AddModule(const string& module_name, const string& script_code)
{
    RUNTIME_ASSERT(curStage == ProfilerInitialized);

    if (saveFileHandle)
    {
        DiskFileSystem::WriteFile(saveFileHandle, module_name.c_str(), (uint)module_name.length() + 1);
        DiskFileSystem::WriteFile(saveFileHandle, script_code.c_str(), (uint)script_code.length() + 1);
    }
}

void ScriptProfiler::EndModules()
{
    RUNTIME_ASSERT(curStage == ProfilerInitialized);

    if (saveFileHandle)
    {
        int dummy = 0;
        DiskFileSystem::WriteFile(saveFileHandle, &dummy, 1);

        // TODO: Proper way
        for (int i = 1; i < 1000000; i++)
        {
            asIScriptFunction* func = scriptEngine->GetFunctionById(i);
            if (!func)
                continue;

            DiskFileSystem::WriteFile(saveFileHandle, &i, 4);

            string buf = (func->GetModuleName() ? func->GetModuleName() : "");
            DiskFileSystem::WriteFile(saveFileHandle, buf.c_str(), (uint)buf.length() + 1);
            buf = func->GetDeclaration();
            DiskFileSystem::WriteFile(saveFileHandle, buf.c_str(), (uint)buf.length() + 1);
        }

        dummy = -1;
        DiskFileSystem::WriteFile(saveFileHandle, &dummy, 4);
    }

    curStage = ProfilerWorking;
}

void ScriptProfiler::Process(asIScriptContext* ctx)
{
    RUNTIME_ASSERT(curStage == ProfilerWorking);

    if (ctx->GetState() != asEXECUTION_ACTIVE)
        return;

    callStack.clear();
    asIScriptFunction* func;
    int line = 0;
    uint stack_size = ctx->GetCallstackSize();
    for (uint j = 0; j < stack_size; j++)
    {
        func = ctx->GetFunction(j);
        if (func)
        {
            line = ctx->GetLineNumber(j);
            callStack.push_back(Call(func->GetId(), line));
        }
        else
        {
            callStack.push_back(Call(0, 0));
        }
    }

    if (isDynamicDisplay)
        ProcessStack(callStack);

    if (saveFileHandle)
    {
        for (auto it = callStack.begin(), end = callStack.end(); it != end; ++it)
        {
            DiskFileSystem::WriteFile(saveFileHandle, &it->Id, 4);
            DiskFileSystem::WriteFile(saveFileHandle, &it->Line, 4);
        }
        static int dummy = -1;
        DiskFileSystem::WriteFile(saveFileHandle, &dummy, 4);
    }
}

void ScriptProfiler::ProcessStack(CallStack& stack)
{
    totalCallPaths++;
    int top_id = stack.back().Id;
    CallPath* path;

    auto itp = callPaths.find(top_id);
    if (itp == callPaths.end())
    {
        path = new CallPath(top_id);
        callPaths[top_id] = path;
    }
    else
    {
        path = itp->second;
        path->Incl++;
    }

    for (CallStack::reverse_iterator it = stack.rbegin() + 1, end = stack.rend(); it != end; ++it)
        path = path->AddChild(it->Id);

    path->StackEnd();
}

void ScriptProfiler::Finish()
{
    RUNTIME_ASSERT(curStage == ProfilerWorking);

    saveFileHandle = nullptr;
    curStage = ProfilerUninitialized;
}

// Helper
struct OutputLine
{
    string FuncName;
    uint Depth;
    float Incl;
    float Excl;
    OutputLine(const string& text, uint depth, float incl, float excl) :
        FuncName(text), Depth(depth), Incl(incl), Excl(excl)
    {
    }
};

static void TraverseCallPaths(asIScriptEngine* engine, CallPath* path, vector<OutputLine>& lines, uint depth,
    uint& max_depth, uint& max_len, float total_call_paths)
{
    asIScriptFunction* func = engine->GetFunctionById(path->Id);
    string name;
    if (func)
        name = _str("{} : {}", func->GetModuleName(), func->GetDeclaration());
    else
        name = "???";

    lines.push_back(OutputLine(name.c_str(), depth, 100.0f * (float)path->Incl / total_call_paths,
        100.0f * (float)path->Excl / total_call_paths));

    uint len = (uint)name.length() + depth;
    if (len > max_len)
        max_len = len;
    depth += 2;
    if (depth > max_depth)
        max_depth = depth;

    for (auto it = path->Children.begin(), end = path->Children.end(); it != end; ++it)
        TraverseCallPaths(engine, it->second, lines, depth, max_depth, max_len, total_call_paths);
}

string ScriptProfiler::GetStatistics()
{
    if (!isDynamicDisplay)
        return "Dynamic display is disabled.";

    if (!totalCallPaths)
        return "No calls recorded.";

    string result;

    vector<OutputLine> lines;
    uint max_depth = 0;
    uint max_len = 0;
    for (auto it = callPaths.begin(), end = callPaths.end(); it != end; ++it)
        TraverseCallPaths(scriptEngine, it->second, lines, 0, max_depth, max_len, (float)totalCallPaths);

    result += _str("{} Inclusive %  Exclusive %\n\n", max_len);

    for (uint i = 0; i < lines.size(); i++)
    {
        result += _str("{}{}   {}       {}\n", lines[i].Depth, "", max_len - lines[i].Depth, lines[i].FuncName,
            lines[i].Incl, lines[i].Excl);
    }

    return result;
}

CallPath* CallPath::AddChild(int id)
{
    auto it = Children.find(id);
    if (it != Children.end())
        return it->second;

    CallPath* child = new CallPath(id);
    Children[id] = child;
    return child;
}

void CallPath::StackEnd()
{
    Excl++;
}
