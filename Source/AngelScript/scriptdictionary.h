#ifndef SCRIPTDICTIONARY_H
#define SCRIPTDICTIONARY_H

// The dictionary class relies on the script string object, thus the script
// string type must be registered with the engine before registering the
// dictionary type

#include "angelscript.h"
#include "scriptarray.h"
#include "scriptstring.h"
#include <string>
#include <map>

class ScriptArray;
class ScriptDictionary;

class ScriptDictValue
{
public:
    // This class must not be declared as local variable in C++, because it needs
    // to receive the script engine pointer in all operations. The engine pointer
    // is not kept as member in order to keep the size down
    ScriptDictValue();
    ScriptDictValue( asIScriptEngine* engine, void* value, int typeId );

    // Destructor must not be called without first calling FreeValue, otherwise a memory leak will occur
    ~ScriptDictValue();

    // Replace the stored value
    void Set( asIScriptEngine* engine, void* value, int typeId );
    void Set( asIScriptEngine* engine, const asINT64& value );
    void Set( asIScriptEngine* engine, const double& value );
    void Set( asIScriptEngine* engine, ScriptDictValue& value );

    // Gets the stored value. Returns false if the value isn't compatible with the informed typeId
    bool Get( asIScriptEngine* engine, void* value, int typeId ) const;
    bool Get( asIScriptEngine* engine, asINT64& value ) const;
    bool Get( asIScriptEngine* engine, double& value ) const;

    // Returns the type id of the stored value
    int GetTypeId() const;

    // Free the stored value
    void FreeValue( asIScriptEngine* engine );

protected:
    friend class ScriptDictionary;

    union
    {
        asINT64 m_valueInt;
        double  m_valueFlt;
        void*   m_valueObj;
    };
    int m_typeId;
};

class ScriptDictionary
{
public:
    #ifdef FONLINE_DLL
    static ScriptDictionary& Create()
    {
        static asIObjectType* ot = ASEngine->GetObjectTypeByDecl( "dictionary" );
        ScriptDictionary*     scriptDictionary = (ScriptDictionary*) ASEngine->CreateScriptObject( ot );
        return *scriptDictionary;
    }
protected:
    #endif

    ScriptDictionary( const ScriptDictionary& );
    ScriptDictionary( asIScriptEngine* engine );

    // Constructor. Called from the script to instantiate a dictionary from an initialization list
    ScriptDictionary( asBYTE* buffer );

public:
    #ifndef FONLINE_DLL
    // Factory functions
    static ScriptDictionary* Create( asIScriptEngine* engine );

    // Called from the script to instantiate a dictionary from an initialization list
    static ScriptDictionary* Create( asBYTE* buffer );
    #endif

    // Reference counting
    virtual void AddRef() const;
    virtual void Release() const;

    // Reassign the dictionary
    virtual ScriptDictionary& operator=( const ScriptDictionary& other );

    // Sets a key/value pair
    virtual void Set( const ScriptString& key, void* value, int typeId );
    virtual void Set( const ScriptString& key, const asINT64& value );
    virtual void Set( const ScriptString& key, const double& value );
    virtual void Set( const std::string& key, void* value, int typeId );
    virtual void Set( const std::string& key, const asINT64& value );
    virtual void Set( const std::string& key, const double& value );

    // Gets the stored value. Returns false if the value isn't compatible with the informed typeId
    virtual bool Get( const ScriptString& key, void* value, int typeId ) const;
    virtual bool Get( const ScriptString& key, asINT64& value ) const;
    virtual bool Get( const ScriptString& key, double& value ) const;

    // Index accessors. If the dictionary is not const it inserts the value if it doesn't already exist
    // If the dictionary is const then a script exception is set if it doesn't exist and a null pointer is returned
    virtual ScriptDictValue*       operator[]( const ScriptString& key );
    virtual const ScriptDictValue* operator[]( const ScriptString& key ) const;

    // Returns the type id of the stored value, or negative if it doesn't exist
    virtual int GetTypeId( const std::string& key ) const;

    // Returns true if the key is set
    virtual bool Exists( const ScriptString& key ) const;

    // Returns true if there are no key/value pairs in the dictionary
    virtual bool IsEmpty() const;

    // Returns the number of key/value pairs in the dictionary
    virtual asUINT GetSize() const;

    // Deletes the key
    virtual void Delete( const ScriptString& key );

    // Deletes all keys
    virtual void DeleteAll();

    // Get an array of all keys
    virtual ScriptArray* GetKeys() const;

public:
    // STL style iterator
    class Iterator
    {
public:
        void operator++();            // Pre-increment
        void operator++( int );       // Post-increment

        // This is needed to support C++11 range-for
        Iterator& operator*();

        bool operator==( const Iterator& other ) const;
        bool operator!=( const Iterator& other ) const;

        // Accessors
        const std::string& GetKey() const;
        int                GetTypeId() const;
        bool               GetValue( asINT64& value ) const;
        bool               GetValue( double& value ) const;
        bool               GetValue( void* value, int typeId ) const;

protected:
        friend class ScriptDictionary;

        Iterator();
        Iterator( const ScriptDictionary& dict, std::map< std::string, ScriptDictValue >::const_iterator it );

        Iterator& operator=( const Iterator& ) { return *this; }      // Not used

        std::map< std::string, ScriptDictValue >::const_iterator m_it;
        const ScriptDictionary&                                  m_dict;
    };

    virtual Iterator begin() const;
    virtual Iterator end() const;

    // Garbage collections behaviors
    virtual int  GetRefCount();
    virtual void SetGCFlag();
    virtual bool GetGCFlag();
    virtual void EnumReferences( asIScriptEngine* engine );
    virtual void ReleaseAllReferences( asIScriptEngine* engine );

protected:
    // We don't want anyone to call the destructor directly, it should be called through the Release method
    virtual ~ScriptDictionary();

    // Our properties
    asIScriptEngine* engine;
    mutable int      refCount;
    mutable bool     gcFlag;

    // TODO: memory: The allocator should use the asAllocMem and asFreeMem
    // TODO: optimize: Use C++11 std::unordered_map instead
    std::map< std::string, ScriptDictValue > dict;
};

#ifndef FONLINE_DLL
void RegisterScriptDictionary( asIScriptEngine* engine );
#endif

#endif
