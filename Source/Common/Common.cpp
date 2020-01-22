#include "Common.h"
#include "Testing.h"

// Dummy symbols for web build to avoid linker errors
#ifdef FO_WEB
void* SDL_LoadObject(const char* sofile)
{
    UNREACHABLE_PLACE;
}

void* SDL_LoadFunction(void* handle, const char* name)
{
    UNREACHABLE_PLACE;
}

void SDL_UnloadObject(void* handle)
{
    UNREACHABLE_PLACE;
}
#endif
