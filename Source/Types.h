#ifndef ___TYPES___
#define ___TYPES___

#include "PlatformSpecific.h"
#include <map>
#include <string>
#include <set>
#include <list>
#include <vector>
#include <deque>
using namespace std;

#if defined(FO_MSVC)
	typedef unsigned char    uchar;
	typedef unsigned short   ushort;
	typedef unsigned int     uint;
	typedef unsigned __int64 uint64;
	typedef __int64          int64;
#elif defined(FO_GCC)
	#include <inttypes.h>
	typedef unsigned char    uchar;
	typedef unsigned short   ushort;
	typedef unsigned int     uint;
	typedef uint64_t         uint64;
	typedef int64_t          int64;
#endif

typedef std::map<std::string,uchar> StrUCharMap;
typedef std::map<std::string,uchar>::iterator StrUCharMapIt;
typedef std::map<std::string,uchar>::value_type StrUCharMapVal;
typedef std::map<uchar,std::string> UCharStrMap;
typedef std::map<uchar,std::string>::iterator UCharStrMapIt;
typedef std::map<uchar,std::string>::value_type UCharStrMapVal;
typedef std::map<std::string,std::string> StrMap;
typedef std::map<std::string,std::string>::iterator StrMapIt;
typedef std::map<std::string,std::string>::value_type StrMapVal;
typedef std::map<uint,std::string> UIntStrMap;
typedef std::map<uint,std::string>::iterator UIntStrMapIt;
typedef std::map<uint,std::string>::value_type UIntStrMapVal;
typedef std::map<std::string,ushort> StrUShortMap;
typedef std::map<std::string,ushort>::iterator StrUShortMapIt;
typedef std::map<std::string,ushort>::value_type StrUShortMapVal;
typedef std::map<std::string,uint> StrUIntMap;
typedef std::map<std::string,uint>::iterator StrUIntMapIt;
typedef std::map<std::string,uint>::value_type StrUIntMapVal;
typedef std::map<std::string,void*> StrPtrMap;
typedef std::map<std::string,void*>::iterator StrPtrMapIt;
typedef std::map<std::string,void*>::value_type StrPtrMapVal;
typedef std::map<ushort,std::string> UShortStrMap;
typedef std::map<ushort,std::string>::iterator UShortStrMapIt;
typedef std::map<ushort,std::string>::value_type UShortStrMapVal;
typedef std::map<std::string,uint> StrUIntMap;
typedef std::map<std::string,uint>::iterator StrUIntMapIt;
typedef std::map<std::string,uint>::value_type StrUIntMapVal;
typedef std::map<uint,uint> UIntMap;
typedef std::map<uint,uint>::iterator UIntMapIt;
typedef std::map<uint,uint>::value_type UIntMapVal;
typedef std::map<int,int> IntMap;
typedef std::map<int,int>::iterator IntMapIt;
typedef std::map<int,int>::value_type IntMapVal;
typedef std::map<int,float> IntFloatMap;
typedef std::map<int,float>::iterator IntFloatMapIt;
typedef std::map<int,float>::value_type IntFloatMapVal;
typedef std::map<int,void*> IntPtrMap;
typedef std::map<int,void*>::iterator IntPtrMapIt;
typedef std::map<int,void*>::value_type IntPtrMapVal;
typedef std::map<uint,float> UIntFloatMap;
typedef std::map<uint,float>::iterator UIntFloatMapIt;
typedef std::map<uint,float>::value_type UIntFloatMapVal;

typedef std::multimap<uint,std::string> UIntStrMulMap;
typedef std::multimap<uint,std::string>::iterator UIntStrMulMapIt;
typedef std::multimap<uint,std::string>::value_type UIntStrMulMapVal;

typedef std::vector<void*> PtrVec;
typedef std::vector<void*>::iterator PtrVecIt;
typedef std::vector<int> IntVec;
typedef std::vector<int>::iterator IntVecIt;
typedef std::vector<uchar> UCharVec;
typedef std::vector<uchar>::iterator UCharVecIt;
typedef std::vector<short> ShortVec;
typedef std::vector<ushort> UShortVec;
typedef std::vector<ushort>::iterator UShortVecIt;
typedef std::vector<uint> UIntVec;
typedef std::vector<uint>::iterator UIntVecIt;
typedef std::vector<char> CharVec;
typedef std::vector<std::string> StrVec;
typedef std::vector<std::string>::iterator StrVecIt;
typedef std::vector<char*> PCharVec;
typedef std::vector<uchar*> PUCharVec;
typedef std::vector<float> FloatVec;
typedef std::vector<float>::iterator FloatVecIt;
typedef std::vector<uint64> UInt64Vec;
typedef std::vector<uint64>::iterator UInt64VecIt;
typedef std::vector<bool>BoolVec;
typedef std::vector<bool>::iterator BoolVecIt;

typedef std::set<std::string> StrSet;
typedef std::set<uchar> UCharSet;
typedef std::set<uchar>::iterator UCharSetIt;
typedef std::set<ushort> UShortSet;
typedef std::set<ushort>::iterator UShortSetIt;
typedef std::set<uint> UIntSet;
typedef std::set<uint>::iterator UIntSetIt;
typedef std::set<int> IntSet;
typedef std::set<int>::iterator IntSetIt;

typedef std::pair<int,int> IntPair;
typedef std::pair<ushort,ushort> UShortPair;
typedef std::pair<uint,uint> UIntPair;
typedef std::pair<char,char> CharPair;
typedef std::pair<char*,char*> PCharPair;
typedef std::pair<uchar,uchar> UCharPair;

typedef std::vector<UShortPair> UShortPairVec;
typedef std::vector<UShortPair>::iterator UShortPairVecIt;
typedef std::vector<UShortPair>::value_type UShortPairVecVal;
typedef std::vector<IntPair> IntPairVec;
typedef std::vector<IntPair>::iterator IntPairVecIt;
typedef std::vector<IntPair>::value_type IntPairVecVal;
typedef std::vector<UIntPair> UIntPairVec;
typedef std::vector<UIntPair>::iterator UIntPairVecIt;
typedef std::vector<UIntPair>::value_type UIntPairVecVal;
typedef std::vector<PCharPair> PCharPairVec;
typedef std::vector<PCharPair>::iterator PCharPairVecIt;
typedef std::vector<PCharPair>::value_type PCharPairVecVal;
typedef std::vector<UCharPair> UCharPairVec;
typedef std::vector<UCharPair>::iterator UCharPairVecIt;
typedef std::vector<UCharPair>::value_type UCharPairVecVal;

#endif // ___TYPES___
