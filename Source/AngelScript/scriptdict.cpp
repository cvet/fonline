//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//


#include <stddef.h>
#include "scriptdict.h"
#include "aswrappedcall.h"
#include <assert.h>
#include <new>
#include <stdio.h> // sprintf
#include <stdlib.h>
#include <string.h>
#include <string>

using namespace std;

BEGIN_AS_NAMESPACE

// This macro is used to avoid warnings about unused variables.
// Usually where the variables are only used in debug mode.
#define UNUSED_VAR(x) (void)(x)

// Set the default memory routines
// Use the angelscript engine's memory routines by default
static asALLOCFUNC_t userAlloc = asAllocMem;
static asFREEFUNC_t userFree = asFreeMem;

// Allows the application to set which memory routines should be used by the array object
void CScriptDict::SetMemoryFunctions(asALLOCFUNC_t allocFunc, asFREEFUNC_t freeFunc)
{
    userAlloc = allocFunc;
    userFree = freeFunc;
}

static void RegisterScriptDict_Native(asIScriptEngine* engine);
static void RegisterScriptDict_Generic(asIScriptEngine* engine);

static void* CopyObject(asITypeInfo* objType, int subTypeIndex, void* value);
static void DestroyObject(asITypeInfo* objType, int subTypeIndex, void* value);
static bool Less(int typeId, const void* a, const void* b);
static bool Equals(int typeId, const void* a, const void* b);

struct DictMapComparator
{
    DictMapComparator(int id) { typeId = id; }

    bool operator()(const void* a, const void* b) const { return Less(typeId, a, b); }

    int typeId;
};

static int stringTypeId;

typedef map<void*, void*, DictMapComparator> DictMap;

CScriptDict* CScriptDict::Create(asITypeInfo* ot)
{
    // Allocate the memory
    void* mem = userAlloc(sizeof(CScriptDict));
    if (mem == 0)
    {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx)
            ctx->SetException("Out of memory");

        return 0;
    }

    // Initialize the object
    CScriptDict* d = new (mem) CScriptDict(ot);

    return d;
}

CScriptDict* CScriptDict::Create(asITypeInfo* ot, void* initList)
{
    // Allocate the memory
    void* mem = userAlloc(sizeof(CScriptDict));
    if (mem == 0)
    {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx)
            ctx->SetException("Out of memory");

        return 0;
    }

    // Initialize the object
    CScriptDict* d = new (mem) CScriptDict(ot, initList);

    return d;
}

