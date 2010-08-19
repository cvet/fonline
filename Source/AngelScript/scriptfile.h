//
// CScriptFile
//
// This class encapsulates a FILE pointer in a reference counted class for
// use within AngelScript.
//
// It requires the CScriptString add-on to work.
//

#ifndef SCRIPTFILE_H
#define SCRIPTFILE_H

//---------------------------
// Compilation settings
//

// Set this flag to turn on/off write support
//  0 = off
//  1 = on

#ifndef AS_WRITE_OPS
#define AS_WRITE_OPS 1
#endif




//---------------------------
// Declaration
//

#include <AngelScript/angelscript.h>
#include "scriptstring.h"

BEGIN_AS_NAMESPACE

class CScriptFile
{
public:
    CScriptFile();

    void AddRef() const;
    void Release() const;

	// TODO: Implement the "r+", "w+" and "a+" modes
	// mode = "r" -> open the file for reading
	//        "w" -> open the file for writing (overwrites existing file)
	//        "a" -> open the file for appending
    int  Open(const CScriptString &filename, const CScriptString &mode);
    int  Close();
    int  GetSize() const;
	bool IsEOF() const;

    // Reading
    int ReadString(unsigned int length, CScriptString &str);
	int ReadLine(CScriptString &str);

	CScriptString* ReadWord();
	int ReadNumber();

    // Writing
    int WriteString(const CScriptString &str);

    // Cursor
	int GetPos() const;
    int SetPos(int pos);
    int MovePos(int delta);

protected:
    ~CScriptFile();

    mutable int refCount;
    FILE *file;
};

// This function will determine the configuration of the engine
// and use one of the two functions below to register the file type
void RegisterScriptFile(asIScriptEngine *engine);

// Call this function to register the file type
// using native calling conventions
void RegisterScriptFile_Native(asIScriptEngine *engine);

// Use this one instead if native calling conventions
// are not supported on the target platform
void RegisterScriptFile_Generic(asIScriptEngine *engine);

END_AS_NAMESPACE

#endif
