//
// ScriptFile
//
// This class encapsulates a FILE pointer in a reference counted class for
// use within AngelScript.
//

#ifndef SCRIPTFILE_H
#define SCRIPTFILE_H

#include "angelscript.h"
#include "scriptstring.h"
#include "scriptarray.h"
#include <string>
#include <stdio.h>

class ScriptFile
{
public:
    #ifdef FONLINE_DLL
    static ScriptFile& Create()
    {
        static int  typeId = ASEngine->GetTypeIdByDecl( "file" );
        ScriptFile* scriptFile = (ScriptFile*) ASEngine->CreateScriptObject( typeId );
        return *scriptFile;
    }
protected:
    #endif

    ScriptFile();
    ScriptFile( const ScriptFile& );

public:
    virtual void AddRef() const;
    virtual void Release() const;

    // TODO: Implement the "r+", "w+" and "a+" modes
    // mode = "r" -> open the file for reading
    //        "w" -> open the file for writing (overwrites existing file)
    //        "a" -> open the file for appending
    virtual int  Open( const ScriptString& filename, const ScriptString& mode );
    virtual int  Close();
    virtual int  GetSize() const;
    virtual bool IsEOF() const;

    // Reading
    virtual int     ReadString( unsigned int length, ScriptString& str );
    virtual int     ReadLine( ScriptString& str );
    virtual asINT64 ReadInt( asUINT bytes );
    virtual asQWORD ReadUInt( asUINT bytes );
    virtual float   ReadFloat();
    virtual double  ReadDouble();

    // Writing
    virtual int WriteString( const ScriptString& str );
    virtual int WriteInt( asINT64 v, asUINT bytes );
    virtual int WriteUInt( asQWORD v, asUINT bytes );
    virtual int WriteFloat( float v );
    virtual int WriteDouble( double v );

    // Cursor
    virtual int GetPos() const;
    virtual int SetPos( int pos );
    virtual int MovePos( int delta );

    // Big-endian = most significant byte first
    bool mostSignificantByteFirst;

    virtual ScriptString*  ReadWord();
    virtual int            ReadNumber();
    virtual unsigned char  ReadUint8();
    virtual unsigned short ReadUint16();
    virtual unsigned int   ReadUint32();
    virtual asQWORD        ReadUint64();
    virtual unsigned int   ReadData( unsigned int count, ScriptArray& data );
    virtual bool           WriteUint8( unsigned char data );
    virtual bool           WriteUint16( unsigned short data );
    virtual bool           WriteUint32( unsigned int data );
    virtual bool           WriteUint64( asQWORD data );
    virtual bool           WriteData( ScriptArray& data, unsigned int count );

protected:
    virtual ~ScriptFile();

    mutable int refCount;
    FILE*       file;
};

#ifndef FONLINE_DLL
void RegisterScriptFile( asIScriptEngine* engine );
#endif

#endif
