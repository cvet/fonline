#ifndef SCRIPTARRAY_H
#define SCRIPTARRAY_H

#include "angelscript.h"

struct ArrayBuffer
{
    asDWORD maxElements;
    asDWORD numElements;
    asBYTE  data[ 1 ];
};

struct ArrayCache
{
    asIScriptFunction* cmpFunc;
    asIScriptFunction* eqFunc;
    int                cmpFuncReturnCode; // To allow better error message in case of multiple matches
    int                eqFuncReturnCode;
};

class ScriptArray
{
public:
    #ifdef FONLINE_DLL
    static ScriptArray& Create( const char* type )
    {
        static asIObjectType* ot = ASEngine->GetObjectTypeByDecl( std::string( type ).append( "[]" ).c_str() );
        ScriptArray*          scriptArray = (ScriptArray*) ASEngine->CreateScriptObject( ot );
        return *scriptArray;
    }
protected:
    #endif

    #ifndef FONLINE_DLL
    // Set the memory functions that should be used by all ScriptArrays
    static void SetMemoryFunctions( asALLOCFUNC_t allocFunc, asFREEFUNC_t freeFunc );

    // Factory functions
    static ScriptArray* Create( asIObjectType* ot );
    static ScriptArray* Create( asIObjectType* ot, asUINT length );
    static ScriptArray* Create( asIObjectType* ot, asUINT length, void* defaultValue );
    static ScriptArray* Create( asIObjectType* ot, void* listBuffer );
    #endif

public:
    // Memory management
    virtual void AddRef() const;
    virtual void Release() const;

    // Type information
    virtual asIObjectType* GetArrayObjectType() const;
    virtual int            GetArrayTypeId() const;
    virtual int            GetElementTypeId() const;

    // Get the current size
    virtual asUINT GetSize() const;
    virtual int    GetElementSize() const;

    // Returns true if the array is empty
    virtual bool IsEmpty() const;

    // Pre-allocates memory for elements
    virtual void Reserve( asUINT maxElements );

    // Resize the array
    virtual void Resize( asUINT numElements );
    virtual void Grow( asUINT numElements );
    virtual void Reduce( asUINT numElements );

    // Get a pointer to an element. Returns 0 if out of bounds
    virtual void*       At( asUINT index );
    virtual const void* At( asUINT index ) const;
    virtual void*       First();
    virtual void*       Last();

    // Set value of an element.
    // The value arg should be a pointer to the value that will be copied to the element.
    // Remember, if the array holds handles the value parameter should be the
    // address of the handle. The refCount of the object will also be incremented
    virtual void SetValue( asUINT index, void* value );

    // Copy the contents of one array to another (only if the types are the same)
    virtual ScriptArray& operator=( const ScriptArray& other );

    // Compare two arrays
    virtual bool operator==( const ScriptArray& ) const;

    // Array manipulation
    virtual void InsertAt( asUINT index, void* value );
    virtual void RemoveAt( asUINT index );
    virtual void InsertFirst( void* value );
    virtual void RemoveFirst();
    virtual void InsertLast( void* value );
    virtual void RemoveLast();
    virtual void SortAsc();
    virtual void SortDesc();
    virtual void SortAsc( asUINT startAt, asUINT count );
    virtual void SortDesc( asUINT startAt, asUINT count );
    virtual void Sort( asUINT startAt, asUINT count, bool asc );
    virtual void Reverse();
    virtual int  Find( void* value ) const;
    virtual int  Find( asUINT startAt, void* value ) const;
    virtual int  FindByRef( void* ref ) const;
    virtual int  FindByRef( asUINT startAt, void* ref ) const;

    // GC methods
    virtual int  GetRefCount();
    virtual void SetFlag();
    virtual bool GetFlag();
    virtual void EnumReferences( asIScriptEngine* engine );
    virtual void ReleaseAllHandles( asIScriptEngine* engine );

protected:
    mutable int    refCount;
    mutable bool   gcFlag;
    asIObjectType* objType;
    ArrayBuffer*   buffer;
    int            elementSize;
    int            subTypeId;

    ScriptArray();
    ScriptArray( asIObjectType* ot, void* initBuf );   // Called from script when initialized with list
    ScriptArray( asUINT length, asIObjectType* ot );
    ScriptArray( asUINT length, void* defVal, asIObjectType* ot );
    ScriptArray( const ScriptArray& other );
    virtual ~ScriptArray();

    virtual bool  Less( const void* a, const void* b, bool asc, asIScriptContext* ctx, ArrayCache* cache );
    virtual void* GetArrayItemPointer( int index );
    virtual void* GetDataPointer( void* buffer );
    virtual void  Copy( void* dst, void* src );
    virtual void  Precache();
    virtual bool  CheckMaxSize( asUINT numElements );
    virtual void  Resize( int delta, asUINT at );
    virtual void  CreateBuffer( ArrayBuffer** buf, asUINT numElements );
    virtual void  DeleteBuffer( ArrayBuffer* buf );
    virtual void  CopyBuffer( ArrayBuffer* dst, ArrayBuffer* src );
    virtual void  Construct( ArrayBuffer* buf, asUINT start, asUINT end );
    virtual void  Destruct( ArrayBuffer* buf, asUINT start, asUINT end );
    virtual bool  Equals( const void* a, const void* b, asIScriptContext* ctx, ArrayCache* cache ) const;
};

#ifndef FONLINE_DLL
void RegisterScriptArray( asIScriptEngine* engine, bool defaultArray );
#endif

#endif
