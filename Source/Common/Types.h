#ifndef ___TYPES___
#define ___TYPES___

#include "PlatformSpecific.h"
#include <map>
#include <string>
#include <set>
#include <list>
#include <vector>
#include <deque>
#include <sstream>
#include <tuple>

#if defined ( FO_MSVC )
using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using uint64 = unsigned __int64;
using int64 = __int64;
#elif defined ( FO_GCC )
# include <inttypes.h>
using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using uint64 = uint64_t;
using int64 = int64_t;
#endif

using hash = uint;
using max_t = uint;

using std::string;
using std::list;
using std::vector;
using std::map;
using std::multimap;
using std::set;
using std::deque;
using std::pair;
using std::tuple;
using std::istringstream;

using StrUCharMap = map< string, uchar >;
using UCharStrMap = map< uchar, string >;
using StrMap = map< string, string >;
using UIntStrMap = map< uint, string >;
using StrUShortMap = map< string, ushort >;
using StrUIntMap = map< string, uint >;
using StrInt64Map = map< string, int64 >;
using StrPtrMap = map< string, void* >;
using UShortStrMap = map< ushort, string >;
using StrUIntMap = map< string, uint >;
using UIntMap = map< uint, uint >;
using IntMap = map< int, int >;
using IntFloatMap = map< int, float >;
using IntPtrMap = map< int, void* >;
using UIntFloatMap = map< uint, float >;
using UShortUIntMap = map< ushort, uint >;
using SizeTStrMap = map< size_t, string >;
using UIntIntMap = map< uint, int >;
using HashIntMap = map< hash, int >;
using HashUIntMap = map< hash, uint >;
using IntStrMap = map< int, string >;
using StrIntMap = map< string, int >;

using UIntStrMulMap = multimap< uint, string >;

using PtrVec = vector< void* >;
using IntVec = vector< int >;
using UCharVec = vector< uchar >;
using UCharVecVec = vector< UCharVec >;
using ShortVec = vector< short >;
using UShortVec = vector< ushort >;
using UIntVec = vector< uint >;
using UIntVecVec = vector< UIntVec >;
using CharVec = vector< char >;
using StrVec = vector< string >;
using PCharVec = vector< char* >;
using PUCharVec = vector< uchar* >;
using FloatVec = vector< float >;
using UInt64Vec = vector< uint64 >;
using BoolVec = vector< bool >;
using SizeVec = vector< size_t >;
using HashVec = vector< hash >;
using HashVecVec = vector< HashVec >;
using MaxTVec = vector< max_t >;
using PStrMapVec = vector< StrMap* >;

using StrSet = set< string >;
using UCharSet = set< uchar >;
using UShortSet = set< ushort >;
using UIntSet = set< uint >;
using IntSet = set< int >;
using HashSet = set< hash >;

using IntPair = pair< int, int >;
using UShortPair = pair< ushort, ushort >;
using UIntPair = pair< uint, uint >;
using CharPair = pair< char, char >;
using PCharPair = pair< char*, char* >;
using UCharPair = pair< uchar, uchar >;

using UShortPairVec = vector< UShortPair >;
using IntPairVec = vector< IntPair >;
using UIntPairVec = vector< UIntPair >;
using PCharPairVec = vector< PCharPair >;
using UCharPairVec = vector< UCharPair >;

using UIntUIntPairMap = map< uint, UIntPair >;
using UIntIntPairVecMap = map< uint, IntPairVec >;
using UIntHashVecMap = map< uint, HashVec >;

#endif // ___TYPES___
