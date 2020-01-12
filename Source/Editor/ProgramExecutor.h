#pragma once

#include "Common.h"

class ProgramExecutor : public NonCopyable
{
public:
    ProgramExecutor(const string& path, const string& name, const StrVec& args);
    ~ProgramExecutor();
    string GetResult();

private:
    string ExecuteAsync();

    string programPath;
    string programName;
    StrVec programArgs;
    std::future<string> returnOutput;
};
