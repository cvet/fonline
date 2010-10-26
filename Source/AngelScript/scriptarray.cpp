#include <new>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "scriptarray.h"

BEGIN_AS_NAMESPACE

static void RegisterScriptArray_Native(asIScriptEngine *engine);
static void RegisterScriptArray_Generic(asIScriptEngine *engine);

struct SArrayBuffer
{
	asDWORD numElements;
	asBYTE  data[1];
};

static CScriptArray* ScriptArrayFactory2(asIObjectType *ot, asUINT length)
{
	CScriptArray *a = new CScriptArray(length, ot);

	// It's possible the constructor raised a script exception, in which case we 
	// need to free the memory and return null instead, else we get a memory leak.
	asIScriptContext *ctx = asGetActiveContext();
	if( ctx && ctx->GetState() == asEXECUTION_EXCEPTION )
	{
		delete a;
		return 0;
	}

	return a;
}

static CScriptArray* ScriptArrayFactoryDefVal(asIObjectType *ot, asUINT length, void *defVal)
{
	CScriptArray *a = new CScriptArray(length, defVal, ot);

	// It's possible the constructor raised a script exception, in which case we 
	// need to free the memory and return null instead, else we get a memory leak.
	asIScriptContext *ctx = asGetActiveContext();
	if( ctx && ctx->GetState() == asEXECUTION_EXCEPTION )
	{
		delete a;
		return 0;
	}

	return a;	
}

static CScriptArray* ScriptArrayFactory(asIObjectType *ot)
{
	return ScriptArrayFactory2(ot, 0);
}

// This optional callback is called when the template type is first used by the compiler.
// It allows the application to validate if the template can be instanciated for the requested 
// subtype at compile time, instead of at runtime.
static bool ScriptArrayTemplateCallback(asIObjectType *ot)
{
	// Make sure the subtype can be instanciated with a default factory/constructor, 
	// otherwise we won't be able to instanciate the elements. Script classes always
	// have default factories, so we don't have to worry about those.
	int typeId = ot->GetSubTypeId();
	if( (typeId & asTYPEID_MASK_OBJECT) && !(typeId & asTYPEID_OBJHANDLE) && !(typeId & asTYPEID_SCRIPTOBJECT) )
	{
		asIObjectType *subtype = ot->GetEngine()->GetObjectTypeById(typeId);
		asDWORD flags = subtype->GetFlags();
		if( (flags & asOBJ_VALUE) && !(flags & asOBJ_POD) )
		{
			// Verify that there is a default constructor
			for( int n = 0; n < subtype->GetBehaviourCount(); n++ )
			{
				asEBehaviours beh;
				int funcId = subtype->GetBehaviourByIndex(n, &beh);
				if( beh != asBEHAVE_CONSTRUCT ) continue;

				asIScriptFunction *func = ot->GetEngine()->GetFunctionDescriptorById(funcId);
				if( func->GetParamCount() == 0 )
				{
					// Found the default constructor
					return true;
				}
			}

			// There is no default constructor
			return false;
		}
		else if( (flags & asOBJ_REF) )
		{
			// Verify that there is a default factory
			for( int n = 0; n < subtype->GetFactoryCount(); n++ )
			{
				int funcId = subtype->GetFactoryIdByIndex(n);
				asIScriptFunction *func = ot->GetEngine()->GetFunctionDescriptorById(funcId);
				if( func->GetParamCount() == 0 )
				{
					// Found the default factory
					return true;
				}
			}	

			// No default factory
			return false;
		}
	}

	// The type is ok
	return true;
}

