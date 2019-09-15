#pragma once

#include "Common.h"

class ISystem
{
public:
    virtual bool Call( const string& program, const StrVec& args ) = 0;
    virtual ~ISystem() = default;
};
using System = std::shared_ptr< ISystem >;

namespace Fabric
{
    System CreateDefaultSystem();
}
