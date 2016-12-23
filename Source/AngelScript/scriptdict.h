#ifndef SCRIPTDICT_H
#define SCRIPTDICT_H

#ifndef ANGELSCRIPT_H 
// Avoid having to inform include path if header is already include before
#include <angelscript.h>
#endif

#include <map>
#include <vector>

BEGIN_AS_NAMESPACE

class CScriptDict
{
public:
    // Set the memory functions that should be used by all CScriptArrays
    static void SetMemoryFunctions(asALLOCFUNC_t allocFunc, asFREEFUNC_t freeFunc);

    // Factory functions
    static CScriptDict* Create(asITypeInfo* ot);
    static CScriptDict* Create(asITypeInfo* ot, void* listBuffer);

    // Memory management
    virtual void AddRef() const;
    virtual void Release() const;

    // Copy the contents of one dict to another (only if the types are the same)
    virtual CScriptDict& operator=(const CScriptDict& other);

    // Compare two dicts
    virtual bool operator==(const CScriptDict&) const;

    // Dict manipulation
    virtual asUINT GetSize() const;
    virtual bool   IsEmpty() const;
    virtual void   Set(void* key, void* value);
    virtual void   SetIfNotExist(void* key, void* value);
    virtual bool   Remove(void* key);
    virtual asUINT RemoveValues(void* value);
    virtual void   Clear();
    virtual void*  Get(void* key);
    virtual void*  GetDefault(void* key, void* defaultValue);
    virtual void*  GetKey(asUINT index);
    virtual void*  GetValue(asUINT index);
    virtual bool   Exists(void* key) const;
    virtual void   GetMap(std::vector< std::pair< void*, void* > >& data);

    // GC methods
    virtual int  GetRefCount();
    virtual void SetFlag();
    virtual bool GetFlag();
    virtual void EnumReferences(asIScriptEngine* engine);
    virtual void ReleaseAllHandles(asIScriptEngine* engine);

protected:
    mutable int  refCount;
    mutable bool gcFlag;
    asITypeInfo* objType;
    int          keyTypeId;
    int          valueTypeId;
    void*        dictMap;

    CScriptDict();
    CScriptDict(asITypeInfo* ot);
    CScriptDict(asITypeInfo* ot, void* initBuf);
    CScriptDict(const CScriptDict& other);
    virtual ~CScriptDict();
};

void RegisterScriptDict(asIScriptEngine* engine);

END_AS_NAMESPACE

#endif
