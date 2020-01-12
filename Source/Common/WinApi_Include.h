// WinApi universal header
#ifdef FO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Extra headers
#include "SDL_syswm.h"
#include <WinSock2.h>
#include <shellapi.h>

// Undef collisions
#undef CopyFile
#undef DeleteFile
#undef PlaySound
#undef MessageBox
#undef GetObject
#undef LoadImage
#endif
