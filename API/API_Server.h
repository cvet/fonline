#ifndef __API_SERVER__
#define __API_SERVER__

#include "API_Common.h"

#ifndef __API_IMPL__
struct Critter;
struct Item;
struct ScriptString;
#endif //__API_IMPL__

EXPORT bool Cl_RunClientScript( Critter* cl, const char* func_name, int p0, int p1, int p2, const char* p3, const uint* p4, size_t p4_size );
EXPORT Critter* Global_GetCritter(uint crid);
EXPORT ScriptString* Item_GetLexems(Item* item);

#endif // __API_SERVER__