// This optional callback is called when the template type is first used by the compiler.
// It allows the application to validate if the template can be instanciated for the requested
// subtype at compile time, instead of at runtime. The output argument dontGarbageCollect
// allow the callback to tell the engine if the template instance type shouldn't be garbage collected,
// i.e. no asOBJ_GC flag.
static bool CScriptDict_TemplateCallbackExt(asITypeInfo* ot, int subTypeIndex, bool& dontGarbageCollect)
{
    // Make sure the subtype can be instanciated with a default factory/constructor,
    // otherwise we won't be able to instanciate the elements.
    int typeId = ot->GetSubTypeId(subTypeIndex);
    if (typeId == asTYPEID_VOID)
        return false;
    if ((typeId & asTYPEID_MASK_OBJECT) && !(typeId & asTYPEID_OBJHANDLE))
    {
        asITypeInfo* subtype = ot->GetEngine()->GetTypeInfoById(typeId);
        asDWORD flags = subtype->GetFlags();
        if ((flags & asOBJ_VALUE) && !(flags & asOBJ_POD))
        {
            // Verify that there is a default constructor
            bool found = false;
            for (asUINT n = 0; n < subtype->GetBehaviourCount(); n++)
            {
                asEBehaviours beh;
                asIScriptFunction* func = subtype->GetBehaviourByIndex(n, &beh);
                if (beh != asBEHAVE_CONSTRUCT)
                    continue;

                if (func->GetParamCount() == 0)
                {
                    // Found the default constructor
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                // There is no default constructor
                ot->GetEngine()->WriteMessage("dict", 0, 0, asMSGTYPE_ERROR, "The subtype has no default constructor");
                return false;
            }
        }
        else if ((flags & asOBJ_REF))
        {
            bool found = false;

            // If value assignment for ref type has been disabled then the dict
            // can be created if the type has a default factory function
            if (!ot->GetEngine()->GetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE))
            {
                // Verify that there is a default factory
                for (asUINT n = 0; n < subtype->GetFactoryCount(); n++)
                {
                    asIScriptFunction* func = subtype->GetFactoryByIndex(n);
                    if (func->GetParamCount() == 0)
                    {
                        // Found the default factory
                        found = true;
                        break;
                    }
                }
            }

            if (!found)
            {
                // No default factory
                ot->GetEngine()->WriteMessage("dict", 0, 0, asMSGTYPE_ERROR, "The subtype has no default factory");
                return false;
            }
        }

        // If the object type is not garbage collected then the dict also doesn't need to be
        if (!(flags & asOBJ_GC))
            dontGarbageCollect = true;
    }
    else if (!(typeId & asTYPEID_OBJHANDLE))
    {
        // Dicts with primitives cannot form circular references,
        // thus there is no need to garbage collect them
        dontGarbageCollect = true;
    }
    else
    {
        assert(typeId & asTYPEID_OBJHANDLE);

        // It is not necessary to set the dict as garbage collected for all handle types.
        // If it is possible to determine that the handle cannot refer to an object type
        // that can potentially form a circular reference with the dict then it is not
        // necessary to make the dict garbage collected.
        asITypeInfo* subtype = ot->GetEngine()->GetTypeInfoById(typeId);
        asDWORD flags = subtype->GetFlags();
        if (!(flags & asOBJ_GC))
        {
            if ((flags & asOBJ_SCRIPT_OBJECT))
            {
                // Even if a script class is by itself not garbage collected, it is possible
                // that classes that derive from it may be, so it is not possible to know
                // that no circular reference can occur.
                if ((flags & asOBJ_NOINHERIT))
                {
                    // A script class declared as final cannot be inherited from, thus
                    // we can be certain that the object cannot be garbage collected.
                    dontGarbageCollect = true;
                }
            }
            else
            {
                // For application registered classes we assume the application knows
                // what it is doing and don't mark the dict as garbage collected unless
                // the type is also garbage collected.
                dontGarbageCollect = true;
            }
        }
    }

    // The type is ok
    return true;
}

static bool CScriptDict_TemplateCallback(asITypeInfo* ot, bool& dontGarbageCollect)
{
    return CScriptDict_TemplateCallbackExt(ot, 0, dontGarbageCollect) &&
        CScriptDict_TemplateCallbackExt(ot, 1, dontGarbageCollect);
}

