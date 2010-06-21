// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
#pragma once

// TODO: reference additional headers your program requires here

#define SAFEDEL(x) {if(x) delete (x);(x)=NULL;}
#define SAFEDELA(x) {if(x) delete[] (x);(x)=NULL;}

#define BREAK_BEGIN do{
#define BREAK_END }while(0)

#ifdef UNICODE
#undef UNICODE
#endif

#include <windows.h>
#include <crtdbg.h>
#include <stdio.h>

#include <map>
#include <string>
#include <set>
#include <list>
#include <vector>
using namespace std;

#include <mysql.h>

#pragma comment(lib,"wsock32.lib")
#pragma comment(lib,"libmySQL.lib")
#pragma comment(lib,"user32.lib")

#pragma warning (disable : 4996)
#pragma warning (disable : 4018)