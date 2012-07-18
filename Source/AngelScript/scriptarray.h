#ifndef SCRIPTARRAY_H
#define SCRIPTARRAY_H

#include "angelscript.h"

class ScriptArray
{
public:
    #ifdef FONLINE_DLL
    static ScriptArray& Create( const char* type )
    {
        static int   typeId = ASEngine->GetTypeIdByDecl( std::string( type ).append( "[]" ).c_str() );
        ScriptArray* scriptArray = (ScriptArray*) ASEngine->CreateScriptObject( typeId );
        return *scriptArray;
    }
protected:
    #endif

    ScriptArray();
    ScriptArray( const ScriptArray& );
    ScriptArray( asUINT length, asIObjectType* ot );
    ScriptArray( asUINT length, void* defVal, asIObjectType* ot );
    virtual ~ScriptArray();

public:
    virtual void AddRef() const;
    virtual void Release() const;

    virtual asIObjectType* GetArrayObjectType() const;
    virtual int            GetArrayTypeId() const;
    virtual int            GetElementTypeId() const;

    virtual void   Resize( asUINT numElements );
    virtual void   Grow( asUINT numElements );
    virtual void   Reduce( asUINT numElements );
    virtual asUINT GetSize() const;
    virtual int    GetElementSize() const;

    virtual void* At( asUINT index );
    virtual void* First();
    virtual void* Last();

    ScriptArray& operator=( const ScriptArray& other )
    {
        Assign( other );
        return *this;
    }
    virtual void Assign( const ScriptArray& other );

    virtual void InsertAt( asUINT index, void* value );
    virtual void RemoveAt( asUINT index );
    virtual void InsertFirst( void* value );
    virtual void RemoveFirst();
    virtual void InsertLast( void* value );
    virtual void RemoveLast();
    virtual void SortAsc();
    virtual void SortDesc();
    virtual void SortAsc( asUINT index, asUINT count );
    virtual void SortDesc( asUINT index, asUINT count );
    virtual void Sort( asUINT index, asUINT count, bool asc );
    virtual void Reverse();
    virtual int  Find( void* value );
    virtual int  Find( asUINT index, void* value );

    virtual int  GetRefCount();
    virtual void SetFlag();
    virtual bool GetFlag();
    virtual void EnumReferences( asIScriptEngine* engine );
    virtual void ReleaseAllHandles( asIScriptEngine* engine );

protected:
    struct ArrayBuffer
    {
        asDWORD numElements;
        asBYTE  data[ 1 ];
    };

    mutable int    refCount;
    mutable bool   gcFlag;
    asIObjectType* objType;
    ArrayBuffer*   buffer;
    int            elementSize;
    asIScriptFunction*            cmpFunc;
    asIScriptFunction*            eqFunc;
    int            subTypeId;

    virtual bool  Less( const void* a, const void* b, bool asc, asIScriptContext* ctx );
    virtual void* GetArrayItemPointer( int index );
    virtual void* GetDataPointer( void* buffer );
    virtual void  Copy( void* dst, void* src );
    virtual void  Precache();
    virtual bool  CheckMaxSize( asUINT numElements );
    virtual void  Resize( int delta, asUINT at );
    virtual void  SetValue( asUINT index, void* value );
    virtual void  CreateBuffer( ArrayBuffer** buf, asUINT numElements );
    virtual void  DeleteBuffer( ArrayBuffer* buf );
    virtual void  CopyBuffer( ArrayBuffer* dst, ArrayBuffer* src );
    virtual void  Construct( ArrayBuffer* buf, asUINT start, asUINT end );
    virtual void  Destruct( ArrayBuffer* buf, asUINT start, asUINT end );
    virtual bool  Equals( const void* a, const void* b, asIScriptContext* ctx );
};

#ifndef FONLINE_DLL
void RegisterScriptArray( asIScriptEngine* engine, bool defaultArray );
#endif

#endif
