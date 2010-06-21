#pragma once

//TODO:
//Undo, Redo
//Comments /**/

#include <windows.h>

//#pragma comment(lib, "User32.lib")

#include "AngelScript/angelscript.h"
#include "AngelScript/scriptstring.h"
#include "AngelScript/Preprocessor/preprocess.h"

typedef int (WINAPI *DLLFunc)(asIScriptEngine *);

using namespace std;

#define PATH_INI		".\\ScriptEditor.ini"
#define PATH_INI_L		L".\\ScriptEditor.ini"

#define VERSION_STR		"1.0.33"

#define MAX_BUFFER		100

#define TEMP_NAME		"temp"
#define TEMP_FULL_NAME	"temp.temp"

#define TRUECHAR(a) \
((a>='a' && a<='z') || (a>='A' && a<='Z') || \
 (a>='à' && a<='ÿ') || (a>='À' && a<='ß') || \
 (a>='0' && a<='9') || \
 (a=='_') || (a=='#'))

#include <string>
#include <vector>



class CBytecodeStream : public asIBinaryStream
{
private:
	UINT wpos,rpos;
	std::vector<asBYTE> buffer;
public:
	UINT GetSize(){return buffer.size();}
	const BYTE* GetBuffer(){return buffer.size()>0?&buffer[0]:NULL;}
	void Write(const void *ptr, asUINT size) {if(!size) return; buffer.resize(buffer.size()+size); memcpy(&buffer[wpos], ptr, size); wpos+=size;}
	void Read(void *ptr, asUINT size) {memcpy(ptr,&buffer[rpos],size); rpos+=size;}
	void SetPos(UINT w, UINT r){wpos=w; rpos=r;}
	void Clear(){SetPos(0,0); buffer.clear();}
	CBytecodeStream(){wpos=0;rpos=0;}
};

#include "AngelScript/as_scriptengine.h"
#include "AngelScript/as_context.h"
#include "AngelScript/as_datatype.h"

struct ParamInfo
{
	asCDataType* type;
	__int64 valI;
	double valD;

	ParamInfo(asCDataType* _type){type=_type;valI=0;valD=0;}
};