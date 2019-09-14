#pragma once

#include "Common.h"

class IRandomizer
{
public:
    virtual int Next( int minimum, int maximum ) = 0;
    virtual ~IRandomizer() = default;
};
using Randomizer = std::shared_ptr< IRandomizer >;

namespace Fabric
{
    Randomizer CreateNullRandomizer();
    Randomizer CreateMersenneTwistRandomizer( uint seed = 0 );
}