// Registers the template dict type
void RegisterScriptDict(asIScriptEngine* engine)
{
    if (strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") == 0)
        RegisterScriptDict_Native(engine);
    else
        RegisterScriptDict_Generic(engine);

    stringTypeId = engine->GetTypeIdByDecl("string");
}

static void RegisterScriptDict_Native(asIScriptEngine* engine)
{
    int r = 0;
    UNUSED_VAR(r);

    // Register the dict type as a template
    r = engine->RegisterObjectType("dict<class T1, class T2>", 0, asOBJ_REF | asOBJ_GC | asOBJ_TEMPLATE);
    assert(r >= 0);

    // Register a callback for validating the subtype before it is used
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)",
        asFUNCTION(CScriptDict_TemplateCallback), asCALL_CDECL);
    assert(r >= 0);

    // Templates receive the object type as the first parameter. To the script writer this is hidden
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", asBEHAVE_FACTORY, "dict<T1,T2>@ f(int&in)",
        asFUNCTIONPR(CScriptDict::Create, (asITypeInfo*), CScriptDict*), asCALL_CDECL);
    assert(r >= 0);

    // Register the factory that will be used for initialization lists
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", asBEHAVE_LIST_FACTORY,
        "dict<T1,T2>@ f(int&in type, int&in list) {repeat {T1, T2}}",
        asFUNCTIONPR(CScriptDict::Create, (asITypeInfo*, void*), CScriptDict*), asCALL_CDECL);
    assert(r >= 0);

    // The memory management methods
    r = engine->RegisterObjectBehaviour(
        "dict<T1,T2>", asBEHAVE_ADDREF, "void f()", asMETHOD(CScriptDict, AddRef), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour(
        "dict<T1,T2>", asBEHAVE_RELEASE, "void f()", asMETHOD(CScriptDict, Release), asCALL_THISCALL);
    assert(r >= 0);

    // The index operator returns the template subtype
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "const T2& get_opIndex(const T1&in) const", asMETHOD(CScriptDict, Get), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "void set_opIndex(const T1&in, const T2&in)", asMETHOD(CScriptDict, Set), asCALL_THISCALL);
    assert(r >= 0);

    // The assignment operator
    // Patch r = engine->RegisterObjectMethod("dict<T1,T2>", "dict<T1,T2>& opAssign(const dict<T1,T2>&in)",
    // asMETHOD(CScriptDict, operator=), asCALL_THISCALL); assert(r >= 0);

    // Other methods
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "void set(const T1&in, const T2&in)", asMETHOD(CScriptDict, Set), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "void setIfNotExist(const T1&in, const T2&in)",
        asMETHOD(CScriptDict, SetIfNotExist), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "bool remove(const T1&in)", asMETHOD(CScriptDict, Remove), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "uint removeValues(const T2&in)", asMETHOD(CScriptDict, RemoveValues), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "uint length() const", asMETHOD(CScriptDict, GetSize), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "void clear()", asMETHOD(CScriptDict, Clear), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "const T2& get(const T1&in) const", asMETHOD(CScriptDict, Get), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "const T2& get(const T1&in, const T2&in) const",
        asMETHOD(CScriptDict, GetDefault), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "const T1& getKey(uint index) const", asMETHOD(CScriptDict, GetKey), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "const T2& getValue(uint index) const", asMETHOD(CScriptDict, GetValue), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "bool exists(const T1&in) const", asMETHOD(CScriptDict, Exists), asCALL_THISCALL);
    assert(r >= 0);
    // Patch r = engine->RegisterObjectMethod("dict<T1,T2>", "bool opEquals(const dict<T1,T2>&in) const",
    // asMETHOD(CScriptDict, operator==), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "bool isEmpty() const", asMETHOD(CScriptDict, IsEmpty), asCALL_THISCALL);
    assert(r >= 0);

    // Register GC behaviors in case the dict needs to be garbage collected
    r = engine->RegisterObjectBehaviour(
        "dict<T1,T2>", asBEHAVE_GETREFCOUNT, "int f()", asMETHOD(CScriptDict, GetRefCount), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour(
        "dict<T1,T2>", asBEHAVE_SETGCFLAG, "void f()", asMETHOD(CScriptDict, SetFlag), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour(
        "dict<T1,T2>", asBEHAVE_GETGCFLAG, "bool f()", asMETHOD(CScriptDict, GetFlag), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour(
        "dict<T1,T2>", asBEHAVE_ENUMREFS, "void f(int&in)", asMETHOD(CScriptDict, EnumReferences), asCALL_THISCALL);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", asBEHAVE_RELEASEREFS, "void f(int&in)",
        asMETHOD(CScriptDict, ReleaseAllHandles), asCALL_THISCALL);
    assert(r >= 0);
}

CScriptDict::CScriptDict(asITypeInfo* ot)
{
    refCount = 1;
    gcFlag = false;
    objType = ot;
    objType->AddRef();
    keyTypeId = objType->GetSubTypeId(0);
    valueTypeId = objType->GetSubTypeId(1);
    dictMap = new DictMap(DictMapComparator(keyTypeId));

    // Notify the GC of the successful creation
    if (objType->GetFlags() & asOBJ_GC)
        objType->GetEngine()->NotifyGarbageCollectorOfNewObject(this, objType);
}

CScriptDict::CScriptDict(asITypeInfo* ot, void* listBuffer)
{
    refCount = 1;
    gcFlag = false;
    objType = ot;
    objType->AddRef();
    keyTypeId = objType->GetSubTypeId(0);
    valueTypeId = objType->GetSubTypeId(1);
    dictMap = new DictMap(DictMapComparator(keyTypeId));

    asIScriptEngine* engine = ot->GetEngine();
    asBYTE* buffer = (asBYTE*)listBuffer;
    asUINT length = *(asUINT*)buffer;
    buffer += sizeof(asUINT);

    while (length--)
    {
        if (asPWORD(buffer) & 0x3)
            buffer += 4 - (asPWORD(buffer) & 0x3);

        void* key = buffer;
        if (keyTypeId & asTYPEID_MASK_OBJECT)
        {
            asITypeInfo* ot = engine->GetTypeInfoById(keyTypeId);
            if (ot->GetFlags() & asOBJ_VALUE)
                buffer += ot->GetSize();
            else
                buffer += sizeof(void*);
            if (ot->GetFlags() & asOBJ_REF && !(keyTypeId & asTYPEID_OBJHANDLE))
                key = *(void**)key;
        }
        else if (keyTypeId == asTYPEID_VOID)
        {
            buffer += sizeof(void*);
        }
        else
        {
            buffer += engine->GetSizeOfPrimitiveType(keyTypeId);
        }

        void* value = buffer;
        if (valueTypeId & asTYPEID_MASK_OBJECT)
        {
            asITypeInfo* ot = engine->GetTypeInfoById(valueTypeId);
            if (ot->GetFlags() & asOBJ_VALUE)
                buffer += ot->GetSize();
            else
                buffer += sizeof(void*);
            if (ot->GetFlags() & asOBJ_REF && !(valueTypeId & asTYPEID_OBJHANDLE))
                value = *(void**)value;
        }
        else if (valueTypeId == asTYPEID_VOID)
        {
            buffer += sizeof(void*);
        }
        else
        {
            buffer += engine->GetSizeOfPrimitiveType(valueTypeId);
        }

        Set(key, value);
    }

    // Notify the GC of the successful creation
    if (objType->GetFlags() & asOBJ_GC)
        objType->GetEngine()->NotifyGarbageCollectorOfNewObject(this, objType);
}

CScriptDict::CScriptDict(const CScriptDict& other)
{
    refCount = 1;
    gcFlag = false;
    objType = other.objType;
    objType->AddRef();
    keyTypeId = objType->GetSubTypeId(0);
    valueTypeId = objType->GetSubTypeId(1);
    dictMap = new DictMap(DictMapComparator(keyTypeId));

    DictMap* dict = (DictMap*)other.dictMap;
    for (auto it = dict->begin(); it != dict->end(); ++it)
        Set(it->first, it->second);

    if (objType->GetFlags() & asOBJ_GC)
        objType->GetEngine()->NotifyGarbageCollectorOfNewObject(this, objType);
}

CScriptDict& CScriptDict::operator=(const CScriptDict& other)
{
    // Only perform the copy if the array types are the same
    if (&other != this && other.objType == objType)
    {
        Clear();

        DictMap* dict = (DictMap*)other.dictMap;
        for (auto it = dict->begin(); it != dict->end(); ++it)
            Set(it->first, it->second);
    }

    return *this;
}

CScriptDict::~CScriptDict()
{
    Clear();
    delete (DictMap*)dictMap;

    if (objType)
        objType->Release();
}

asUINT CScriptDict::GetSize() const
{
    DictMap* dict = (DictMap*)dictMap;

    return (asUINT)dict->size();
}

bool CScriptDict::IsEmpty() const
{
    DictMap* dict = (DictMap*)dictMap;

    return dict->empty();
}

void CScriptDict::Set(void* key, void* value)
{
    DictMap* dict = (DictMap*)dictMap;

    auto it = dict->find(key);
    if (it == dict->end())
    {
        key = CopyObject(objType, 0, key);
        value = CopyObject(objType, 1, value);
        dict->insert(DictMap::value_type(key, value));
    }
    else
    {
        DestroyObject(objType, 1, it->second);
        value = CopyObject(objType, 1, value);
        it->second = value;
    }
}

void CScriptDict::SetIfNotExist(void* key, void* value)
{
    DictMap* dict = (DictMap*)dictMap;

    auto it = dict->find(key);
    if (it == dict->end())
    {
        key = CopyObject(objType, 0, key);
        value = CopyObject(objType, 1, value);
        dict->insert(DictMap::value_type(key, value));
    }
}

bool CScriptDict::Remove(void* key)
{
    DictMap* dict = (DictMap*)dictMap;

    auto it = dict->find(key);
    if (it != dict->end())
    {
        DestroyObject(objType, 0, it->first);
        DestroyObject(objType, 1, it->second);
        dict->erase(it);
        return true;
    }

    return false;
}

asUINT CScriptDict::RemoveValues(void* value)
{
    DictMap* dict = (DictMap*)dictMap;
    asUINT result = 0;

    for (auto it = dict->begin(); it != dict->end();)
    {
        if (Equals(valueTypeId, it->second, value))
        {
            DestroyObject(objType, 0, it->first);
            DestroyObject(objType, 1, it->second);
            it = dict->erase(it);
            result++;
        }
        else
        {
            ++it;
        }
    }

    return result;
}

void CScriptDict::Clear()
{
    DictMap* dict = (DictMap*)dictMap;

    for (auto it = dict->begin(), end = dict->end(); it != end; ++it)
    {
        DestroyObject(objType, 0, it->first);
        DestroyObject(objType, 1, it->second);
    }
    dict->clear();
}

void* CScriptDict::Get(void* key)
{
    DictMap* dict = (DictMap*)dictMap;

    auto it = dict->find(key);
    if (it == dict->end())
    {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx)
            ctx->SetException("Key not found");
        return NULL;
    }

    return it->second;
}

