#ifndef __RESOURCE_CONVERTER__
#define __RESOURCE_CONVERTER__

#include "Common.h"
#include "FileUtils.h"

class ResourceConverter
{
public:
    static bool Generate( StrVec* resource_names );

private:
    static File* Convert( const string& name, File& file );
    static File* ConvertImage( const string& name, File& file );
    static File* Convert3d( const string& name, File& file );
};

#endif // __RESOURCE_CONVERTER__
