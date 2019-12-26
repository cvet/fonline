#pragma once

#include "Common.h"

class IProgramExecutor;
using ProgramExecutor = shared_ptr<IProgramExecutor>;

class IProgramExecutor : public NonCopyable
{
public:
    static ProgramExecutor Execute(const string& path, const string& name, const StrVec& args);
    virtual string GetResult() = 0;
    virtual ~IProgramExecutor() = default;
};