void* CScriptDict::GetDefault(void* key, void* defaultValue)
{
    DictMap* dict = (DictMap*)dictMap;

    auto it = dict->find(key);
    if (it == dict->end())
        return defaultValue;

    return it->second;
}

void* CScriptDict::GetKey(asUINT index)
{
    DictMap* dict = (DictMap*)dictMap;

    if (index >= (asUINT)dict->size())
    {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx)
            ctx->SetException("Index out of bounds");
        return NULL;
    }

    auto it = dict->begin();
    while (index--)
        it++;

    return it->first;
}

void* CScriptDict::GetValue(asUINT index)
{
    DictMap* dict = (DictMap*)dictMap;

    if (index >= (asUINT)dict->size())
    {
        asIScriptContext* ctx = asGetActiveContext();
        if (ctx)
            ctx->SetException("Index out of bounds");
        return NULL;
    }

    auto it = dict->begin();
    while (index--)
        it++;

    return it->second;
}

bool CScriptDict::Exists(void* key) const
{
    DictMap* dict = (DictMap*)dictMap;

    return dict->count(key) > 0;
}

bool CScriptDict::operator==(const CScriptDict& other) const
{
    if (objType != other.objType)
        return false;

    if (GetSize() != other.GetSize())
        return false;

    DictMap* dict1 = (DictMap*)dictMap;
    DictMap* dict2 = (DictMap*)other.dictMap;

    auto it1 = dict1->begin();
    auto it2 = dict2->begin();
    auto end1 = dict1->end();
    auto end2 = dict2->end();

    while (it1 != end1 && it2 != end2)
    {
        if (!Equals(keyTypeId, (*it1).first, (*it2).first) || !Equals(valueTypeId, (*it1).second, (*it2).second))
            return false;
        it1++;
        it2++;
    }

    return true;
}

