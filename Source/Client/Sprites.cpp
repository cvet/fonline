#include "Sprites.h"
#include "Settings.h"
#include "SpriteManager.h"
#include "Testing.h"

SpriteVec Sprites::spritesPool;

Sprite::Sprite()
{
    memzero(this, sizeof(Sprite));
}

void Sprite::Unvalidate()
{
    if (!Valid)
        return;
    Valid = false;

    if (ValidCallback)
    {
        *ValidCallback = false;
        ValidCallback = nullptr;
    }

    if (Parent)
    {
        Parent->Child = nullptr;
        Parent->Unvalidate();
    }
    if (Child)
    {
        Child->Parent = nullptr;
        Child->Unvalidate();
    }

    if (ExtraChainRoot)
        *ExtraChainRoot = ExtraChainChild;
    if (ExtraChainParent)
        ExtraChainParent->ExtraChainChild = ExtraChainChild;
    if (ExtraChainChild)
        ExtraChainChild->ExtraChainParent = ExtraChainParent;
    if (ExtraChainRoot && ExtraChainChild)
        ExtraChainChild->ExtraChainRoot = ExtraChainRoot;
    ExtraChainRoot = nullptr;
    ExtraChainParent = nullptr;
    ExtraChainChild = nullptr;

    if (MapSpr)
    {
        MapSpr->Release();
        MapSpr = nullptr;
    }

    Root->unvalidatedSprites.push_back(this);

    if (ChainRoot)
        *ChainRoot = ChainChild;
    if (ChainLast)
        *ChainLast = ChainParent;
    if (ChainParent)
        ChainParent->ChainChild = ChainChild;
    if (ChainChild)
        ChainChild->ChainParent = ChainParent;
    if (ChainRoot && ChainChild)
        ChainChild->ChainRoot = ChainRoot;
    if (ChainLast && ChainParent)
        ChainParent->ChainLast = ChainLast;
    ChainRoot = nullptr;
    ChainLast = nullptr;
    ChainParent = nullptr;
    ChainChild = nullptr;

    Root = nullptr;
}

Sprite* Sprite::GetIntersected(int ox, int oy)
{
    // Check for cutting
    if (ox < 0 || oy < 0)
        return nullptr;
    if (!CutType)
        return Root->sprMngr.IsPixNoTransp(PSprId ? *PSprId : SprId, ox, oy) ? this : nullptr;

    // Find root sprite
    Sprite* spr = this;
    while (spr->Parent)
        spr = spr->Parent;

    // Check sprites
    float oxf = (float)ox * Root->settings.SpritesZoom;
    while (spr)
    {
        if (oxf >= spr->CutX && oxf < spr->CutX + spr->CutW)
            return Root->sprMngr.IsPixNoTransp(spr->PSprId ? *spr->PSprId : spr->SprId, ox, oy) ? spr : nullptr;
        spr = spr->Child;
    }
    return nullptr;
}

void Sprite::SetEgg(int egg)
{
    if (!Valid)
        return;

    Valid = false;
    EggType = egg;
    if (Parent)
        Parent->SetEgg(egg);
    if (Child)
        Child->SetEgg(egg);
    Valid = true;
}

void Sprite::SetContour(int contour)
{
    if (!Valid)
        return;

    Valid = false;
    ContourType = contour;
    if (Parent)
        Parent->SetContour(contour);
    if (Child)
        Child->SetContour(contour);
    Valid = true;
}

void Sprite::SetContour(int contour, uint color)
{
    if (!Valid)
        return;

    Valid = false;
    ContourType = contour;
    ContourColor = color;
    if (Parent)
        Parent->SetContour(contour, color);
    if (Child)
        Child->SetContour(contour, color);
    Valid = true;
}

void Sprite::SetColor(uint color)
{
    if (!Valid)
        return;

    Valid = false;
    Color = color;
    if (Parent)
        Parent->SetColor(color);
    if (Child)
        Child->SetColor(color);
    Valid = true;
}

void Sprite::SetAlpha(uchar* alpha)
{
    if (!Valid)
        return;

    Valid = false;
    Alpha = alpha;
    if (Parent)
        Parent->SetAlpha(alpha);
    if (Child)
        Child->SetAlpha(alpha);
    Valid = true;
}

void Sprite::SetFlash(uint mask)
{
    if (!Valid)
        return;

    Valid = false;
    FlashMask = mask;
    if (Parent)
        Parent->SetFlash(mask);
    if (Child)
        Child->SetFlash(mask);
    Valid = true;
}