// Registers the template array type
void RegisterScriptArray(asIScriptEngine *engine, bool defaultArray)
{
	if( strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") == 0 )
		RegisterScriptArray_Native(engine);
	else
		RegisterScriptArray_Generic(engine);

	if( defaultArray )
	{
		int r = engine->RegisterDefaultArrayType("array<T>"); assert( r >= 0 );
	}
}

static void RegisterScriptArray_Native(asIScriptEngine *engine)
{
	int r;
	
	// Register the array type as a template
	r = engine->RegisterObjectType("array<class T>", 0, asOBJ_REF | asOBJ_GC | asOBJ_TEMPLATE); assert( r >= 0 );

	// Register a callback for validating the subtype before it is used
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in)", asFUNCTION(ScriptArrayTemplateCallback), asCALL_CDECL); assert( r >= 0 );

	// Templates receive the object type as the first parameter. To the script writer this is hidden
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_FACTORY, "array<T>@ f(int&in)", asFUNCTIONPR(ScriptArrayFactory, (asIObjectType*), CScriptArray*), asCALL_CDECL); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_FACTORY, "array<T>@ f(int&in, uint)", asFUNCTIONPR(ScriptArrayFactory2, (asIObjectType*, asUINT), CScriptArray*), asCALL_CDECL); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_FACTORY, "array<T>@ f(int&in, uint, const T &in)", asFUNCTIONPR(ScriptArrayFactoryDefVal, (asIObjectType*, asUINT, void *), CScriptArray*), asCALL_CDECL); assert( r >= 0 );

	// Register the factory that will be used for initialization lists
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_LIST_FACTORY, "array<T>@ f(int&in, uint)", asFUNCTIONPR(ScriptArrayFactory2, (asIObjectType*, asUINT), CScriptArray*), asCALL_CDECL); assert( r >= 0 );

	// The memory management methods
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_ADDREF, "void f()", asMETHOD(CScriptArray,AddRef), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_RELEASE, "void f()", asMETHOD(CScriptArray,Release), asCALL_THISCALL); assert( r >= 0 );

	// The index operator returns the template subtype
	r = engine->RegisterObjectMethod("array<T>", "T &opIndex(uint)", asMETHOD(CScriptArray, At), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "const T &opIndex(uint) const", asMETHOD(CScriptArray, At), asCALL_THISCALL); assert( r >= 0 );
	
	// The assignment operator
	r = engine->RegisterObjectMethod("array<T>", "array<T> &opAssign(const array<T>&in)", asMETHOD(CScriptArray, operator=), asCALL_THISCALL); assert( r >= 0 );

	// Other methods
	r = engine->RegisterObjectMethod("array<T>", "void insertAt(uint, const T&in)", asMETHOD(CScriptArray, InsertAt), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "void removeAt(uint)", asMETHOD(CScriptArray, RemoveAt), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "void insertFirst(const T&in)", asMETHOD(CScriptArray, InsertFirst), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "void removeFirst()", asMETHOD(CScriptArray, RemoveFirst), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "void insertLast(const T&in)", asMETHOD(CScriptArray, InsertLast), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "void removeLast()", asMETHOD(CScriptArray, RemoveLast), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "uint length() const", asMETHOD(CScriptArray, GetSize), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "void resize(uint)", asMETHODPR(CScriptArray, Resize, (asUINT), void), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "void grow(uint)", asMETHOD(CScriptArray, Grow), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "void reduce(uint)", asMETHOD(CScriptArray, Reduce), asCALL_THISCALL); assert( r >= 0 );

	r = engine->RegisterObjectMethod("array<T>", "T &first()", asMETHOD(CScriptArray, First), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "const T &first() const", asMETHOD(CScriptArray, First), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "T &last()", asMETHOD(CScriptArray, Last), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "const T &last() const", asMETHOD(CScriptArray, Last), asCALL_THISCALL); assert( r >= 0 );

	// Register GC behaviours in case the array needs to be garbage collected
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_GETREFCOUNT, "int f()", asMETHOD(CScriptArray, GetRefCount), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_SETGCFLAG, "void f()", asMETHOD(CScriptArray, SetFlag), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_GETGCFLAG, "bool f()", asMETHOD(CScriptArray, GetFlag), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_ENUMREFS, "void f(int&in)", asMETHOD(CScriptArray, EnumReferences), asCALL_THISCALL); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_RELEASEREFS, "void f(int&in)", asMETHOD(CScriptArray, ReleaseAllHandles), asCALL_THISCALL); assert( r >= 0 );
}