asITypeInfo* CScriptDict::GetDictObjectType() const
{
    return objType;
}

int CScriptDict::GetDictTypeId() const
{
    return objType->GetTypeId();
}

// GC behaviour
void CScriptDict::EnumReferences(asIScriptEngine* engine)
{
    // TODO: If garbage collection can be done from a separate thread, then this method must be
    //       protected so that it doesn't get lost during the iteration if the array is modified

    // If the array is holding handles, then we need to notify the GC of them
    DictMap* dict = (DictMap*)dictMap;

    bool keysHandle = (keyTypeId & asTYPEID_MASK_OBJECT) != 0;
    bool valuesHandle = (valueTypeId & asTYPEID_MASK_OBJECT) != 0;

    if (keysHandle || valuesHandle)
    {
        for (auto it = dict->begin(), end = dict->end(); it != end; ++it)
        {
            if (keysHandle)
                engine->GCEnumCallback(it->first);
            if (valuesHandle)
                engine->GCEnumCallback(it->second);
        }
    }
}

// GC behaviour
void CScriptDict::ReleaseAllHandles(asIScriptEngine*)
{
    // Resizing to zero will release everything
    Clear();
}

void CScriptDict::AddRef() const
{
    // Clear the GC flag then increase the counter
    gcFlag = false;
    asAtomicInc(refCount);
}

