#ifndef SCRIPTSTRING_H
#define SCRIPTSTRING_H

#include "angelscript.h"
#include <string>

typedef unsigned int uint;

class ScriptString
{
public:
    #ifdef FONLINE_DLL
    static ScriptString& Create( const char* str = NULL )
    {
        static asIObjectType* ot = ASEngine->GetObjectTypeByDecl( "string" );
        ScriptString*         scriptStr = (ScriptString*) ASEngine->CreateScriptObject( ot );
        if( str )
            scriptStr->assign( str );
        return *scriptStr;
    }
    #else
    static ScriptString* Create();
    static ScriptString* Create( const ScriptString& other );
    static ScriptString* Create( const char* s, uint len );
    static ScriptString* Create( const char* s );
    static ScriptString* Create( const std::string& s );
    #endif

    virtual void AddRef() const;
    virtual void Release() const;

    ScriptString& operator=( const ScriptString& other )
    {
        assign( other.c_str(), (uint) other.length() );
        return *this;
    }
    ScriptString& operator+=( const ScriptString& other )
    {
        append( other.c_str(), (uint) other.length() );
        return *this;
    }
    ScriptString& operator=( const std::string& other )
    {
        assign( other.c_str(), (uint) other.length() );
        return *this;
    }
    ScriptString& operator+=( const std::string& other )
    {
        append( other.c_str(), (uint) other.length() );
        return *this;
    }
    ScriptString& operator=( const char* other )
    {
        assign( other );
        return *this;
    }
    ScriptString& operator+=( const char* other )
    {
        append( other );
        return *this;
    }

    virtual void  assign( const char* buf, uint count );
    virtual void  assign( const char* buf );
    virtual void  append( const char* buf, uint count );
    virtual void  append( const char* buf );
    virtual void  reserve( uint count );
    virtual void  rawResize( uint count );
    virtual void  clear();
    virtual uint  lengthUTF8() const;
    virtual bool  indexUTF8ToRaw( int& index, uint* length = NULL, uint offset = 0 );
    virtual int   indexRawToUTF8( int index );
    virtual int   toInt( int defaultValue ) const;
    virtual float toFloat( float defaultValue ) const;
    virtual bool  startsWith( const ScriptString& str ) const;
    virtual bool  endsWith( const ScriptString& str ) const;

    const char*        c_str()     const { return buffer.c_str(); }
    uint               length()    const { return (uint) buffer.length(); }
    uint               capacity()  const { return (uint) buffer.capacity(); }
    const std::string& c_std_str() const { return buffer; }
    int                rcount()    const { return refCount; }

    char rawGet( uint index )
    {
        return index < buffer.length() ? buffer[ index ] : 0;
    }
    void rawSet( uint index, char value )
    {
        if( index < buffer.length() )
            buffer[ index ] = value;
    }

protected:
    static ScriptString* GetFromPool();
    static void          PutToPool( ScriptString* str );
    ScriptString();
    virtual ~ScriptString();

    std::string buffer;
    mutable int refCount;
};

#ifndef FONLINE_DLL
void RegisterScriptString( asIScriptEngine* engine );
#endif

#endif // SCRIPTSTRING_H
