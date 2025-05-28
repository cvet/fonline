#ifndef SCRIPTARRAY_H
#define SCRIPTARRAY_H

#ifndef ANGELSCRIPT_H 
// Avoid having to inform include path if header is already include before
#include <angelscript.h>
#endif

// Sometimes it may be desired to use the same method names as used by C++ STL.
// This may for example reduce time when converting code from script to C++ or
// back.
//
//  0 = off
//  1 = on

#ifndef AS_USE_STLNAMES
#define AS_USE_STLNAMES 0
#endif

BEGIN_AS_NAMESPACE

struct SArrayBuffer;
struct SArrayCache;

class CScriptArray
{
public:
	// Set the memory functions that should be used by all CScriptArrays
	static void SetMemoryFunctions(asALLOCFUNC_t allocFunc, asFREEFUNC_t freeFunc);

	// Factory functions
	static CScriptArray *Create(asITypeInfo *ot);
	static CScriptArray *Create(asITypeInfo *ot, int length);
	static CScriptArray *Create(asITypeInfo *ot, int length, void *defaultValue);
	static CScriptArray *Create(asITypeInfo *ot, void *listBuffer);

	// Memory management
	void AddRef() const;
	void Release() const;

	// Type information
	asITypeInfo *GetArrayObjectType() const;
	int          GetArrayTypeId() const;
	int          GetElementTypeId() const;

	// Get the current size
	int GetSize() const;

	// Returns true if the array is empty
	bool   IsEmpty() const;

	// Pre-allocates memory for elements
	void   Reserve(int maxElements);

	// Resize the array
	void   Resize(int numElements);

	// Get a pointer to an element. Returns 0 if out of bounds
	void       *At(int index);
	const void *At(int index) const;

	// Set value of an element. 
	// The value arg should be a pointer to the value that will be copied to the element.
	// Remember, if the array holds handles the value parameter should be the 
	// address of the handle. The refCount of the object will also be incremented
	void  SetValue(int index, void *value);

	// Copy the contents of one array to another (only if the types are the same)
	CScriptArray &operator=(const CScriptArray&);

	// Compare two arrays
	bool operator==(const CScriptArray &) const;

	// Array manipulation
	void InsertAt(int index, void *value);
	void InsertAt(int index, const CScriptArray &arr);
	void InsertLast(void *value);
	void RemoveAt(int index);
	void RemoveLast();
	void RemoveRange(int start, int count);
	void SortAsc();
	void SortDesc();
	void SortAsc(int startAt, int count);
	void SortDesc(int startAt, int count);
	void Sort(int startAt, int count, bool asc);
	void Reverse();
	int  Find(void *value) const;
	int  Find(int startAt, void *value) const;
	int  FindByRef(void *ref) const;
	int  FindByRef(int startAt, void *ref) const;

	// Return the address of internal buffer for direct manipulation of elements
	void *GetBuffer();

	// GC methods
	int  GetRefCount();
	void SetFlag();
	bool GetFlag();
	void EnumReferences(asIScriptEngine *engine);
	void ReleaseAllHandles(asIScriptEngine *engine);

protected:
	mutable int     refCount;
	mutable bool    gcFlag;
	asITypeInfo    *objType;
	SArrayBuffer   *buffer;
	int             elementSize;
	int             subTypeId;

	// Constructors
	CScriptArray(asITypeInfo *ot, void *initBuf); // Called from script when initialized with list
	CScriptArray(int length, asITypeInfo *ot);
	CScriptArray(int length, void *defVal, asITypeInfo *ot);
	CScriptArray(const CScriptArray &other);
	virtual ~CScriptArray();

	bool  Less(const void *a, const void *b, bool asc, asIScriptContext *ctx, SArrayCache *cache);
	void *GetArrayItemPointer(int index);
	void *GetDataPointer(void *buffer);
	void  Copy(void *dst, void *src);
	void  Precache();
	bool  CheckMaxSize(int numElements);
	void  Resize(int delta, int at);
	void  CreateBuffer(SArrayBuffer **buf, int numElements);
	void  DeleteBuffer(SArrayBuffer *buf);
	void  CopyBuffer(SArrayBuffer *dst, SArrayBuffer *src);
	void  Construct(SArrayBuffer *buf, int start, int end);
	void  Destruct(SArrayBuffer *buf, int start, int end);
	bool  Equals(const void *a, const void *b, asIScriptContext *ctx, SArrayCache *cache) const;
};

void RegisterScriptArray(asIScriptEngine *engine, bool defaultArray);

END_AS_NAMESPACE

#endif
