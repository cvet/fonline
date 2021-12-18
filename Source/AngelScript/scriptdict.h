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

#ifndef SCRIPTDICT_H
#define SCRIPTDICT_H

#ifndef ANGELSCRIPT_H
// Avoid having to inform include path if header is already include before
#include "angelscript.h"
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

    // Type information
    asITypeInfo* GetDictObjectType() const;
    int GetDictTypeId() const;

    // Copy the contents of one dict to another (only if the types are the same)
    virtual CScriptDict& operator=(const CScriptDict& other);

    // Compare two dicts
    virtual bool operator==(const CScriptDict&) const;

    // Dict manipulation
    virtual asUINT GetSize() const;
    virtual bool IsEmpty() const;
    virtual void Set(void* key, void* value);
    virtual void SetIfNotExist(void* key, void* value);
    virtual bool Remove(void* key);
    virtual asUINT RemoveValues(void* value);
    virtual void Clear();
    virtual void* Get(void* key);
    virtual void* GetDefault(void* key, void* defaultValue);
    virtual void* GetKey(asUINT index);
    virtual void* GetValue(asUINT index);
    virtual bool Exists(void* key) const;
    virtual void GetMap(std::vector<std::pair<void*, void*>>& data);

    // GC methods
    virtual int GetRefCount();
    virtual void SetFlag();
    virtual bool GetFlag();
    virtual void EnumReferences(asIScriptEngine* engine);
    virtual void ReleaseAllHandles(asIScriptEngine* engine);

protected:
    mutable int refCount;
    mutable bool gcFlag;
    asITypeInfo* objType;
    int keyTypeId;
    int valueTypeId;
    void* dictMap;

    CScriptDict();
    CScriptDict(asITypeInfo* ot);
    CScriptDict(asITypeInfo* ot, void* initBuf);
    CScriptDict(const CScriptDict& other);
    virtual ~CScriptDict();
};

void RegisterScriptDict(asIScriptEngine* engine);

END_AS_NAMESPACE

#endif
