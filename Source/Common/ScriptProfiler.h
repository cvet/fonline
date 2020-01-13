#pragma once

#include "Common.h"

class asIScriptEngine;
class asIScriptContext;
namespace DiskFileSystem
{
    struct DiskFile;
}

struct Call
{
    Call() : Id(0), Line(0) {}
    Call(int id, uint line) : Id(id), Line(line) {}
    int Id;
    uint Line;
};
typedef vector<Call> CallStack;

struct CallPath
{
    uint Id;
    map<int, CallPath*> Children;
    uint Incl;
    uint Excl;
    CallPath(int id) : Id(id), Incl(1), Excl(0) {}
    CallPath* AddChild(int id);
    void StackEnd();
};
typedef map<int, CallPath*> IntCallPathMap;

class ScriptProfiler
{
    friend class Script;

private:
    enum ProfilerStage
    {
        ProfilerUninitialized = 0,
        ProfilerInitialized = 1,
        ProfilerWorking = 2,
    };

    asIScriptEngine* scriptEngine;
    ProfilerStage curStage;
    uint sampleInterval;
    CallStack callStack;

    // Save stacks
    shared_ptr<DiskFileSystem::DiskFile> saveFileHandle;

    // Dynamic display
    bool isDynamicDisplay;
    IntCallPathMap callPaths;
    uint totalCallPaths;

    ScriptProfiler();
    bool Init(asIScriptEngine* engine, uint sample_time, bool save_to_file, bool dynamic_display);
    void AddModule(const string& module_name, const string& script_code);
    void EndModules();
    void Process(asIScriptContext* ctx);
    void ProcessStack(CallStack& stack);
    void Finish();
    string GetStatistics();
};