CScriptArray &CScriptArray::operator=(const CScriptArray &other)
{
	// Only perform the copy if the array types are the same
	if( &other != this && 
		other.GetArrayObjectType() == GetArrayObjectType() )
	{
		if( buffer )
		{
			DeleteBuffer(buffer);
			buffer = 0;
		}

		// Copy all elements from the other array
		CreateBuffer(&buffer, other.buffer->numElements);
		CopyBuffer(buffer, other.buffer);
	}

	return *this;
}

CScriptArray::CScriptArray(asUINT length, asIObjectType *ot)
{
	refCount = 1;
	gcFlag = false;
	objType = ot;
	objType->AddRef();
	buffer = 0;

	// Determine element size
	// TODO: Should probably store the template sub type id as well
	int typeId = objType->GetSubTypeId();
	if( typeId & asTYPEID_MASK_OBJECT )
	{
		elementSize = sizeof(asPWORD);
	}
	else
	{
		elementSize = objType->GetEngine()->GetSizeOfPrimitiveType(typeId);
	}

	isArrayOfHandles = typeId & asTYPEID_OBJHANDLE ? true : false;

	// Make sure the array size isn't too large for us to handle
	if( !CheckMaxSize(length) )
	{
		// Don't continue with the initialization
		return;
	}

	CreateBuffer(&buffer, length);

	// Notify the GC of the successful creation
	if( objType->GetFlags() & asOBJ_GC )
		objType->GetEngine()->NotifyGarbageCollectorOfNewObject(this, objType->GetTypeId());
}

CScriptArray::CScriptArray(asUINT length, void *defVal, asIObjectType *ot)
{
	refCount = 1;
	gcFlag = false;
	objType = ot;
	objType->AddRef();
	buffer = 0;

	// Determine element size
	// TODO: Should probably store the template sub type id as well
	int typeId = objType->GetSubTypeId();
	if( typeId & asTYPEID_MASK_OBJECT )
	{
		elementSize = sizeof(asPWORD);
	}
	else
	{
		elementSize = objType->GetEngine()->GetSizeOfPrimitiveType(typeId);
	}

	isArrayOfHandles = typeId & asTYPEID_OBJHANDLE ? true : false;

	// Make sure the array size isn't too large for us to handle
	if( !CheckMaxSize(length) )
	{
		// Don't continue with the initialization
		return;
	}

	CreateBuffer(&buffer, length);

	// Notify the GC of the successful creation
	if( objType->GetFlags() & asOBJ_GC )
		objType->GetEngine()->NotifyGarbageCollectorOfNewObject(this, objType->GetTypeId());

	// Initialize the elements with the default value
	for( asUINT n = 0; n < GetSize(); n++ )
		SetValue(n, defVal);
}

// Internal
void CScriptArray::SetValue(asUINT index, void *value)
{
	int typeId = objType->GetSubTypeId();

	if( (typeId & ~0x03FFFFFF) && !(typeId & asTYPEID_OBJHANDLE) )
		objType->GetEngine()->CopyScriptObject(At(index), value, typeId);
	else if( typeId & asTYPEID_OBJHANDLE )
	{
		*(void**)At(index) = *(void**)value;
		objType->GetEngine()->AddRefScriptObject(*(void**)value, typeId);
	}
	else if( typeId == asTYPEID_BOOL ||
			 typeId == asTYPEID_INT8 ||
			 typeId == asTYPEID_UINT8 )
		*(char*)At(index) = *(char*)value;
	else if( typeId == asTYPEID_INT16 ||
			 typeId == asTYPEID_UINT16 )
		*(short*)At(index) = *(short*)value;
	else if( typeId == asTYPEID_INT32 ||
			 typeId == asTYPEID_UINT32 ||
			 typeId == asTYPEID_FLOAT )
		*(int*)At(index) = *(int*)value;
	else if( typeId == asTYPEID_INT64 ||
			 typeId == asTYPEID_UINT64 ||
			 typeId == asTYPEID_DOUBLE )
		*(double*)At(index) = *(double*)value;
}

CScriptArray::~CScriptArray()
{
	if( buffer )
	{
		DeleteBuffer(buffer);
		buffer = 0;
	}
	if( objType ) objType->Release();
}