void CScriptDict::Release() const
{
    // Clearing the GC flag then descrease the counter
    gcFlag = false;
    if (asAtomicDec(refCount) == 0)
    {
        // When reaching 0 no more references to this instance
        // exists and the object should be destroyed
        this->~CScriptDict();
        userFree(const_cast<CScriptDict*>(this));
    }
}

// GC behaviour
int CScriptDict::GetRefCount()
{
    return refCount;
}

// GC behaviour
void CScriptDict::SetFlag()
{
    gcFlag = true;
}

// GC behaviour
bool CScriptDict::GetFlag()
{
    return gcFlag;
}

void CScriptDict::GetMap(std::vector<std::pair<void*, void*>>& data)
{
    DictMap* dict = (DictMap*)dictMap;
    data.reserve(data.size() + dict->size());
    for (const auto& kv : *dict)
        data.push_back(std::pair<void*, void*>(kv.first, kv.second));
}

// internal
static void* CopyObject(asITypeInfo* objType, int subTypeIndex, void* value)
{
    int subTypeId = objType->GetSubTypeId(subTypeIndex);
    asITypeInfo* subType = objType->GetSubType(subTypeIndex);
    asIScriptEngine* engine = objType->GetEngine();

    if (subTypeId & asTYPEID_MASK_OBJECT && !(subTypeId & asTYPEID_OBJHANDLE))
        return engine->CreateScriptObjectCopy(value, subType);

    int elementSize;
    if (subTypeId & asTYPEID_MASK_OBJECT)
        elementSize = sizeof(asPWORD);
    else
        elementSize = engine->GetSizeOfPrimitiveType(subTypeId);

    void* ptr = userAlloc(elementSize);
    memset(ptr, 0, elementSize);

    if (subTypeId & asTYPEID_OBJHANDLE)
    {
        *(void**)ptr = *(void**)value;
        if (*(void**)value)
            subType->GetEngine()->AddRefScriptObject(*(void**)value, subType);
    }
    else if (subTypeId == asTYPEID_BOOL || subTypeId == asTYPEID_INT8 || subTypeId == asTYPEID_UINT8)
        *(char*)ptr = *(char*)value;
    else if (subTypeId == asTYPEID_INT16 || subTypeId == asTYPEID_UINT16)
        *(short*)ptr = *(short*)value;
    else if (subTypeId == asTYPEID_INT32 || subTypeId == asTYPEID_UINT32 || subTypeId == asTYPEID_FLOAT ||
        subTypeId > asTYPEID_DOUBLE) // enums have a type id larger than doubles
        *(int*)ptr = *(int*)value;
    else if (subTypeId == asTYPEID_INT64 || subTypeId == asTYPEID_UINT64 || subTypeId == asTYPEID_DOUBLE)
        *(double*)ptr = *(double*)value;

    return ptr;
}

