// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
#pragma once

// TODO: reference additional headers your program requires here


#ifdef UNICODE
#undef UNICODE
#endif //UNICODE


#include <windows.h>
#include <stdio.h>

#define CFG_FILE			".\\FOnline.cfg"
#define CFG_FILE_APP_NAME	"Game Options"

#define LANG_RUS            (0)
#define LANG_ENG            (1)

#pragma warning (disable : 4996)