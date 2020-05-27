#ifndef __API_ANGELSCRIPT__
#define __API_ANGELSCRIPT__

#include "API_Common.h"

#ifndef __API_IMPL__
typedef unsigned long	 asDWORD;
struct asSFuncPtr;
struct ScriptString;
typedef unsigned char asEBehaviours;
#endif //__API_IMPL__

EXPORT int Script_RegisterObjectType(const char *obj, int byteSize, asDWORD flags);
EXPORT int Script_RegisterObjectProperty(const char *obj, const char *declaration, int byteOffset);
EXPORT int Script_RegisterObjectMethod(const char *obj, const char *declaration, const asSFuncPtr &funcPointer, asDWORD callConv);
EXPORT int Script_RegisterObjectBehaviour(const char *obj, asEBehaviours behaviour, const char *declaration, const asSFuncPtr &funcPointer, asDWORD callConv);

EXPORT ScriptString* Script_String(const char *str);

#endif // __API_ANGELSCRIPT__