static void DestroyObject(asITypeInfo* objType, int subTypeIndex, void* value)
{
    int subTypeId = objType->GetSubTypeId(subTypeIndex);
    asIScriptEngine* engine = objType->GetEngine();

    if (subTypeId & asTYPEID_MASK_OBJECT && !(subTypeId & asTYPEID_OBJHANDLE))
    {
        engine->ReleaseScriptObject(value, engine->GetTypeInfoById(subTypeId));
    }
    else
    {
        if (subTypeId & asTYPEID_OBJHANDLE && *(void**)value)
            engine->ReleaseScriptObject(*(void**)value, engine->GetTypeInfoById(subTypeId));

        userFree(value);
    }
}

static bool Less(int typeId, const void* a, const void* b)
{
    if (typeId == stringTypeId)
    {
        const std::string& aStr = *(const std::string*)a;
        const std::string& bStr = *(const std::string*)b;
        return aStr < bStr;
    }

    if (!(typeId & asTYPEID_MASK_OBJECT))
    {
        // Simple compare of values
        switch (typeId)
        {
#define COMPARE(T) *((T*)a) < *((T*)b)
        case asTYPEID_BOOL:
            return COMPARE(bool);
        case asTYPEID_INT8:
            return COMPARE(signed char);
        case asTYPEID_UINT8:
            return COMPARE(unsigned char);
        case asTYPEID_INT16:
            return COMPARE(signed short);
        case asTYPEID_UINT16:
            return COMPARE(unsigned short);
        case asTYPEID_INT32:
            return COMPARE(signed int);
        case asTYPEID_UINT32:
            return COMPARE(unsigned int);
        case asTYPEID_FLOAT:
            return COMPARE(float);
        case asTYPEID_DOUBLE:
            return COMPARE(double);
        default:
            return COMPARE(signed int); // All enums fall in this case
#undef COMPARE
        }
    }
    return a < b;
}

static bool Equals(int typeId, const void* a, const void* b)
{
    if (typeId == stringTypeId)
    {
        const std::string& aStr = *(const std::string*)a;
        const std::string& bStr = *(const std::string*)b;
        return aStr == bStr;
    }

    if (!(typeId & asTYPEID_MASK_OBJECT))
    {
        // Simple compare of values
        switch (typeId)
        {
#define COMPARE(T) *((T*)a) == *((T*)b)
        case asTYPEID_BOOL:
            return COMPARE(bool);
        case asTYPEID_INT8:
            return COMPARE(signed char);
        case asTYPEID_UINT8:
            return COMPARE(unsigned char);
        case asTYPEID_INT16:
            return COMPARE(signed short);
        case asTYPEID_UINT16:
            return COMPARE(unsigned short);
        case asTYPEID_INT32:
            return COMPARE(signed int);
        case asTYPEID_UINT32:
            return COMPARE(unsigned int);
        case asTYPEID_FLOAT:
            return COMPARE(float);
        case asTYPEID_DOUBLE:
            return COMPARE(double);
        default:
            return COMPARE(signed int); // All enums fall here
#undef COMPARE
        }
    }
    return a == b;
}

static CScriptDict* CScriptDict_Create(asITypeInfo* ti)
{
    return CScriptDict::Create(ti);
}

static CScriptDict* CScriptDict_CreateList(asITypeInfo* ti, void* list)
{
    return CScriptDict::Create(ti, list);
}