void Sprite::SetLight(int corner, uchar* light, int maxhx, int maxhy)
{
    if (!Valid)
        return;

    Valid = false;

    if (HexX >= 1 && HexX < maxhx - 1 && HexY >= 1 && HexY < maxhy - 1)
    {
        Light = &light[HexY * maxhx * 3 + HexX * 3];

        switch (corner)
        {
        default:
        case CORNER_EAST_WEST:
        case CORNER_EAST:
            LightRight = Light - 3;
            LightLeft = Light + 3;
            break;
        case CORNER_NORTH_SOUTH:
        case CORNER_WEST:
            LightRight = Light + (maxhx * 3);
            LightLeft = Light - (maxhx * 3);
            break;
        case CORNER_SOUTH:
            LightRight = Light - 3;
            LightLeft = Light - (maxhx * 3);
            break;
        case CORNER_NORTH:
            LightRight = Light + (maxhx * 3);
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

    if (Parent)
        Parent->SetLight(corner, light, maxhx, maxhy);
    if (Child)
        Child->SetLight(corner, light, maxhx, maxhy);

    Valid = true;
}

void Sprite::SetFixedAlpha(uchar alpha)
{
    if (!Valid)
        return;

    Valid = false;

    Alpha = ((uchar*)&Color) + 3;
    *Alpha = alpha;

    if (Parent)
        Parent->SetFixedAlpha(alpha);
    if (Child)
        Child->SetFixedAlpha(alpha);

    Valid = true;
}

void Sprites::GrowPool()
{
    spritesPool.reserve(spritesPool.size() + SPRITES_POOL_GROW_SIZE);
    for (uint i = 0; i < SPRITES_POOL_GROW_SIZE; i++)
        spritesPool.push_back(new Sprite());
}

Sprites::Sprites(HexSettings& sett, SpriteManager& spr_mngr) :
    settings {sett}, sprMngr(spr_mngr), rootSprite {}, lastSprite {}, spriteCount {}, unvalidatedSprites {}
{
}

Sprites::~Sprites()
{
    Clear();
}

Sprite* Sprites::RootSprite()
{
    return rootSprite;
}

Sprite& Sprites::PutSprite(Sprite* child, int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy,
    uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, Effect** effect, bool* callback)
{
    spriteCount++;

    Sprite* spr;
    if (!unvalidatedSprites.empty())
    {
        spr = unvalidatedSprites.back();
        unvalidatedSprites.pop_back();
    }
    else
    {
        if (spritesPool.empty())
            GrowPool();

        spr = spritesPool.back();
        spritesPool.pop_back();
    }

    spr->Root = this;

    if (!child)
    {
        if (!lastSprite)
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
        if (spr->ChainParent)
            spr->ChainParent->ChainChild = spr;

        // Recalculate indices
        uint index = (spr->ChainParent ? spr->ChainParent->TreeIndex + 1 : 0);
        Sprite* spr_ = spr;
        while (spr_)
        {
            spr_->TreeIndex = index;
            spr_ = spr_->ChainChild;
            index++;
        }

        if (!spr->ChainParent)
        {
            RUNTIME_ASSERT(child->ChainRoot);
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
    if (callback)
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
    if (cut == SPRITE_CUT_HORIZONTAL || cut == SPRITE_CUT_VERTICAL)
    {
        bool hor = (cut == SPRITE_CUT_HORIZONTAL);

        int stepi = settings.MapHexWidth / 2;
        if (settings.MapHexagonal && hor)
            stepi = (settings.MapHexWidth + settings.MapHexWidth / 2) / 2;
        float stepf = (float)stepi;

        SpriteInfo* si = sprMngr.GetSpriteInfo(id_ptr ? *id_ptr : id);
        if (!si || si->Width < stepi * 2)
            return *spr;

        spr->CutType = cut;

        int h1, h2;
        if (hor)
        {
            h1 = spr->HexX + si->Width / 2 / stepi;
            h2 = spr->HexX - si->Width / 2 / stepi - (si->Width / 2 % stepi ? 1 : 0);
            spr->HexX = h1;
        }
        else
        {
            h1 = spr->HexY - si->Width / 2 / stepi;
            h2 = spr->HexY + si->Width / 2 / stepi + (si->Width / 2 % stepi ? 1 : 0);
            spr->HexY = h1;
        }

        float widthf = (float)si->Width;
        float xx = 0.0f;
        Sprite* parent = spr;
        for (int i = h1;;)
        {
            float ww = stepf;
            if (xx + ww > widthf)
                ww = widthf - xx;

            Sprite& spr_ = (i != h1 ? PutSprite(nullptr, draw_order, hor ? i : hx, hor ? hy : i, 0, x, y, sx, sy, id,
                                          id_ptr, ox, oy, alpha, effect, nullptr) :
                                      *spr);
            if (i != h1)
                spr_.Parent = parent;
            parent->Child = &spr_;
            parent = &spr_;

            spr_.CutX = xx;
            spr_.CutW = ww;
            spr_.CutTexL = si->SprRect.L + (si->SprRect.R - si->SprRect.L) * (xx / widthf);
            spr_.CutTexR = si->SprRect.L + (si->SprRect.R - si->SprRect.L) * ((xx + ww) / widthf);
            spr_.CutType = cut;

#ifdef FONLINE_EDITOR
            spr_.CutOyL = (hor ? -6 : -12) * ((hor ? hx : hy) - i);
            spr_.CutOyR = spr_.CutOyL;
            if (ww < stepf)
                spr_.CutOyR += (int)((hor ? 3.6f : -8.0f) * (1.0f - (ww / stepf)));
#endif

            xx += stepf;
            if (xx > widthf)
                break;

            if ((hor && --i < h2) || (!hor && ++i > h2))
                break;
        }
    }

    // Draw order
    spr->DrawOrderType = draw_order;
    spr->DrawOrderPos = (draw_order >= DRAW_ORDER_FLAT && draw_order < DRAW_ORDER ?
            spr->HexY * MAXHEX_MAX + spr->HexX + MAXHEX_MAX * MAXHEX_MAX * (draw_order - DRAW_ORDER_FLAT) :
            MAXHEX_MAX * MAXHEX_MAX * DRAW_ORDER + spr->HexY * DRAW_ORDER * MAXHEX_MAX + spr->HexX * DRAW_ORDER +
                (draw_order - DRAW_ORDER));

    return *spr;
}

Sprite& Sprites::AddSprite(int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id,
    uint* id_ptr, short* ox, short* oy, uchar* alpha, Effect** effect, bool* callback)
{
    return PutSprite(nullptr, draw_order, hx, hy, cut, x, y, sx, sy, id, id_ptr, ox, oy, alpha, effect, callback);
}

Sprite& Sprites::InsertSprite(int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id,
    uint* id_ptr, short* ox, short* oy, uchar* alpha, Effect** effect, bool* callback)
{
    // For cutted sprites need resort all tree
    if (cut == SPRITE_CUT_HORIZONTAL || cut == SPRITE_CUT_VERTICAL)
    {
        Sprite& spr =
            PutSprite(nullptr, draw_order, hx, hy, cut, x, y, sx, sy, id, id_ptr, ox, oy, alpha, effect, callback);
        SortByMapPos();
        return spr;
    }

    // Find place
    uint index = 0;
    uint pos = (draw_order >= DRAW_ORDER_FLAT && draw_order < DRAW_ORDER ?
            hy * MAXHEX_MAX + hx + MAXHEX_MAX * MAXHEX_MAX * (draw_order - DRAW_ORDER_FLAT) :
            MAXHEX_MAX * MAXHEX_MAX * DRAW_ORDER + hy * DRAW_ORDER * MAXHEX_MAX + hx * DRAW_ORDER +
                (draw_order - DRAW_ORDER));

    Sprite* parent = rootSprite;
    while (parent)
    {
        if (!parent->Valid)
            continue;
        if (pos < parent->DrawOrderPos)
            break;
        parent = parent->ChainChild;
    }

    return PutSprite(parent, draw_order, hx, hy, cut, x, y, sx, sy, id, id_ptr, ox, oy, alpha, effect, callback);
}

void Sprites::Unvalidate()
{
    while (rootSprite)
        rootSprite->Unvalidate();
    spriteCount = 0;
}

void Sprites::SortByMapPos()
{
    if (!rootSprite)
        return;

    SpriteVec sprites;
    sprites.reserve(spriteCount);
    Sprite* spr = rootSprite;
    while (spr)
    {
        sprites.push_back(spr);
        spr = spr->ChainChild;
    }

    auto& spr_infos = sprMngr.GetSpritesInfo();
    std::sort(sprites.begin(), sprites.end(), [&spr_infos](Sprite* spr1, Sprite* spr2) {
        SpriteInfo* si1 = spr_infos[spr1->PSprId ? *spr1->PSprId : spr1->SprId];
        SpriteInfo* si2 = spr_infos[spr2->PSprId ? *spr2->PSprId : spr2->SprId];
        return si1 && si2 && si1->Atlas && si2->Atlas && si1->Atlas->TextureOwner < si2->Atlas->TextureOwner;
    });

    std::sort(sprites.begin(), sprites.end(), [](Sprite* spr1, Sprite* spr2) {
        if (spr1->DrawOrderPos == spr2->DrawOrderPos)
            return spr1->TreeIndex < spr2->TreeIndex;
        return spr1->DrawOrderPos < spr2->DrawOrderPos;
    });

    for (size_t i = 0; i < sprites.size(); i++)
    {
        sprites[i]->ChainParent = nullptr;
        sprites[i]->ChainChild = nullptr;
        sprites[i]->ChainRoot = nullptr;
        sprites[i]->ChainLast = nullptr;
    }

    for (size_t i = 1; i < sprites.size(); i++)
    {
        sprites[i - 1]->ChainChild = sprites[i];
        sprites[i]->ChainParent = sprites[i - 1];
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

    for (Sprite* spr : unvalidatedSprites)
        spritesPool.push_back(spr);
    unvalidatedSprites.clear();
}
