#include "StdAfx.h"
#include "Utility.h"
#include "Log.h"
#include <io.h>
#include <algorithm>

// Output a string to Visual Studio's output pane
void Utility3d::DebugString(const std::string& str)
{
	WriteLog("Mesh debug message<%s>.\n",str.c_str());
}

// Check to see if a DirectX call failed. If it did output the error string.
bool Utility3d::FailedHr(HRESULT hr)
{
	if(FAILED(hr))
	{
		DebugString("DirectX Reported Error: "+ToString(DXGetErrorString(hr))+" - "+ToString(DXGetErrorDescription(hr)));
		return true;
	}
	return false;
}

char* Utility3d::DuplicateCharString(const char* c_str)
{
    if(!c_str) return NULL;

	size_t len=strlen(c_str)+1;
	char* str=new char[len];
	memcpy(str,c_str,len*sizeof(char));
	return str;
}
