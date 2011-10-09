//
// CScriptFile
//
// This class encapsulates a FILE pointer in a reference counted class for
// use within AngelScript.
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

#include <string>
#include <stdio.h>

#include "AngelScript/angelscript.h"
#include "scriptstring.h"
#include "scriptarray.h"

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
    int  Open(const std::string &filename, const std::string &mode);
    int  Close();
    int  GetSize() const;
    bool IsEOF() const;

    // Reading
    int      ReadString(unsigned int length, std::string &str);
    int      ReadLine(std::string &str);
    asINT64  ReadInt(asUINT bytes);
    asQWORD  ReadUInt(asUINT bytes);
    float    ReadFloat();
    double   ReadDouble();

    // Writing
    int WriteString(const std::string &str);
    int WriteInt(asINT64 v, asUINT bytes);
    int WriteUInt(asQWORD v, asUINT bytes);
    int WriteFloat(float v);
    int WriteDouble(double v);

    // Cursor
	int GetPos() const;
    int SetPos(int pos);
    int MovePos(int delta);

    // Big-endian = most significant byte first
    bool mostSignificantByteFirst;

    //////////////////////////////////////////////////////////////////////////
    CScriptString* ReadWord();
    int ReadNumber();
    unsigned char ReadUint8();
    unsigned short ReadUint16();
    unsigned int ReadUint32();
    asQWORD ReadUint64();
    unsigned int ReadData(unsigned int count, CScriptArray& data);
    bool WriteUint8(unsigned char data);
    bool WriteUint16(unsigned short data);
    bool WriteUint32(unsigned int data);
    bool WriteUint64(asQWORD data);
    bool WriteData(CScriptArray& data, unsigned int count);
    //////////////////////////////////////////////////////////////////////////

protected:
    ~CScriptFile();

    mutable int refCount;
    FILE       *file;
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
