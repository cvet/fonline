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
// as_typeinfo.h
//



#ifndef AS_TYPEINFO_H
#define AS_TYPEINFO_H

#include "as_config.h"
#include "as_string.h"
#include "as_atomic.h"

#ifndef AS_NO_COMPILER

BEGIN_AS_NAMESPACE

class asCScriptEngine;
class asCModule;
class asCObjectType;
class asCEnumType;
struct asSNameSpace;

// TODO: type: This is where the new asCTypeInfo will be implemented
//             asCTypeInfo should implement the asITypeInfo interface with dummy classes
//               only the name, flags, and some other basic members shall be in this class
//             asCObjectType will inherit from asCTypeInfo instead of directly implementing asITypeInfo
//             asCEnumType shall be implemented to represent enums
//               the enum value list shall be in this class
//             asCTypeDefType shall be implemented to represent typedefs
//               the aliased data type shall be in this class
//             asCFundDefType shall be implemented to represent funcdefs
//               a pointer to the asCScriptFunction describing the func def shall be in this class
//             asCPrimitiveType shall be implemented to represent primitives (void, int, double, etc)
//             All classes except asCObjectType will be in this file

// TODO: type: asIObjectType shall be renamed to asITypeInfo
class asCTypeInfo : public asIObjectType
{
public:
	//=====================================
	// From asITypeInfo
	//=====================================
	asIScriptEngine *GetEngine() const;
	const char      *GetConfigGroup() const;
	asDWORD          GetAccessMask() const;
	asIScriptModule *GetModule() const;

	// Memory management
	int AddRef() const;
	int Release() const;

	// Type info
	const char      *GetName() const;
	const char      *GetNamespace() const;
	asIObjectType   *GetBaseType() const { return 0; }
	bool             DerivesFrom(const asIObjectType *objType) const { UNUSED_VAR(objType); return 0; }
	asDWORD          GetFlags() const;
	asUINT           GetSize() const;
	int              GetTypeId() const;
	int              GetSubTypeId(asUINT subtypeIndex = 0) const { UNUSED_VAR(subtypeIndex); return -1; }
	asIObjectType   *GetSubType(asUINT subtypeIndex = 0) const { UNUSED_VAR(subtypeIndex); return 0; }
	asUINT           GetSubTypeCount() const { return 0; }

	// Interfaces
	asUINT           GetInterfaceCount() const { return 0; }
	asIObjectType   *GetInterface(asUINT index) const { UNUSED_VAR(index); return 0; }
	bool             Implements(const asIObjectType *objType) const { UNUSED_VAR(objType); return false; }

	// Factories
	asUINT             GetFactoryCount() const { return 0; }
	asIScriptFunction *GetFactoryByIndex(asUINT index) const { UNUSED_VAR(index); return 0; }
	asIScriptFunction *GetFactoryByDecl(const char *decl) const { UNUSED_VAR(decl); return 0; }

	// Methods
	asUINT             GetMethodCount() const { return 0; }
	asIScriptFunction *GetMethodByIndex(asUINT index, bool getVirtual) const { UNUSED_VAR(index); UNUSED_VAR(getVirtual); return 0; }
	asIScriptFunction *GetMethodByName(const char *in_name, bool getVirtual) const { UNUSED_VAR(in_name); UNUSED_VAR(getVirtual); return 0; }
	asIScriptFunction *GetMethodByDecl(const char *decl, bool getVirtual) const { UNUSED_VAR(decl); UNUSED_VAR(getVirtual); return 0; }

	// Properties
	asUINT      GetPropertyCount() const { return 0; }
	int         GetProperty(asUINT index, const char **name, int *typeId, bool *isPrivate, bool *isProtected, int *offset, bool *isReference, asDWORD *accessMask) const;
	const char *GetPropertyDeclaration(asUINT index, bool includeNamespace = false) const { UNUSED_VAR(index); UNUSED_VAR(includeNamespace); return 0; }

	// Behaviours
	asUINT             GetBehaviourCount() const { return 0; }
	asIScriptFunction *GetBehaviourByIndex(asUINT index, asEBehaviours *outBehaviour) const { UNUSED_VAR(index); UNUSED_VAR(outBehaviour); return 0; }

	// Child types
	asUINT             GetChildFuncdefCount() const { return 0; }
	asIScriptFunction *GetChildFuncdef(asUINT index) const { UNUSED_VAR(index); return 0; }

	// User data
	void *SetUserData(void *data, asPWORD type);
	void *GetUserData(asPWORD type) const;

	//===========================================
	// Internal
	//===========================================
public:
	asCTypeInfo(asCScriptEngine *engine);
	virtual ~asCTypeInfo();

	// Keep an internal reference counter to separate references coming from 
	// application or script objects and references coming from the script code
	virtual int AddRefInternal();
	virtual int ReleaseInternal();

	void CleanUserData();

	bool IsShared() const;

	// These can be safely used on null pointers (which will return null)
	asCObjectType *CastToObjectType();
	asCEnumType   *CastToEnumType();


	asCString                    name;
	asSNameSpace                *nameSpace;
	int                          size;
	mutable int                  typeId;
	asDWORD                      flags;
	asDWORD                      accessMask;

	// Store the script section where the code was declared
	int                             scriptSectionIdx;
	// Store the location where the function was declared (row in the lower 20 bits, and column in the upper 12)
	int                             declaredAt;

	asCScriptEngine  *engine;
	asCModule        *module;
	asCArray<asPWORD> userData;

protected:
	friend class asCScriptEngine;
	friend class asCConfigGroup;
	friend class asCModule;
	asCTypeInfo();

	mutable asCAtomic externalRefCount;
	asCAtomic         internalRefCount;
};

struct asSEnumValue
{
	asCString name;
	int       value;
};

class asCEnumType : public asCTypeInfo
{
public:
	asCEnumType(asCScriptEngine *engine) : asCTypeInfo(engine) {}
	~asCEnumType();

	asCArray<asSEnumValue*> enumValues;

protected:
	asCEnumType() : asCTypeInfo() {}
};

END_AS_NAMESPACE

#endif // AS_NO_COMPILER

#endif
