/*
   AngelCode Scripting Library
   Copyright (c) 2003-2015 Andreas Jonsson

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any
   purpose, including commercial applications, and to alter it and
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you
      must not claim that you wrote the original software. If you use
      this software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and
      must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
      distribution.

   The original version of this library can be located at:
   http://www.angelcode.com/angelscript/

   Andreas Jonsson
   andreas@angelcode.com
*/


//
// as_typeinfo.cpp
//


#include "as_config.h"

#ifndef AS_NO_COMPILER

#include "as_typeinfo.h"
#include "as_scriptengine.h"

BEGIN_AS_NAMESPACE

asCTypeInfo::asCTypeInfo()
{
	externalRefCount.set(0);
	internalRefCount.set(1); // start with one internal ref-count
	engine = 0;
	module = 0;
	size = 0;
	typeId = -1; // start as -1 to signal that it hasn't been defined

	scriptSectionIdx = -1;
	declaredAt = 0;

	accessMask = 0xFFFFFFFF;
	nameSpace = 0;
}

asCTypeInfo::asCTypeInfo(asCScriptEngine *in_engine)
{
	externalRefCount.set(0);
	internalRefCount.set(1); // start with one internal ref count
	engine = in_engine;
	module = 0;
	typeId = -1; // start as -1 to signal that it hasn't been defined

	scriptSectionIdx = -1;
	declaredAt = 0;

	accessMask = 0xFFFFFFFF;
	nameSpace = engine->nameSpaces[0];
}

asCTypeInfo::~asCTypeInfo()
{
}

// interface
int asCTypeInfo::AddRef() const
{
	return externalRefCount.atomicInc();
}

// interface
int asCTypeInfo::Release() const
{
	int r = externalRefCount.atomicDec();

	if (r == 0)
	{
		// There are no more external references, if there are also no
		// internal references then it is time to delete the object type
		if (internalRefCount.get() == 0)
		{
			// If the engine is no longer set, then it has already been 
			// released and we must take care of the deletion ourselves
			asDELETE(const_cast<asCTypeInfo*>(this), asCTypeInfo);
		}
	}

	return r;
}

int asCTypeInfo::AddRefInternal()
{
	return internalRefCount.atomicInc();
}

int asCTypeInfo::ReleaseInternal()
{
	int r = internalRefCount.atomicDec();

	if (r == 0)
	{
		// There are no more internal references, if there are also no
		// external references then it is time to delete the object type
		if (externalRefCount.get() == 0)
		{
			// If the engine is no longer set, then it has already been 
			// released and we must take care of the deletion ourselves
			asDELETE(const_cast<asCTypeInfo*>(this), asCTypeInfo);
		}
	}

	return r;
}

// interface
asIScriptModule *asCTypeInfo::GetModule() const
{
	return module;
}

void *asCTypeInfo::SetUserData(void *data, asPWORD type)
{
	// As a thread might add a new new user data at the same time as another
	// it is necessary to protect both read and write access to the userData member
	ACQUIREEXCLUSIVE(engine->engineRWLock);

	// It is not intended to store a lot of different types of userdata,
	// so a more complex structure like a associative map would just have
	// more overhead than a simple array.
	for (asUINT n = 0; n < userData.GetLength(); n += 2)
	{
		if (userData[n] == type)
		{
			void *oldData = reinterpret_cast<void*>(userData[n + 1]);
			userData[n + 1] = reinterpret_cast<asPWORD>(data);

			RELEASEEXCLUSIVE(engine->engineRWLock);

			return oldData;
		}
	}

	userData.PushLast(type);
	userData.PushLast(reinterpret_cast<asPWORD>(data));

	RELEASEEXCLUSIVE(engine->engineRWLock);

	return 0;
}

void *asCTypeInfo::GetUserData(asPWORD type) const
{
	// There may be multiple threads reading, but when  
	// setting the user data nobody must be reading.
	ACQUIRESHARED(engine->engineRWLock);

	for (asUINT n = 0; n < userData.GetLength(); n += 2)
	{
		if (userData[n] == type)
		{
			RELEASESHARED(engine->engineRWLock);
			return reinterpret_cast<void*>(userData[n + 1]);
		}
	}

	RELEASESHARED(engine->engineRWLock);

	return 0;
}

