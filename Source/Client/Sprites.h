#pragma once

#include "Common.h"
#include "GraphicStructures.h"

#define SPRITES_POOL_GROW_SIZE    ( 10000 )

class SpriteManager;
class Sprites;
class Sprite;
typedef vector< Sprite* > SpriteVec;

class Sprite
{
public:
    Sprites*   Root;
    int        DrawOrderType;
    uint       DrawOrderPos;
    uint       TreeIndex;
    uint       SprId;
    uint*      PSprId;
    int        HexX, HexY;
    int        ScrX, ScrY;
    int*       PScrX, * PScrY;
    short*     OffsX, * OffsY;
    int        CutType;
    Sprite*    Parent, * Child;
    float      CutX, CutW, CutTexL, CutTexR;
    uchar*     Alpha;
    uchar*     Light;
    uchar*     LightRight;
    uchar*     LightLeft;
    int        EggType;
    int        ContourType;
    uint       ContourColor;
    uint       Color;
    uint       FlashMask;
    Effect**   DrawEffect;
    bool*      ValidCallback;
    bool       Valid;
    MapSprite* MapSpr;
    Sprite**   ExtraChainRoot;
    Sprite*    ExtraChainParent;
    Sprite*    ExtraChainChild;
    Sprite**   ChainRoot;
    Sprite**   ChainLast;
    Sprite*    ChainParent;
    Sprite*    ChainChild;

    #ifdef FONLINE_EDITOR
    int CutOyL, CutOyR;
    #endif

    Sprite();
    void    Unvalidate();
    Sprite* GetIntersected( int ox, int oy );

    void SetEgg( int egg );
    void SetContour( int contour );
    void SetContour( int contour, uint color );
    void SetColor( uint color );
    void SetAlpha( uchar* alpha );
    void SetFlash( uint mask );
    void SetLight( int corner, uchar* light, int maxhx, int maxhy );
    void SetFixedAlpha( uchar alpha );
};

class Sprites
{
    friend class Sprite;

public:
    static void GrowPool();
    Sprites( SpriteManager& spr_mngr );
    ~Sprites();
    Sprite* RootSprite();
    Sprite& AddSprite( int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, Effect** effect, bool* callback );
    Sprite& InsertSprite( int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, Effect** effect, bool* callback );
    void    Unvalidate();
    void    SortByMapPos();
    uint    Size();
    void    Clear();

private:
    static SpriteVec spritesPool;
    SpriteManager&   sprMngr;
    Sprite*          rootSprite;
    Sprite*          lastSprite;
    uint             spriteCount;
    SpriteVec        unvalidatedSprites;

    Sprite&          PutSprite( Sprite* child, int draw_order, int hx, int hy, int cut, int x, int y, int* sx, int* sy, uint id, uint* id_ptr, short* ox, short* oy, uchar* alpha, Effect** effect, bool* callback );

};
