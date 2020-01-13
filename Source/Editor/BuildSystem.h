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

class BuildSystem : public NonCopyable
{
public:
    BuildSystem();
    ~BuildSystem();
    bool BuildAll();

private:
    bool GenerateResources(StrVec* resource_names);
    bool BuildTarget(string target);
    bool BuildWindows();
};
