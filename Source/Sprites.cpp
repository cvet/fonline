#include "Common.h"
#include "Sprites.h"
#include "SpriteManager.h"

void Sprite::Unvalidate()
{
    if( Valid )
    {
        if( ValidCallback )
        {
            *ValidCallback = false;
            ValidCallback = NULL;
        }
        Valid = false;

        if( Parent )
        {
            Parent->Child = NULL;
            Parent->Unvalidate();
        }
        if( Child )
        {
            Child->Parent = NULL;
            Child->Unvalidate();
        }
    }
}

Sprite* Sprite::GetIntersected( int ox, int oy )
{
    // Check for cutting
    if( ox < 0 || oy < 0 )
        return NULL;
    if( !CutType )
        return SprMngr.IsPixNoTransp( PSprId ? *PSprId : SprId, ox, oy ) ? this : NULL;

    // Find root sprite
    Sprite* spr = this;
    while( spr->Parent )
        spr = spr->Parent;

    // Check sprites
    float oxf = (float) ox * GameOpt.SpritesZoom;
    while( spr )
    {
        if( oxf >= spr->CutX && oxf < spr->CutX + spr->CutW )
            return SprMngr.IsPixNoTransp( spr->PSprId ? *spr->PSprId : spr->SprId, ox, oy ) ? spr : NULL;
        spr = spr->Child;
    }
    return NULL;
}

