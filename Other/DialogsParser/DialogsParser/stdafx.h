// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
#pragma once

// TODO: reference additional headers your program requires here

#include < vcclr.h >

#include <Windows.h>
#include <map>
#include <string>

using namespace std;

typedef map<DWORD,string,less<DWORD> > StrMap;
typedef map<string,StrMap*,less<string> > StrStrMap;

#define DATA_PATH	".\\data\\"
#define MSG_NAME	"FODLG.MSG"
#define LIST_NAME	"dialogs.lst"