#ifndef ___STDAFX_H___
#define ___STDAFX_H___

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>

#pragma warning (disable : 4786)

#include <map>
#include <vector> //!Cvet
using namespace std;

#define SAFEDEL(x) {if(x) delete (x);(x)=NULL;}
#define SAFEDELA(x) {if(x) delete[] (x);(x)=NULL;}

#endif //___STDAFX_H___