asUINT CScriptArray::GetSize() const
{
	return buffer->numElements;
}

int CScriptArray::GetElementSize() const
{
	return elementSize;
}

void CScriptArray::Resize(asUINT numElements)
{
	if( numElements & 0x80000000 )
	{
		CheckMaxSize(numElements);
		return;
	}

	Resize((int)numElements - (int)buffer->numElements, -1);
}

void CScriptArray::Grow(asUINT numElements)
{
	if(numElements == 0) return;

	Resize(GetSize() + numElements);
}

void CScriptArray::Reduce(asUINT numElements)
{
	if(numElements == 0) return;

	asUINT size = GetSize();
	if(numElements > size)
	{
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx ) ctx->SetException("Array size is less than reduce count");
		return;
	}
	Resize(size - numElements);
}

// Internal
void CScriptArray::Resize(int delta, asUINT at)
{
	if( delta < 0 )
	{
		if( -delta > (int)buffer->numElements )
			delta = -(int)buffer->numElements;
		if( at > buffer->numElements + delta )
			at = buffer->numElements + delta;
	}
	else if( delta > 0 )
	{
		// Make sure the array size isn't too large for us to handle
		if( delta > 0 && !CheckMaxSize(buffer->numElements + delta) )
			return;

		if( at > buffer->numElements )
			at = buffer->numElements;
	}

	if( delta == 0 ) return;

	// Allocate memory for the buffer
	SArrayBuffer *newBuffer;
	newBuffer = (SArrayBuffer*)new asBYTE[sizeof(SArrayBuffer)-1 + elementSize*(buffer->numElements + delta)];
	newBuffer->numElements = buffer->numElements + delta;

	int c = newBuffer->numElements > buffer->numElements ? buffer->numElements : newBuffer->numElements;
	memcpy(newBuffer->data, buffer->data, at*elementSize);
	if( delta > 0 && at < buffer->numElements )
		memcpy(newBuffer->data + (at+delta)*elementSize, buffer->data + at*elementSize, (buffer->numElements-at)*elementSize);
	else if( delta < 0 && at < buffer->numElements )
		memcpy(newBuffer->data + at*elementSize, buffer->data + (at-delta)*elementSize, (buffer->numElements-at+delta)*elementSize);

	int typeId = objType->GetSubTypeId();
	if( typeId & asTYPEID_MASK_OBJECT )
	{
		if( delta > 0 )
			Construct(newBuffer, at, at+delta);
		else if( delta < 0 )
			Destruct(buffer, at, at-delta);
	}

	// Release the old buffer
	delete[] (asBYTE*)buffer;

	buffer = newBuffer;
}

// internal
bool CScriptArray::CheckMaxSize(asUINT numElements)
{
	// This code makes sure the size of the buffer that is allocated 
	// for the array doesn't overflow and becomes smaller than requested

	asUINT maxSize = 0xFFFFFFFFul - sizeof(SArrayBuffer) + 1;
	if( objType->GetSubTypeId() & asTYPEID_MASK_OBJECT )
	{
		maxSize /= sizeof(void*);
	}
	else
	{
		maxSize /= elementSize;
	}

	if( numElements > maxSize )
	{
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
		{
			// Set a script exception
			ctx->SetException("Too large array size");
		}

		return false;
	}

	// OK
	return true;
}

asIObjectType *CScriptArray::GetArrayObjectType() const
{
	return objType;
}

int CScriptArray::GetArrayTypeId() const
{
	return objType->GetTypeId();
}

int CScriptArray::GetElementTypeId() const
{
	return objType->GetSubTypeId();
}

void CScriptArray::InsertAt(asUINT index, void *value)
{
	if( index > buffer->numElements )
	{
		// If this is called from a script we raise a script exception
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Index out of bounds");
		return;
	}

	// Make room for the new element
	Resize(1, index);

	// Set the value of the new element
	SetValue(index, value);
}

void CScriptArray::InsertFirst(void *value)
{
	InsertAt(0, value);
}

void CScriptArray::InsertLast(void *value)
{
	InsertAt(buffer->numElements, value);
}

