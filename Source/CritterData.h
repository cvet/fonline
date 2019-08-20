#ifndef __CRITTER_DATA__
#define __CRITTER_DATA__

#include "Common.h"

class ProtoCritter: public ProtoEntity
{
public:
    ProtoCritter( hash pid );

    CLASS_PROPERTY_ALIAS( uint, Multihex );

    UIntVec  TextsLang;
    FOMsgVec Texts;

    #ifdef FONLINE_EDITOR
    string CollectionName;
    #endif
};
typedef map< hash, ProtoCritter* > ProtoCritterMap;
typedef vector< ProtoCritter* >    ProtoCritterVec;

#endif // __CRITTER_DATA__
