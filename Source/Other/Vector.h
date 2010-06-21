#ifndef __FOVECTOR__H__
#define __FOVECTOR__H__

/********************************************************************
	created:	26:07:2007   17:36;
	author:		Anton Tsvetinskiy aka Cvet
	purpose:	
*********************************************************************/

#include <list>
#include <map>

using namespace std;

#pragma warning (disable : 4172)

#define INLINE __forceinline

template<class __Key, class __Type, \
class __Sort = less<__Key>, class __Alloc = allocator<__Type> >

class FOVector
{
	typedef list<__Type,__Alloc> LIST;
	typedef LIST::value_type LIST_VAL;
	typedef LIST::iterator LIST_IT;
	typedef map<__Key,LIST_IT,__Sort,__Alloc> MAP;
	typedef MAP::iterator MAP_IT;
	typedef MAP::value_type MAP_VAL;

public:
	INLINE FOVector():size(0),curit(0),mit(0),lit(0){};
	INLINE ~FOVector(){};

	INLINE void Insert(const __Key& index, const __Type& element)
	{
		List.push_front(LIST_VAL(element));
		lit=List.begin();
		Map.insert(MAP_VAL(index,lit));
		size++;
	}

	INLINE const __Type& Find(const __Key& index)
	{
		mit=Map.find(index);
		return (mit==Map.end())?0:*(*mit).second;
	}

	INLINE void Erase(const __Key& index)
	{
		mit=Map.find(index);
		if(mit==Map.end()) return;
		if(curit==(*mit).second) curit=List.begin(); //iterator position???
		List.erase((*mit).second);
		Map.erase(mit);
		size--;
	}

	INLINE void Clear()
	{
		List.clear();
		Map.clear();
		curit=List.end();
		lit=List.end();
		mit=Map.end();
		size=0;
	}

	INLINE const __Type& Begin()
	{
		curit=List.begin();
		return (curit==List.end())?0:*curit;
	}

	INLINE const __Type& Next()
	{
		curit++;
		return (curit==List.end())?0:*curit;
	}

	INLINE bool Count(const __Key& index)
	{
		return Map.count(index)?true:false;
	}

	INLINE unsigned Size()
	{
		return size;
	}

private:
	LIST List;
	MAP Map;

	unsigned size;
	LIST_IT curit;
	MAP_IT mit;
	LIST_IT lit;
};

#endif //__FOVECTOR__H__