void CScriptArray::RemoveAt(asUINT index)
{
	if( index >= buffer->numElements )
	{
		// If this is called from a script we raise a script exception
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Index out of bounds");
		return;
	}

	// Remove the element
	Resize(-1, index);
}

void CScriptArray::RemoveFirst()
{
	RemoveAt(0);
}

void CScriptArray::RemoveLast()
{
	RemoveAt(buffer->numElements-1);
}

// Return a pointer to the array element. Returns 0 if the index is out of bounds
void *CScriptArray::At(asUINT index)
{
	if( index >= buffer->numElements )
	{
		// If this is called from a script we raise a script exception
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Index out of bounds");
		return 0;
	}
	else
	{
		int typeId = objType->GetSubTypeId();
		if( (typeId & asTYPEID_MASK_OBJECT) && !isArrayOfHandles )
			return (void*)((size_t*)buffer->data)[index];
		else
			return buffer->data + elementSize*index;
	}
}

void *CScriptArray::First()
{
	return At(0);
}

void *CScriptArray::Last()
{
	return At(buffer->numElements ? buffer->numElements - 1 : 0);
}

// internal
void CScriptArray::CreateBuffer(SArrayBuffer **buf, asUINT numElements)
{
	int typeId = objType->GetSubTypeId();
	if( typeId & asTYPEID_MASK_OBJECT )
	{
		*buf = (SArrayBuffer*)new asBYTE[sizeof(SArrayBuffer)-1+sizeof(void*)*numElements];
		(*buf)->numElements = numElements;
	}
	else
	{
		*buf = (SArrayBuffer*)new asBYTE[sizeof(SArrayBuffer)-1+elementSize*numElements];
		(*buf)->numElements = numElements;
	}

	Construct(*buf, 0, numElements);
}

// internal
void CScriptArray::DeleteBuffer(SArrayBuffer *buf)
{
	Destruct(buf, 0, buf->numElements);

	// Free the buffer
	delete[] (asBYTE*)buf;
}

// internal
void CScriptArray::Construct(SArrayBuffer *buf, asUINT start, asUINT end)
{
	int typeId = objType->GetSubTypeId();
	if( isArrayOfHandles )
	{
		// Set all object handles to null
		void *d = (void*)(buf->data + start * sizeof(void*));
		memset(d, 0, (end-start)*sizeof(void*));
	}
	else if( typeId & asTYPEID_MASK_OBJECT )
	{
		void **max = (void**)(buf->data + end * sizeof(void*));
		void **d = (void**)(buf->data + start * sizeof(void*));

		asIScriptEngine *engine = objType->GetEngine();

		for( ; d < max; d++ )
			*d = (void*)engine->CreateScriptObject(typeId);
	}
}

// internal
void CScriptArray::Destruct(SArrayBuffer *buf, asUINT start, asUINT end)
{
	int typeId = objType->GetSubTypeId();
	if( typeId & asTYPEID_MASK_OBJECT )
	{
		asIScriptEngine *engine = objType->GetEngine();

		void **max = (void**)(buf->data + end * sizeof(void*));
		void **d   = (void**)(buf->data + start * sizeof(void*));

		for( ; d < max; d++ )
		{
			if( *d )
				engine->ReleaseScriptObject(*d, typeId);
		}
	}
}

// internal
void CScriptArray::CopyBuffer(SArrayBuffer *dst, SArrayBuffer *src)
{
	asIScriptEngine *engine = objType->GetEngine();
	if( isArrayOfHandles )
	{
		// Copy the references and increase the reference counters
		if( dst->numElements > 0 && src->numElements > 0 )
		{
			int typeId = objType->GetSubTypeId();
			int count = dst->numElements > src->numElements ? src->numElements : dst->numElements;

			void **max = (void**)(dst->data + count * sizeof(void*));
			void **d   = (void**)dst->data;
			void **s   = (void**)src->data;
			
			for( ; d < max; d++, s++ )
			{
				*d = *s;
				if( *d )
					engine->AddRefScriptObject(*d, typeId);
			}
		}
	}
	else
	{
		int typeId = objType->GetSubTypeId();

		if( dst->numElements > 0 && src->numElements > 0 )
		{
			int count = dst->numElements > src->numElements ? src->numElements : dst->numElements;
			if( typeId & asTYPEID_MASK_OBJECT )
			{
				// Call the assignment operator on all of the objects
				void **max = (void**)(dst->data + count * sizeof(void*));
				void **d   = (void**)dst->data;
				void **s   = (void**)src->data;

				for( ; d < max; d++, s++ )
					engine->CopyScriptObject(*d, *s, typeId);
			}
			else
			{
				// Primitives are copied byte for byte
				memcpy(dst->data, src->data, count*elementSize);
			}
		}
	}
}