static void RegisterScriptDict_Generic(asIScriptEngine* engine)
{
    int r = 0;
    UNUSED_VAR(r);
    r = engine->RegisterObjectType("dict<class T1, class T2>", 0, asOBJ_REF | asOBJ_GC | asOBJ_TEMPLATE);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in, bool&out)",
        WRAP_FN(CScriptDict_TemplateCallback), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", asBEHAVE_FACTORY, "dict<T1,T2>@ f(int&in)",
        WRAP_FN_PR(CScriptDict::Create, (asITypeInfo*), CScriptDict*), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", asBEHAVE_LIST_FACTORY,
        "dict<T1,T2>@ f(int&in type, int&in list) {repeat {T1, T2}}",
        WRAP_FN_PR(CScriptDict::Create, (asITypeInfo*, void*), CScriptDict*), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour(
        "dict<T1,T2>", asBEHAVE_ADDREF, "void f()", WRAP_MFN(CScriptDict, AddRef), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour(
        "dict<T1,T2>", asBEHAVE_RELEASE, "void f()", WRAP_MFN(CScriptDict, Release), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "const T2& get_opIndex(const T1&in) const", WRAP_MFN(CScriptDict, Get), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "void set_opIndex(const T1&in, const T2&in)", WRAP_MFN(CScriptDict, Set), asCALL_GENERIC);
    assert(r >= 0);
    // Patch r = engine->RegisterObjectMethod("dict<T1,T2>", "dict<T1,T2>& opAssign(const dict<T1,T2>&in)",
    // WRAP_MFN(CScriptDict, operator=), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "void set(const T1&in, const T2&in)", WRAP_MFN(CScriptDict, Set), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "void setIfNotExist(const T1&in, const T2&in)",
        WRAP_MFN(CScriptDict, SetIfNotExist), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "bool remove(const T1&in)", WRAP_MFN(CScriptDict, Remove), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "uint removeValues(const T2&in)", WRAP_MFN(CScriptDict, RemoveValues), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "uint length() const", WRAP_MFN(CScriptDict, GetSize), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "void clear()", WRAP_MFN(CScriptDict, Clear), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "const T2& get(const T1&in) const", WRAP_MFN(CScriptDict, Get), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectMethod("dict<T1,T2>", "const T2& get(const T1&in, const T2&in) const",
        WRAP_MFN(CScriptDict, GetDefault), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "const T1& getKey(uint index) const", WRAP_MFN(CScriptDict, GetKey), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "const T2& getValue(uint index) const", WRAP_MFN(CScriptDict, GetValue), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "bool exists(const T1&in) const", WRAP_MFN(CScriptDict, Exists), asCALL_GENERIC);
    assert(r >= 0);
    // Patch r = engine->RegisterObjectMethod("dict<T1,T2>", "bool opEquals(const dict<T1,T2>&in) const",
    // WRAP_MFN(CScriptDict, operator==), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod(
        "dict<T1,T2>", "bool isEmpty() const", WRAP_MFN(CScriptDict, IsEmpty), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour(
        "dict<T1,T2>", asBEHAVE_GETREFCOUNT, "int f()", WRAP_MFN(CScriptDict, GetRefCount), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour(
        "dict<T1,T2>", asBEHAVE_SETGCFLAG, "void f()", WRAP_MFN(CScriptDict, SetFlag), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour(
        "dict<T1,T2>", asBEHAVE_GETGCFLAG, "bool f()", WRAP_MFN(CScriptDict, GetFlag), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour(
        "dict<T1,T2>", asBEHAVE_ENUMREFS, "void f(int&in)", WRAP_MFN(CScriptDict, EnumReferences), asCALL_GENERIC);
    assert(r >= 0);
    r = engine->RegisterObjectBehaviour("dict<T1,T2>", asBEHAVE_RELEASEREFS, "void f(int&in)",
        WRAP_MFN(CScriptDict, ReleaseAllHandles), asCALL_GENERIC);
    assert(r >= 0);
}

END_AS_NAMESPACE
