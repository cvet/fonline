#ifndef SCRIPTARRAY_H
#define SCRIPTARRAY_H

#include "angelscript.h"

BEGIN_AS_NAMESPACE

struct SArrayBuffer;

class CScriptArray
{
public:
	CScriptArray(asUINT length, asIObjectType *ot);
	CScriptArray(asUINT length, void *defVal, asIObjectType *ot);
	virtual ~CScriptArray();

	void AddRef() const;
	void Release() const;

	// Type information
	asIObjectType *GetArrayObjectType() const;
	int            GetArrayTypeId() const;
	int            GetElementTypeId() const;

	void   Resize(asUINT numElements);
	void   Grow(asUINT numElements);
	void   Reduce(asUINT numElements);
	asUINT GetSize() const;
	int    GetElementSize() const;

	// Get a pointer to an element. Returns 0 if out of bounds
	void  *At(asUINT index);
	void  *First();
	void  *Last();

	CScriptArray &operator=(const CScriptArray&);

	// TODO: Add methods Reverse, Find, etc
	void InsertAt(asUINT index, void *value);
	void RemoveAt(asUINT index);
	void InsertFirst(void *value);
	void RemoveFirst();
	void InsertLast(void *value);
	void RemoveLast();
	void SortAsc();
	void SortDesc();
	void SortAsc(asUINT index, asUINT count);
	void SortDesc(asUINT index, asUINT count);
	void Sort(asUINT index, asUINT count, bool asc);

	// GC methods
	int  GetRefCount();
	void SetFlag();
	bool GetFlag();
	void EnumReferences(asIScriptEngine *engine);
	void ReleaseAllHandles(asIScriptEngine *engine);

protected:
	mutable int       refCount;
	mutable bool      gcFlag;
	asIObjectType    *objType;
	SArrayBuffer     *buffer;
	bool              isArrayOfHandles; // TODO: Since we store subTypeId, it's not really necessary to store this
	int               elementSize;
	int               cmpFuncId;
	int               subTypeId;

	bool  Less(const void *a, const void *b, bool asc, asIScriptContext *ctx);
	void *GetArrayItemPointer(int index);
	void *GetDataPointer(void *buffer);
	void  Copy(void *dst, void *src);
	void  PrepareForSorting();
	bool  CheckMaxSize(asUINT numElements);
	void  Resize(int delta, asUINT at);
	void  SetValue(asUINT index, void *value);
	void  CreateBuffer(SArrayBuffer **buf, asUINT numElements);
	void  DeleteBuffer(SArrayBuffer *buf);
	void  CopyBuffer(SArrayBuffer *dst, SArrayBuffer *src);
	void  Construct(SArrayBuffer *buf, asUINT start, asUINT end);
	void  Destruct(SArrayBuffer *buf, asUINT start, asUINT end);
};

void RegisterScriptArray(asIScriptEngine *engine, bool defaultArray);

END_AS_NAMESPACE

#endif
