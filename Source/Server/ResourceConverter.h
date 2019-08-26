#ifndef __RESOURCE_CONVERTER__
#define __RESOURCE_CONVERTER__

#include "Common.h"
#include "FileManager.h"

class ResourceConverter
{
public:
    static bool Generate( StrVec* resource_names );

private:
    static FileManager* Convert( const string& name, FileManager& file );
    static FileManager* ConvertImage( const string& name, FileManager& file );
    static FileManager* Convert3d( const string& name, FileManager& file );
};

#endif // __RESOURCE_CONVERTER__
