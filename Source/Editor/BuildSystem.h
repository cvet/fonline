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

class IBuildSystem
{
public:
    virtual bool BuildAll() = 0;
    virtual ~IBuildSystem() {}
};
using BuildSystem = std::shared_ptr< IBuildSystem >;

namespace Fabric
{
    BuildSystem CreateDefaultBuildSystem();
}
