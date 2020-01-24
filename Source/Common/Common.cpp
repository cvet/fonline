#include "Common.h"
#include "Testing.h"

// Dummy symbols for web build to avoid linker errors
#ifdef FO_WEB
void* SDL_LoadObject(const char* sofile)
{
    throw UnreachablePlaceException("Unreachable place");
}

void* SDL_LoadFunction(void* handle, const char* name)
{
    throw UnreachablePlaceException("Unreachable place");
}

void SDL_UnloadObject(void* handle)
{
    throw UnreachablePlaceException("Unreachable place");
}
#endif