#define SPRITE_SETTER( func, type, val )                                                           \
    void Sprite::func( type val ## __ ) { if( !Valid )                                             \
                                              return; Valid = false; val = val ## __; if( Parent ) \
                                              Parent->func( val ## __ ); if( Child )               \
                                              Child->func( val ## __ ); Valid = true; }
#define SPRITE_SETTER2( func, type, val, type2, val2 )                                                                                  \
    void Sprite::func( type val ## __, type2 val2 ## __ ) { if( !Valid )                                                                \
                                                                return; Valid = false; val = val ## __; val2 = val2 ## __; if( Parent ) \
                                                                Parent->func( val ## __, val2 ## __ ); if( Child )                      \
                                                                Child->func( val ## __, val2 ## __ ); Valid = true; }
SPRITE_SETTER( SetEgg, int, EggType );
SPRITE_SETTER( SetContour, int, ContourType );
SPRITE_SETTER2( SetContour, int, ContourType, uint, ContourColor );
SPRITE_SETTER( SetColor, uint, Color );
SPRITE_SETTER( SetAlpha, uchar *, Alpha );
SPRITE_SETTER( SetFlash, uint, FlashMask );

void Sprite::SetLight( uchar* light, int maxhx, int maxhy )
{
    if( !Valid )
        return;
    Valid = false;

    if( HexX >= 0 && HexX < maxhx && HexY >= 0 && HexY < maxhy )
        Light = &light[ HexY * maxhx * 3 + HexX * 3 ];
    else
        Light = NULL;

    if( Parent )
        Parent->SetLight( light, maxhx, maxhy );
    if( Child )
        Child->SetLight( light, maxhx, maxhy );
    Valid = true;
}

SpriteVec Sprites::spritesPool;
void Sprites::GrowPool( uint size )
{
    spritesPool.reserve( spritesPool.size() + size );
    for( uint i = 0; i < size; i++ )
        spritesPool.push_back( new Sprite() );
}

void Sprites::ClearPool()
{
    for( auto it = spritesPool.begin(), end = spritesPool.end(); it != end; ++it )
    {
        Sprite* spr = *it;
        spr->Unvalidate();
        delete spr;
    }
    spritesPool.clear();
}

Sprite& Sprites::PutSprite( uint index, int draw_order, int hx, int hy, int cut, int x, int y, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, Effect** effect, bool* callback )
{
    if( index >= spritesTreeSize )
    {
        spritesTreeSize = index + 1;
        if( spritesTreeSize >= spritesTree.size() )
            Resize( spritesTreeSize + SPRITES_RESIZE_COUNT );
    }
    Sprite* spr = spritesTree[ index ];
    spr->TreeIndex = index;
    spr->HexX = hx;
    spr->HexY = hy;
    spr->CutType = 0;
    spr->ScrX = x;
    spr->ScrY = y;
    spr->SprId = id;
    spr->PSprId = id_ptr;
    spr->OffsX = ox;
    spr->OffsY = oy;
    spr->Alpha = alpha;
    spr->Light = NULL;
    spr->Valid = true;
    spr->ValidCallback = callback;
    if( callback )
        *callback = true;
    spr->EggType = 0;
    spr->ContourType = 0;
    spr->ContourColor = 0;
    spr->Color = 0;
    spr->FlashMask = 0;
    spr->DrawEffect = effect;
    spr->Parent = NULL;
    spr->Child = NULL;

    // Cutting
    if( cut == SPRITE_CUT_HORIZONTAL || cut == SPRITE_CUT_VERTICAL )
    {
        bool hor = ( cut == SPRITE_CUT_HORIZONTAL );

        int  stepi = GameOpt.MapHexWidth / 2;
        if( GameOpt.MapHexagonal && hor )
            stepi = ( GameOpt.MapHexWidth + GameOpt.MapHexWidth / 2 ) / 2;
        float       stepf = (float) stepi;

        SpriteInfo* si = SprMngr.GetSpriteInfo( id_ptr ? *id_ptr : id );
        if( !si || si->Width < stepi * 2 )
            return *spr;

        spr->CutType = cut;

        int h1, h2;
        if( hor )
        {
            h1 = spr->HexX + si->Width / 2 / stepi;
            h2 = spr->HexX - si->Width / 2 / stepi - ( si->Width / 2 % stepi ? 1 : 0 );
            spr->HexX = h1;
        }
        else
        {
            h1 = spr->HexY - si->Width / 2 / stepi;
            h2 = spr->HexY + si->Width / 2 / stepi + ( si->Width / 2 % stepi ? 1 : 0 );
            spr->HexY = h1;
        }

        float   widthf = (float) si->Width;
        float   xx = 0.0f;
        Sprite* parent = spr;
        for( int i = h1; ;)
        {
            float ww = stepf;
            if( xx + ww > widthf )
                ww = widthf - xx;

            Sprite& spr_ = ( i != h1 ? PutSprite( spritesTreeSize, draw_order, hor ? i : hx, hor ? hy : i, 0, x, y, id, id_ptr, ox, oy, alpha, effect, NULL ) : *spr );
            if( i != h1 )
                spr_.Parent = parent;
            parent->Child = &spr_;
            parent = &spr_;

            spr_.CutX = xx;
            spr_.CutW = ww;
            spr_.CutTexL = si->SprRect.L + ( si->SprRect.R - si->SprRect.L ) * ( xx / widthf );
            spr_.CutTexR = si->SprRect.L + ( si->SprRect.R - si->SprRect.L ) * ( ( xx + ww ) / widthf );
            spr_.CutType = cut;

            #ifdef FONLINE_MAPPER
            spr_.CutOyL = ( hor ? -6 : -12 ) * ( ( hor ? hx : hy ) - i );
            spr_.CutOyR = spr_.CutOyL;
            if( ww < stepf )
                spr_.CutOyR += (int) ( ( hor ? 3.6f : -8.0f ) * ( 1.0f - ( ww / stepf ) ) );
            #endif

            xx += stepf;
            if( xx > widthf )
                break;

            if( ( hor && --i < h2 ) || ( !hor && ++i > h2 ) )
                break;
        }
    }

    // Draw order
    spr->DrawOrderType = draw_order;
    spr->DrawOrderPos = ( draw_order >= DRAW_ORDER_FLAT && draw_order < DRAW_ORDER ?
                          spr->HexY * MAXHEX_MAX + spr->HexX + MAXHEX_MAX * MAXHEX_MAX * ( draw_order - DRAW_ORDER_FLAT ) :
                          MAXHEX_MAX * MAXHEX_MAX * DRAW_ORDER + spr->HexY * DRAW_ORDER * MAXHEX_MAX + spr->HexX * DRAW_ORDER + ( draw_order - DRAW_ORDER ) );

    return *spr;
}

Sprite& Sprites::AddSprite( int draw_order, int hx, int hy, int cut, int x, int y, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, Effect** effect, bool* callback )
{
    return PutSprite( spritesTreeSize, draw_order, hx, hy, cut, x, y, id, id_ptr, ox, oy, alpha, effect, callback );
}

Sprite& Sprites::InsertSprite( int draw_order, int hx, int hy, int cut, int x, int y, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, Effect** effect, bool* callback )
{
    // For cutted sprites need resort all tree
    if( cut == SPRITE_CUT_HORIZONTAL || cut == SPRITE_CUT_VERTICAL )
    {
        Sprite& spr = PutSprite( spritesTreeSize, draw_order, hx, hy, cut, x, y, id, id_ptr, ox, oy, alpha, effect, callback );
        SortByMapPos();
        return spr;
    }

    // Find place
    uint index = 0;
    uint pos = ( draw_order >= DRAW_ORDER_FLAT && draw_order < DRAW_ORDER ?
                 hy * MAXHEX_MAX + hx + MAXHEX_MAX * MAXHEX_MAX * ( draw_order - DRAW_ORDER_FLAT ) :
                 MAXHEX_MAX * MAXHEX_MAX * DRAW_ORDER + hy * DRAW_ORDER * MAXHEX_MAX + hx * DRAW_ORDER + ( draw_order - DRAW_ORDER ) );
    for( ; index < spritesTreeSize; index++ )
    {
        Sprite* spr = spritesTree[ index ];
        if( !spr->Valid )
            continue;
        if( pos < spr->DrawOrderPos )
            break;
    }

    // Gain tree index to other sprites
    for( uint i = index; i < spritesTreeSize; i++ )
        spritesTree[ i ]->TreeIndex++;

    // Insert to array
    spritesTreeSize++;
    if( spritesTreeSize >= spritesTree.size() )
        Resize( spritesTreeSize + SPRITES_RESIZE_COUNT );
    if( index < spritesTreeSize - 1 )
    {
        Sprite* back = spritesTree.back();
        spritesTree.insert( spritesTree.begin() + index, back );
        spritesTree.pop_back();
    }

    return PutSprite( index, draw_order, hx, hy, cut, x, y, id, id_ptr, ox, oy, alpha, effect, callback );
}

void Sprites::Resize( uint size )
{
    uint tree_size = (uint) spritesTree.size();
    uint pool_size = (uint) spritesPool.size();
    if( size > tree_size ) // Get from pool
    {
        uint diff = size - tree_size;
        if( diff > pool_size )
            GrowPool( diff > SPRITES_POOL_GROW_SIZE ? diff : SPRITES_POOL_GROW_SIZE );
        spritesTree.reserve( tree_size + diff );
        // spritesTree.insert(spritesTree.end(),spritesPool.rbegin(),spritesPool.rbegin()+diff);
        // spritesPool.erase(spritesPool.begin()+tree_size-diff,spritesPool.end());
        for( uint i = 0; i < diff; i++ )
        {
            Sprite* back = spritesPool.back();
            spritesTree.push_back( back );
            spritesPool.pop_back();
        }
    }
    else if( size < tree_size ) // Put in pool
    {
        uint diff = tree_size - size;
        if( diff > tree_size - spritesTreeSize )
            spritesTreeSize -= diff - ( tree_size - spritesTreeSize );

        // Unvalidate putted sprites
        for( SpriteVec::reverse_iterator it = spritesTree.rbegin(), end = spritesTree.rbegin() + diff; it != end; ++it )
            ( *it )->Unvalidate();

        // Put
        spritesPool.reserve( pool_size + diff );
        // spritesPool.insert(spritesPool.end(),spritesTree.rbegin(),spritesTree.rbegin()+diff);
        // spritesTree.erase(spritesTree.begin()+tree_size-diff,spritesTree.end());
        for( uint i = 0; i < diff; i++ )
        {
            Sprite* back = spritesTree.back();
            spritesPool.push_back( back );
            spritesTree.pop_back();
        }
    }
}

void Sprites::Unvalidate()
{
    for( SpriteVec::iterator it = spritesTree.begin(), end = spritesTree.begin() + spritesTreeSize; it != end; ++it )
        ( *it )->Unvalidate();
    spritesTreeSize = 0;
}

SprInfoVec* SortSpritesSurfSprData = NULL;
void Sprites::SortBySurfaces()
{
    struct Sorter
    {
        static bool SortBySurfaces( Sprite* spr1, Sprite* spr2 )
        {
            SpriteInfo* si1 = ( *SortSpritesSurfSprData )[ spr1->PSprId ? *spr1->PSprId : spr1->SprId ];
            SpriteInfo* si2 = ( *SortSpritesSurfSprData )[ spr2->PSprId ? *spr2->PSprId : spr2->SprId ];
            return si1 && si2 && si1->Atlas && si2->Atlas && si1->Atlas->TextureOwner < si2->Atlas->TextureOwner;
        }
    };
    SortSpritesSurfSprData = &SprMngr.GetSpritesInfo();
    std::sort( spritesTree.begin(), spritesTree.begin() + spritesTreeSize, Sorter::SortBySurfaces );
}

void Sprites::SortByMapPos()
{
    struct Sorter
    {
        static bool SortByMapPos( Sprite* spr1, Sprite* spr2 )
        {
            if( spr1->DrawOrderPos == spr2->DrawOrderPos ) return spr1->TreeIndex < spr2->TreeIndex;
            return spr1->DrawOrderPos < spr2->DrawOrderPos;
        }
    };
    std::sort( spritesTree.begin(), spritesTree.begin() + spritesTreeSize, Sorter::SortByMapPos );
    for( uint i = 0; i < spritesTreeSize; i++ )
        spritesTree[ i ]->TreeIndex = i;
}
