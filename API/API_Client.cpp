#include "StdAfx.h"
#include "Client.h"

#define __API_IMPL__
#include "API_Client.h"

void Net_SendRunScript( bool unsafe, const char* func_name, int p0, int p1, int p2, const char* p3, const uint* p4, size_t p4_size )
{
	UIntVec dw;
	if( p4 && p4_size)
		dw.assign(p4, p4 + p4_size);
	FOClient::Self->Net_SendRunScript( unsafe, func_name, p0, p1, p2, p3, dw );
}

uint Client_AnimGetCurSpr(uint anim_id)
{
	return FOClient::Self->AnimGetCurSpr(anim_id);
}

Sprites* HexMngr_GetDrawTree()
{
	return &FOClient::Self->HexMngr.GetDrawTree();
}

Sprite* Sprites_InsertSprite(Sprites* sprites, int draw_order, int hx, int hy, int cut, int x, int y, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, bool* callback )
{
	return &sprites->InsertSprite(draw_order, hx, hy, cut, x, y, id, id_ptr, ox, oy, alpha, callback);
}

void Field_ChangeTile(Field* field, AnyFrames* anim, short ox, short oy, uchar layer, bool is_roof )
{
	Field::TileVec* tiles;
	// select vector with requested tile type: either floor or roof
    if( is_roof )
        tiles = &field->Roofs;
    else
        tiles = &field->Tiles;

	Field::Tile* tile = NULL;

	//search if there is already tile with the same layer
	for( uint i = 0, j = (uint) tiles->size(); i < j; i++ )
    {
		if( (*tiles)[i].Layer == layer) {
			tile = &(*tiles)[i];
			break;
		}
	}
	//if there is no tile with same layer, push new
	if( tile == NULL ) {
		tiles->push_back(Field::Tile());
		tile = &tiles->back();
		//set layer as it's new tile
		tile->Layer = layer;
	}
	//fill rest of the values
    tile->Anim = anim;
    tile->OffsX = ox;
    tile->OffsY = oy;
}

AnyFrames* ResMngr_GetAnim(uint name_hash, int dir, int res_type, bool filter_nearest) {
	bool old_filter = SprMngr.SurfFilterNearest;
	SprMngr.SurfFilterNearest = filter_nearest;
    AnyFrames* anim = ResMngr.GetAnim( name_hash, dir, res_type);
    SprMngr.SurfFilterNearest = old_filter;
	return anim;
}

bool HexManager_ChangeTile( uint name_hash, ushort hx, ushort hy, short ox, short oy, uchar layer, bool is_roof)
{
	HexManager& self = FOClient::Self->HexMngr;
	if( hx >= self.GetMaxHexX() || hy >= self.GetMaxHexY() )
        return false;
    Field& f = self.GetField( hx, hy );

    SprMngr.SurfFilterNearest = true;
    AnyFrames* anim = ResMngr.GetItemAnim( name_hash );
    SprMngr.SurfFilterNearest = false;
    if( !anim )
        return false;

	Field_ChangeTile(&f, anim, ox, oy, layer, is_roof );
	
	return true;

    /*if( is_roof )
    {
        f.AddTile( anim, 0, 0, layer, true );
        ProtoMap::TileVec& roofs = CurProtoMap->GetTiles( hx, hy, true );
        roofs.push_back( ProtoMap::Tile( name_hash, hx, hy, (char) ox, (char) oy, layer, true ) );
        roofs.back().      IsSelected = select;
    }
    else
    {
        f.AddTile( anim, 0, 0, layer, false );
        ProtoMap::TileVec& tiles = CurProtoMap->GetTiles( hx, hy, false );
        tiles.push_back( ProtoMap::Tile( name_hash, hx, hy, (char) ox, (char) oy, layer, false ) );
        tiles.back().      IsSelected = select;
    }*/

    /*if( CheckTilesBorder( is_roof ? f.Roofs.back() : f.Tiles.back(), is_roof ) )
    {
        ResizeView();
        RefreshMap();
    }
    else
    {
        if( is_roof )
            RebuildRoof();
        else
            RebuildTiles();
    }*/
}

size_t HexMngr_GetAllItems_ScriptArray(ScriptArray* script_array)
{
	ItemPtrVec items;
	ItemHexVec& item_hexes = FOClient::Self->HexMngr.GetItems();
	for( auto it = item_hexes.begin(), end = item_hexes.end(); it != end; ++it )
    {
        ItemHex* item_hex = *it;
		if( item_hex && !item_hex->IsNotValid && item_hex->IsItem() )
		{
			Item* item = item_hex;
			items.push_back(item);
		}
    }
	Script::AppendVectorToArrayRef< Item* >( items, script_array );
	return items.size();
}

void HexMngr_GetScreenHexes(int& sx, int& sy)
{
	FOClient::Self->HexMngr.GetScreenHexes(sx, sy);
}

void HexMngr_GetHexCurrentPosition(ushort hx, ushort hy, int& x, int& y)
{
	FOClient::Self->HexMngr.GetHexCurrentPosition(hx, hy, x, y);
}