// interface
const char *asCTypeInfo::GetName() const
{
	return name.AddressOf();
}

// interface
const char *asCTypeInfo::GetNamespace() const
{
	return nameSpace->name.AddressOf();
}

// interface
asDWORD asCTypeInfo::GetFlags() const
{
	return flags;
}

// interface
asUINT asCTypeInfo::GetSize() const
{
	return size;
}

// interface
int asCTypeInfo::GetTypeId() const
{
	if (typeId == -1)
	{
		// We need a non const pointer to create the asCDataType object.
		// We're not breaking anything here because this function is not
		// modifying the object, so this const cast is safe.
		asCTypeInfo *ot = const_cast<asCTypeInfo*>(this);

		// The engine will define the typeId for this object type
		engine->GetTypeIdFromDataType(asCDataType::CreateType(ot, false));
	}

	return typeId;
}

// interface
asIScriptEngine *asCTypeInfo::GetEngine() const
{
	return engine;
}

// interface
const char *asCTypeInfo::GetConfigGroup() const
{
	asCConfigGroup *group = engine->FindConfigGroupForTypeInfo(this);
	if (group == 0)
		return 0;

	return group->groupName.AddressOf();
}

// interface
asDWORD asCTypeInfo::GetAccessMask() const
{
	return accessMask;
}

// interface
int asCTypeInfo::GetProperty(asUINT index, const char **out_name, int *out_typeId, bool *out_isPrivate, bool *out_isProtected, int *out_offset, bool *out_isReference, asDWORD *out_accessMask) const
{
	UNUSED_VAR(index);
	if (out_name) *out_name = 0;
	if (out_typeId) *out_typeId = 0;
	if (out_isPrivate) *out_isPrivate = false;
	if (out_isProtected) *out_isProtected = false;
	if (out_offset) *out_offset = 0;
	if (out_isReference) *out_isReference = false;
	if (out_accessMask) *out_accessMask = 0;
	return -1;
}

// internal
asCObjectType *asCTypeInfo::CastToObjectType()
{
	if (this == 0) return 0;

	// TODO: type: Should List pattern have its own type class?
	// TODO: type: typedefs should have their own class
	if (flags & (asOBJ_VALUE | asOBJ_REF | asOBJ_TEMPLATE_SUBTYPE | asOBJ_LIST_PATTERN | asOBJ_TYPEDEF))
		return reinterpret_cast<asCObjectType*>(this);

	return 0;
}

// internal
asCEnumType *asCTypeInfo::CastToEnumType()
{
	if (this == 0) return 0;

	if (flags & (asOBJ_ENUM))
		return reinterpret_cast<asCEnumType*>(this);

	return 0;
}

// internal
void asCTypeInfo::CleanUserData()
{
	asASSERT(engine);
	for (asUINT n = 0; n < userData.GetLength(); n += 2)
	{
		if (userData[n + 1])
		{
			for (asUINT c = 0; c < engine->cleanObjectTypeFuncs.GetLength(); c++)
				if (engine->cleanObjectTypeFuncs[c].type == userData[n])
					engine->cleanObjectTypeFuncs[c].cleanFunc(this);
		}
	}
	userData.SetLength(0);
}

// internal
bool asCTypeInfo::IsShared() const
{
	// Types that can be declared by scripts need to have the explicit flag asOBJ_SHARED
	if (flags & (asOBJ_SCRIPT_OBJECT | asOBJ_ENUM)) return flags & asOBJ_SHARED ? true : false;

	// Otherwise we assume the type to be shared
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////

asCEnumType::~asCEnumType()
{
	asUINT n;
	for (n = 0; n < enumValues.GetLength(); n++)
	{
		if (enumValues[n])
			asDELETE(enumValues[n], asSEnumValue);
	}
	enumValues.SetLength(0);
}

END_AS_NAMESPACE

#endif // AS_NO_COMPILER
