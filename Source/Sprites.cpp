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
            ValidCallback = nullptr;
        }
        Valid = false;

        if( Parent )
        {
            Parent->Child = nullptr;
            Parent->Unvalidate();
        }
        if( Child )
        {
            Child->Parent = nullptr;
            Child->Unvalidate();
        }

        if( ExtraChainRoot )
            *ExtraChainRoot = ExtraChainChild;
        if( ExtraChainParent )
            ExtraChainParent->ExtraChainChild = ExtraChainChild;
        if( ExtraChainChild )
            ExtraChainChild->ExtraChainParent = ExtraChainParent;
        if( ExtraChainRoot && ExtraChainChild )
            ExtraChainChild->ExtraChainRoot = ExtraChainRoot;
        ExtraChainRoot = nullptr;
        ExtraChainParent = nullptr;
        ExtraChainChild = nullptr;

        if( MapSpr )
        {
            MapSpr->Release();
            MapSpr = nullptr;
        }

        UnvalidatedPlace->push_back( this );
        UnvalidatedPlace = nullptr;

        if( ChainRoot )
            *ChainRoot = ChainChild;
        if( ChainLast )
            *ChainLast = ChainParent;
        if( ChainParent )
            ChainParent->ChainChild = ChainChild;
        if( ChainChild )
            ChainChild->ChainParent = ChainParent;
        if( ChainRoot && ChainChild )
            ChainChild->ChainRoot = ChainRoot;
        if( ChainLast && ChainParent )
            ChainParent->ChainLast = ChainLast;
        ChainRoot = nullptr;
        ChainLast = nullptr;
        ChainParent = nullptr;
        ChainChild = nullptr;
    }
}

