#include "System.h"

class DefaultSystem: public ISystem
{
public:
    virtual bool Call( const string& path, const StrVec& args ) override
    {
        return false;
    }
};

System Fabric::CreateDefaultSystem()
{
    return std::make_shared< DefaultSystem >();
}
