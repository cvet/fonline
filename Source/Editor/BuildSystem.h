#pragma once

#include "Common.h"

enum class BuildTarget
{
    Windows,
    Linux,
    macOS,
    iOS,
    Android,
};

class IBuildSystem;
using BuildSystem = std::shared_ptr<IBuildSystem>;

class IBuildSystem : public NonCopyable
{
public:
    static BuildSystem Create();
    virtual bool BuildAll() = 0;
    virtual ~IBuildSystem() = default;
};
