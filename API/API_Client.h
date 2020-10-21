#ifndef __API_CLIENT__
#define __API_CLIENT__

#include "API_Common.h"

#ifndef __API_IMPL__
struct Sprite;
struct Sprites;
struct AnyFrames;
struct Field;
struct ScriptArray;
#endif //__API_IMPL__

EXPORT void Net_SendRunScript( bool unsafe, const char* func_name, int p0, int p1, int p2, const char* p3, const uint* p4, size_t p4_size );
EXPORT uint Client_AnimGetCurSpr(uint anim_id);
EXPORT Sprites* HexMngr_GetDrawTree();
EXPORT Sprite* Sprites_InsertSprite(Sprites* sprites, int draw_order, int hx, int hy, int cut, int x, int y, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, bool* callback );
EXPORT void Field_ChangeTile(Field* field, AnyFrames* anim, short ox, short oy, uchar layer, bool is_roof );
EXPORT AnyFrames* ResMngr_GetAnim(uint name_hash, int dir, int res_type, bool filter_nearest);
EXPORT size_t HexMngr_GetAllItems_ScriptArray(ScriptArray* items);
EXPORT bool HexMngr_GetScreenHexes(int& sx, int& sy);
EXPORT bool HexMngr_GetHexCurrentPosition(ushort hx, ushort hy, int& x, int& y);

#endif // __API_CLIENT__
