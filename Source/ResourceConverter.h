#ifndef __RESOURCE_CONVERTER__
#define __RESOURCE_CONVERTER__

#include "Common.h"

class ResourceConverter
{
public:
    static bool Generate();

private:
    static FileManager* Convert( const char* name, FileManager& file );
    static FileManager* ConvertImage( const char* name, FileManager& file );
    static FileManager* Convert3d( const char* name, FileManager& file );
};

#endif // __RESOURCE_CONVERTER__