// GC behaviour
void CScriptArray::EnumReferences(asIScriptEngine *engine)
{
	// If the array is holding handles, then we need to notify the GC of them
	int typeId = objType->GetSubTypeId();
	if( typeId & asTYPEID_MASK_OBJECT )
	{
		void **d = (void**)buffer->data;
		for( asUINT n = 0; n < buffer->numElements; n++ )
		{
			if( d[n] )
				engine->GCEnumCallback(d[n]);
		}
	}
}

// GC behaviour
void CScriptArray::ReleaseAllHandles(asIScriptEngine *engine)
{
	// Resizing to zero will release everything
	Resize(0);
}

void CScriptArray::AddRef() const
{
	// Clear the GC flag then increase the counter
	gcFlag = false;
	refCount++;
}

void CScriptArray::Release() const
{
	// Now do the actual releasing (clearing the flag set by GC)
	gcFlag = false;
	if( --refCount == 0 )
	{
		delete this;
	}
}

// GC behaviour
int CScriptArray::GetRefCount()
{
	return refCount;
}

// GC behaviour
void CScriptArray::SetFlag()
{
	gcFlag = true;
}

// GC behaviour
bool CScriptArray::GetFlag()
{
	return gcFlag;
}

//--------------------------------------------
// Generic calling conventions

static void ScriptArrayFactory_Generic(asIScriptGeneric *gen)
{
	asIObjectType *ot = *(asIObjectType**)gen->GetAddressOfArg(0);

	*(CScriptArray**)gen->GetAddressOfReturnLocation() = ScriptArrayFactory(ot);
}

static void ScriptArrayFactory2_Generic(asIScriptGeneric *gen)
{
	asIObjectType *ot = *(asIObjectType**)gen->GetAddressOfArg(0);
	asUINT length = gen->GetArgDWord(1);

	*(CScriptArray**)gen->GetAddressOfReturnLocation() = ScriptArrayFactory2(ot, length);
}

static void ScriptArrayFactoryDefVal_Generic(asIScriptGeneric *gen)
{
	asIObjectType *ot = *(asIObjectType**)gen->GetAddressOfArg(0);
	asUINT length = gen->GetArgDWord(1);
	void *defVal = gen->GetArgAddress(2);

	*(CScriptArray**)gen->GetAddressOfReturnLocation() = ScriptArrayFactoryDefVal(ot, length, defVal);
}

static void ScriptArrayTemplateCallback_Generic(asIScriptGeneric *gen)
{
	asIObjectType *ot = *(asIObjectType**)gen->GetAddressOfArg(0);
	*(bool*)gen->GetAddressOfReturnLocation() = ScriptArrayTemplateCallback(ot);
}

static void ScriptArrayAssignment_Generic(asIScriptGeneric *gen)
{
	CScriptArray *other = (CScriptArray*)gen->GetArgObject(0);
	CScriptArray *self = (CScriptArray*)gen->GetObject();

	*self = *other;

	gen->SetReturnObject(self);
}

static void ScriptArrayAt_Generic(asIScriptGeneric *gen)
{
	asUINT index = gen->GetArgDWord(0);
	CScriptArray *self = (CScriptArray*)gen->GetObject();

	gen->SetReturnAddress(self->At(index));
}

static void ScriptArrayLength_Generic(asIScriptGeneric *gen)
{
	CScriptArray *self = (CScriptArray*)gen->GetObject();

	gen->SetReturnDWord(self->GetSize());
}