Sprite* Sprite::GetIntersected( int ox, int oy )
{
    // Check for cutting
    if( ox < 0 || oy < 0 )
        return nullptr;
    if( !CutType )
        return SprMngr.IsPixNoTransp( PSprId ? *PSprId : SprId, ox, oy ) ? this : nullptr;

    // Find root sprite
    Sprite* spr = this;
    while( spr->Parent )
        spr = spr->Parent;

    // Check sprites
    float oxf = (float) ox * GameOpt.SpritesZoom;
    while( spr )
    {
        if( oxf >= spr->CutX && oxf < spr->CutX + spr->CutW )
            return SprMngr.IsPixNoTransp( spr->PSprId ? *spr->PSprId : spr->SprId, ox, oy ) ? spr : nullptr;
        spr = spr->Child;
    }
    return nullptr;
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

void Sprite::SetLight( int corner, uchar* light, int maxhx, int maxhy )
{
    if( !Valid )
        return;
    Valid = false;

    if( HexX >= 1 && HexX < maxhx - 1 && HexY >= 1 && HexY < maxhy - 1 )
    {
        Light = &light[ HexY * maxhx * 3 + HexX * 3 ];

        switch( corner )
        {
        default:
        case CORNER_EAST_WEST:
        case CORNER_EAST:
            LightRight = Light - 3;
            LightLeft = Light + 3;
            break;
        case CORNER_NORTH_SOUTH:
        case CORNER_WEST:
            LightRight = Light + ( maxhx * 3 );
            LightLeft = Light - ( maxhx * 3 );
            break;
        case CORNER_SOUTH:
            LightRight = Light - 3;
            LightLeft = Light - ( maxhx * 3 );
            break;
        case CORNER_NORTH:
            LightRight = Light + ( maxhx * 3 );
            LightLeft = Light + 3;
            break;
        }
    }
    else
    {
        Light = nullptr;
        LightRight = nullptr;
        LightLeft = nullptr;
    }

    if( Parent )
        Parent->SetLight( corner, light, maxhx, maxhy );
    if( Child )
        Child->SetLight( corner, light, maxhx, maxhy );
    Valid = true;
}

void Sprite::SetFixedAlpha( uchar alpha )
{
    if( !Valid )
        return;
    Valid = false;

    Alpha = ( (uchar*) &Color ) + 3;
    *Alpha = alpha;

    if( Parent )
        Parent->SetFixedAlpha( alpha );
    if( Child )
        Child->SetFixedAlpha( alpha );
    Valid = true;
}

SpriteVec Sprites::spritesPool;

void Sprites::GrowPool()
{
    spritesPool.reserve( spritesPool.size() + SPRITES_POOL_GROW_SIZE );
    for( uint i = 0; i < SPRITES_POOL_GROW_SIZE; i++ )
        spritesPool.push_back( new Sprite() );
}

Sprites::Sprites()
{
    rootSprite = nullptr;
    lastSprite = nullptr;
    spriteCount = 0;
}

Sprites::~Sprites()
{
    Clear();
}

Sprite* Sprites::RootSprite()
{
    return rootSprite;
}

Sprite& Sprites::PutSprite( Sprite* child, int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, Effect** effect, bool* callback )
{
    spriteCount++;

    Sprite* spr;
    if( !unvalidatedSprites.empty() )
    {
        spr = unvalidatedSprites.back();
        unvalidatedSprites.pop_back();
    }
    else
    {
        if( spritesPool.empty() )
            GrowPool();

        spr = spritesPool.back();
        spritesPool.pop_back();
    }

    spr->UnvalidatedPlace = &unvalidatedSprites;

    if( !child )
    {
        if( !lastSprite )
        {
            rootSprite = spr;
            lastSprite = spr;
            spr->ChainRoot = &rootSprite;
            spr->ChainLast = &lastSprite;
            spr->ChainParent = nullptr;
            spr->ChainChild = nullptr;
            spr->TreeIndex = 0;
        }
        else
        {
            spr->ChainParent = lastSprite;
            spr->ChainChild = nullptr;
            lastSprite->ChainChild = spr;
            lastSprite->ChainLast = nullptr;
            spr->ChainLast = &lastSprite;
            spr->TreeIndex = lastSprite->TreeIndex + 1;
            lastSprite = spr;
        }
    }
    else
    {
        spr->ChainChild = child;
        spr->ChainParent = child->ChainParent;
        child->ChainParent = spr;
        if( spr->ChainParent )
            spr->ChainParent->ChainChild = spr;

        // Recalculate indices
        uint    index = ( spr->ChainParent ? spr->ChainParent->TreeIndex + 1 : 0 );
        Sprite* spr_ = spr;
        while( spr_ )
        {
            spr_->TreeIndex = index;
            spr_ = spr_->ChainChild;
            index++;
        }

        if( !spr->ChainParent )
        {
            RUNTIME_ASSERT( child->ChainRoot );
            rootSprite = spr;
            spr->ChainRoot = &rootSprite;
            child->ChainRoot = nullptr;
        }
    }

    spr->HexX = hx;
    spr->HexY = hy;
    spr->CutType = 0;
    spr->ScrX = x;
    spr->ScrY = y;
    spr->PScrX = sx;
    spr->PScrY = sy;
    spr->SprId = id;
    spr->PSprId = id_ptr;
    spr->OffsX = ox;
    spr->OffsY = oy;
    spr->Alpha = alpha;
    spr->Light = nullptr;
    spr->LightRight = nullptr;
    spr->LightLeft = nullptr;
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
    spr->Parent = nullptr;
    spr->Child = nullptr;

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

            Sprite& spr_ = ( i != h1 ? PutSprite( nullptr, draw_order, hor ? i : hx, hor ? hy : i, 0, x, y, sx, sy, id, id_ptr, ox, oy, alpha, effect, nullptr ) : *spr );
            if( i != h1 )
                spr_.Parent = parent;
            parent->Child = &spr_;
            parent = &spr_;

            spr_.CutX = xx;
            spr_.CutW = ww;
            spr_.CutTexL = si->SprRect.L + ( si->SprRect.R - si->SprRect.L ) * ( xx / widthf );
            spr_.CutTexR = si->SprRect.L + ( si->SprRect.R - si->SprRect.L ) * ( ( xx + ww ) / widthf );
            spr_.CutType = cut;

            #ifdef FONLINE_EDITOR
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

Sprite& Sprites::AddSprite( int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, Effect** effect, bool* callback )
{
    return PutSprite( nullptr, draw_order, hx, hy, cut, x, y, sx, sy, id, id_ptr, ox, oy, alpha, effect, callback );
}

Sprite& Sprites::InsertSprite( int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, Effect** effect, bool* callback )
{
    // For cutted sprites need resort all tree
    if( cut == SPRITE_CUT_HORIZONTAL || cut == SPRITE_CUT_VERTICAL )
    {
        Sprite& spr = PutSprite( nullptr, draw_order, hx, hy, cut, x, y, sx, sy, id, id_ptr, ox, oy, alpha, effect, callback );
        SortByMapPos();
        return spr;
    }

    // Find place
    uint index = 0;
    uint pos = ( draw_order >= DRAW_ORDER_FLAT && draw_order < DRAW_ORDER ?
                 hy * MAXHEX_MAX + hx + MAXHEX_MAX * MAXHEX_MAX * ( draw_order - DRAW_ORDER_FLAT ) :
                 MAXHEX_MAX * MAXHEX_MAX * DRAW_ORDER + hy * DRAW_ORDER * MAXHEX_MAX + hx * DRAW_ORDER + ( draw_order - DRAW_ORDER ) );

    Sprite* parent = rootSprite;
    while( parent )
    {
        if( !parent->Valid )
            continue;
        if( pos < parent->DrawOrderPos )
            break;
        parent = parent->ChainChild;
    }

    return PutSprite( parent, draw_order, hx, hy, cut, x, y, sx, sy, id, id_ptr, ox, oy, alpha, effect, callback );
}

void Sprites::Unvalidate()
{
    while( rootSprite )
        rootSprite->Unvalidate();
    spriteCount = 0;
}

SprInfoVec* SortSpritesSurfSprData = nullptr;
void Sprites::SortByMapPos()
{
    if( !rootSprite )
        return;

    struct Sorter
    {
        static bool SortBySurfaces( Sprite* spr1, Sprite* spr2 )
        {
            SpriteInfo* si1 = ( *SortSpritesSurfSprData )[ spr1->PSprId ? *spr1->PSprId : spr1->SprId ];
            SpriteInfo* si2 = ( *SortSpritesSurfSprData )[ spr2->PSprId ? *spr2->PSprId : spr2->SprId ];
            return si1 && si2 && si1->Atlas && si2->Atlas && si1->Atlas->TextureOwner < si2->Atlas->TextureOwner;
        }

        static bool SortByMapPos( Sprite* spr1, Sprite* spr2 )
        {
            if( spr1->DrawOrderPos == spr2->DrawOrderPos )
                return spr1->TreeIndex < spr2->TreeIndex;
            return spr1->DrawOrderPos < spr2->DrawOrderPos;
        }
    };
    SortSpritesSurfSprData = &SprMngr.GetSpritesInfo();

    SpriteVec sprites;
    sprites.reserve( spriteCount );
    Sprite*   spr = rootSprite;
    while( spr )
    {
        sprites.push_back( spr );
        spr = spr->ChainChild;
    }

    std::sort( sprites.begin(), sprites.end(), Sorter::SortBySurfaces );
    std::sort( sprites.begin(), sprites.end(), Sorter::SortByMapPos );

    for( size_t i = 0; i < sprites.size(); i++ )
    {
        sprites[ i ]->ChainParent = nullptr;
        sprites[ i ]->ChainChild = nullptr;
        sprites[ i ]->ChainRoot = nullptr;
        sprites[ i ]->ChainLast = nullptr;
    }

    for( size_t i = 1; i < sprites.size(); i++ )
    {
        sprites[ i - 1 ]->ChainChild = sprites[ i ];
        sprites[ i ]->ChainParent = sprites[ i - 1 ];
    }

    rootSprite = sprites.front();
    lastSprite = sprites.back();
    rootSprite->ChainRoot = &rootSprite;
    lastSprite->ChainLast = &lastSprite;
}

uint Sprites::Size()
{
    return spriteCount;
}

void Sprites::Clear()
{
    Unvalidate();

    for( Sprite* spr : unvalidatedSprites )
        spritesPool.push_back( spr );
    unvalidatedSprites.clear();
}
