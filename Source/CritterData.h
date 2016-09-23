#ifndef __CRITTER_DATA__
#define __CRITTER_DATA__

#include "Common.h"
#ifdef FONLINE_MAPPER
# include "CritterCl.h"
#endif

class ProtoCritter: public ProtoEntity
{
public:
    ProtoCritter( hash pid );

    CLASS_PROPERTY_ALIAS( uint, Multihex );

    UIntVec  TextsLang;
    FOMsgVec Texts;

    #ifdef FONLINE_MAPPER
    string CollectionName;
    #endif
};
typedef map< hash, ProtoCritter* > ProtoCritterMap;
typedef vector< ProtoCritter* >    ProtoCritterVec;

#endif // __CRITTER_DATA__