static void ScriptArrayResize_Generic(asIScriptGeneric *gen)
{
	asUINT size = gen->GetArgDWord(0);
	CScriptArray *self = (CScriptArray*)gen->GetObject();

	self->Resize(size);
}

static void ScriptArrayAddRef_Generic(asIScriptGeneric *gen)
{
	CScriptArray *self = (CScriptArray*)gen->GetObject();
	self->AddRef();
}

static void ScriptArrayRelease_Generic(asIScriptGeneric *gen)
{
	CScriptArray *self = (CScriptArray*)gen->GetObject();
	self->Release();
}

static void ScriptArrayGetRefCount_Generic(asIScriptGeneric *gen)
{
	CScriptArray *self = (CScriptArray*)gen->GetObject();
	*(int*)gen->GetAddressOfReturnLocation() = self->GetRefCount();
}

static void ScriptArraySetFlag_Generic(asIScriptGeneric *gen)
{
	CScriptArray *self = (CScriptArray*)gen->GetObject();
	self->SetFlag();
}

static void ScriptArrayGetFlag_Generic(asIScriptGeneric *gen)
{
	CScriptArray *self = (CScriptArray*)gen->GetObject();
	*(bool*)gen->GetAddressOfReturnLocation() = self->GetFlag();
}

static void ScriptArrayEnumReferences_Generic(asIScriptGeneric *gen)
{
	CScriptArray *self = (CScriptArray*)gen->GetObject();
	asIScriptEngine *engine = *(asIScriptEngine**)gen->GetAddressOfArg(0);
	self->EnumReferences(engine);
}

static void ScriptArrayReleaseAllHandles_Generic(asIScriptGeneric *gen)
{
	CScriptArray *self = (CScriptArray*)gen->GetObject();
	asIScriptEngine *engine = *(asIScriptEngine**)gen->GetAddressOfArg(0);
	self->ReleaseAllHandles(engine);
}

static void RegisterScriptArray_Generic(asIScriptEngine *engine)
{
	int r;
	
	r = engine->RegisterObjectType("array<class T>", 0, asOBJ_REF | asOBJ_GC | asOBJ_TEMPLATE); assert( r >= 0 );

	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_FACTORY, "array<T>@ f(int&in)", asFUNCTION(ScriptArrayFactory_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_TEMPLATE_CALLBACK, "bool f(int&in)", asFUNCTION(ScriptArrayTemplateCallback_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_FACTORY, "array<T>@ f(int&in, uint)", asFUNCTION(ScriptArrayFactory2_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_FACTORY, "array<T>@ f(int&in, uint, const T &in)", asFUNCTION(ScriptArrayFactoryDefVal_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_LIST_FACTORY, "array<T>@ f(int&in, uint)", asFUNCTION(ScriptArrayFactory2_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_ADDREF, "void f()", asFUNCTION(ScriptArrayAddRef_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_RELEASE, "void f()", asFUNCTION(ScriptArrayRelease_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "T &opIndex(uint)", asFUNCTION(ScriptArrayAt_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "const T &opIndex(uint) const", asFUNCTION(ScriptArrayAt_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "array<T> &opAssign(const array<T>&in)", asFUNCTION(ScriptArrayAssignment_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "uint length() const", asFUNCTION(ScriptArrayLength_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectMethod("array<T>", "void resize(uint)", asFUNCTION(ScriptArrayResize_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_GETREFCOUNT, "int f()", asFUNCTION(ScriptArrayGetRefCount_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_SETGCFLAG, "void f()", asFUNCTION(ScriptArraySetFlag_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_GETGCFLAG, "bool f()", asFUNCTION(ScriptArrayGetFlag_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_ENUMREFS, "void f(int&in)", asFUNCTION(ScriptArrayEnumReferences_Generic), asCALL_GENERIC); assert( r >= 0 );
	r = engine->RegisterObjectBehaviour("array<T>", asBEHAVE_RELEASEREFS, "void f(int&in)", asFUNCTION(ScriptArrayReleaseAllHandles_Generic), asCALL_GENERIC); assert( r >= 0 );
}

END_AS_NAMESPACE
