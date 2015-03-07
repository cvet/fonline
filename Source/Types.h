#ifndef ___TYPES___
#define ___TYPES___

#include "PlatformSpecific.h"
#include <map>
#include <string>
#include <set>
#include <list>
#include <vector>
#include <deque>
#include <strstream>

#if defined ( FO_MSVC )
typedef unsigned char    uchar;
typedef unsigned short   ushort;
typedef unsigned int     uint;
typedef unsigned __int64 uint64;
typedef __int64          int64;
#elif defined ( FO_GCC )
# include <inttypes.h>
typedef unsigned char    uchar;
typedef unsigned short   ushort;
typedef unsigned int     uint;
typedef uint64_t         uint64;
typedef int64_t          int64;
#endif

typedef uint             hash;
typedef uint             max_t;

using std::string;
using std::list;
using std::vector;
using std::map;
using std::multimap;
using std::set;
using std::deque;
using std::pair;
using std::istrstream;

#define PAIR( k, v )    pair< decltype( k ), decltype( v ) >( k, v )

typedef map< string, uchar >     StrUCharMap;
typedef map< uchar, string >     UCharStrMap;
typedef map< string, string >    StrMap;
typedef map< uint, string >      UIntStrMap;
typedef map< string, ushort >    StrUShortMap;
typedef map< string, uint >      StrUIntMap;
typedef map< string, void* >     StrPtrMap;
typedef map< ushort, string >    UShortStrMap;
typedef map< string, uint >      StrUIntMap;
typedef map< uint, uint >        UIntMap;
typedef map< int, int >          IntMap;
typedef map< int, float >        IntFloatMap;
typedef map< int, void* >        IntPtrMap;
typedef map< uint, float >       UIntFloatMap;
typedef map< ushort, uint >      UShortUIntMap;
typedef map< size_t, string >    SizeTStrMap;
typedef map< uint, int >         UIntIntMap;
typedef map< hash, int >         HashIntMap;
typedef map< hash, uint >        HashUIntMap;

typedef multimap< uint, string > UIntStrMulMap;

typedef vector< void* >          PtrVec;
typedef vector< int >            IntVec;
typedef vector< uchar >          UCharVec;
typedef vector< short >          ShortVec;
typedef vector< ushort >         UShortVec;
typedef vector< uint >           UIntVec;
typedef vector< UIntVec >        UIntVecVec;
typedef vector< char >           CharVec;
typedef vector< string >         StrVec;
typedef vector< char* >          PCharVec;
typedef vector< uchar* >         PUCharVec;
typedef vector< float >          FloatVec;
typedef vector< uint64 >         UInt64Vec;
typedef vector< bool >           BoolVec;
typedef vector< size_t >         SizeVec;
typedef vector< hash >           HashVec;
typedef vector< max_t >          MaxTVec;

typedef set< string >            StrSet;
typedef set< uchar >             UCharSet;
typedef set< ushort >            UShortSet;
typedef set< uint >              UIntSet;
typedef set< int >               IntSet;
typedef set< hash >              HashSet;

typedef pair< int, int >         IntPair;
typedef pair< ushort, ushort >   UShortPair;
typedef pair< uint, uint >       UIntPair;
typedef pair< char, char >       CharPair;
typedef pair< char*, char* >     PCharPair;
typedef pair< uchar, uchar >     UCharPair;

typedef vector< UShortPair >     UShortPairVec;
typedef vector< IntPair >        IntPairVec;
typedef vector< UIntPair >       UIntPairVec;
typedef vector< PCharPair >      PCharPairVec;
typedef vector< UCharPair >      UCharPairVec;

typedef map< uint, UIntPair >    UIntUIntPairMap;

#endif // ___TYPES___
