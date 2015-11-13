#ifndef __CRITTER_DATA__
#define __CRITTER_DATA__

#include "Item.h"
#include "FileManager.h"
#include "AI.h"
#include "Defines.h"
#ifdef FONLINE_MAPPER
# include "CritterCl.h"
#endif

struct ProtoCritter: public Entity
{
    ProtoCritter();

    hash     ProtoId;
    UIntVec  TextsLang;
    FOMsgVec Texts;

    #ifdef FONLINE_MAPPER
    string   CollectionName;
    #endif

    const char* GetName() { return Str::GetName( ProtoId ); }

    #ifdef FONLINE_MAPPER
    uint GetCrType()
    {
        return ( uint ) Properties::GetValueAsInt( this, CritterCl::PropertyBaseCrType->GetEnumValue() );
    }
    #endif
};
typedef map< hash, ProtoCritter* > ProtoCritterMap;
typedef vector< ProtoCritter* >    ProtoCritterVec;

#endif // __CRITTER_DATA__
