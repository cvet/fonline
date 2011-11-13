#include "StdAfx.h"
#include "SpriteManager.h"

#ifdef FO_D3D
# define SURF_POINT( lr, x, y )    ( *( (uint*) ( (uchar*) lr.pBits + lr.Pitch * ( y ) + ( x ) * 4 ) ) )
#endif

#define FONT_BUF_LEN          ( 0x5000 )
#define FONT_MAX_LINES        ( 1000 )
#define FORMAT_TYPE_DRAW      ( 0 )
#define FORMAT_TYPE_SPLIT     ( 1 )
#define FORMAT_TYPE_LCOUNT    ( 2 )

struct Letter
{
    short PosX;
    short PosY;
    short W;
    short H;
    short OffsX;
    short OffsY;
    short XAdvance;

    Letter(): PosX( 0 ), PosY( 0 ), W( 0 ), H( 0 ), OffsX( 0 ), OffsY( 0 ), XAdvance( 0 ) {};
};

struct FontData
{
    Texture* FontTex;
    Texture* FontTexBordered;

    Letter   Letters[ 256 ];
    int      SpaceWidth;
    int      MaxLetterHeight;
    int      YAdvance;
    RectF    TexUV[ 256 ];
    RectF    TexBorderedUV[ 256 ];
    Effect*  DrawEffect;

    FontData()
    {
        FontTex = NULL;
        FontTexBordered = NULL;
        SpaceWidth = 0;
        MaxLetterHeight = 0;
        YAdvance = 0;
        for( int i = 0; i < 256; i++ )
        {
            TexUV[ i ][ 0 ] = 0.0f;
            TexUV[ i ][ 1 ] = 0.0f;
            TexUV[ i ][ 2 ] = 0.0f;
            TexUV[ i ][ 3 ] = 0.0f;
        }
        DrawEffect = NULL;
    }
};
typedef vector< FontData* > FontDataVec;

FontDataVec Fonts;

int         DefFontIndex = -1;
uint        DefFontColor = 0;

FontData* GetFont( int num )
{
    if( num < 0 )
        num = DefFontIndex;
    if( num < 0 || num >= (int) Fonts.size() )
        return NULL;
    return Fonts[ num ];
}

struct FontFormatInfo
{
    FontData* CurFont;
    uint      Flags;
    Rect      Region;
    char      Str[ FONT_BUF_LEN ];
    char*     PStr;
    uint      LinesAll;
    uint      LinesInRect;
    int       CurX, CurY, MaxCurX;
    uint      ColorDots[ FONT_BUF_LEN ];
    short     LineWidth[ FONT_MAX_LINES ];
    ushort    LineSpaceWidth[ FONT_MAX_LINES ];
    uint      OffsColDots;
    uint      DefColor;
    StrVec*   StrLines;
    bool      IsError;

    void      Init( FontData* font, uint flags, const Rect& region, const char* str_in )
    {
        CurFont = font;
        Flags = flags;
        LinesAll = 1;
        LinesInRect = 0;
        IsError = false;
        CurX = CurY = MaxCurX = 0;
        Region = region;
        memzero( ColorDots, sizeof( ColorDots ) );
        memzero( LineWidth, sizeof( LineWidth ) );
        memzero( LineSpaceWidth, sizeof( LineSpaceWidth ) );
        OffsColDots = 0;
        Str::Copy( Str, str_in );
        PStr = Str;
        DefColor = COLOR_TEXT;
        StrLines = NULL;
    }

    FontFormatInfo& operator=( const FontFormatInfo& _fi )
    {
        CurFont = _fi.CurFont;
        Flags = _fi.Flags;
        LinesAll = _fi.LinesAll;
        LinesInRect = _fi.LinesInRect;
        IsError = _fi.IsError;
        CurX = _fi.CurX;
        CurY = _fi.CurY;
        MaxCurX = _fi.MaxCurX;
        Region = _fi.Region;
        memcpy( Str, _fi.Str, sizeof( Str ) );
        memcpy( ColorDots, _fi.ColorDots, sizeof( ColorDots ) );
        memcpy( LineWidth, _fi.LineWidth, sizeof( LineWidth ) );
        memcpy( LineSpaceWidth, _fi.LineSpaceWidth, sizeof( LineSpaceWidth ) );
        OffsColDots = _fi.OffsColDots;
        DefColor = _fi.DefColor;
        PStr = Str;
        StrLines = _fi.StrLines;
        return *this;
    }
};

void SpriteManager::SetDefaultFont( int index, uint color )
{
    DefFontIndex = index;
    DefFontColor = color;
}

void SpriteManager::SetFontEffect( int index, Effect* effect )
{
    FontData* font = GetFont( index );
    if( font )
        font->DrawEffect = effect;
}

bool SpriteManager::LoadFontFO( int index, const char* font_name )
{
    // Load font data
    char        fname[ MAX_FOPATH ];
    Str::Format( fname, "%s.fofnt", font_name );
    FileManager fm;
    if( !fm.LoadFile( fname, PT_FONTS ) )
    {
        WriteLogF( _FUNC_, " - File<%s> not found.\n", fname );
        return false;
    }

    // Parse data
    istrstream str( (char*) fm.GetBuf() );
    char       key[ MAX_FOTEXT ];
    FontData   font;
    char       image_name[ MAX_FOPATH ] = { 0 };
    int        cur_letter = 0;
    while( !str.eof() && !str.fail() )
    {
        // Get key
        str >> key;

        // Cut off comments
        char* comment = Str::Substring( key, "#" );
        if( comment )
            *comment = 0;
        comment = Str::Substring( key, ";" );
        if( comment )
            *comment = 0;

        // Get value
        if( Str::CompareCase( key, "Image" ) )
            str >> image_name;
        else if( Str::CompareCase( key, "YAdvance" ) )
            str >> font.YAdvance;
        else if( Str::CompareCase( key, "Letter" ) )
            str >> cur_letter, cur_letter = CLAMP( cur_letter, 0, 255 );
        else if( Str::CompareCase( key, "PositionX" ) )
            str >> font.Letters[ cur_letter ].PosX;
        else if( Str::CompareCase( key, "PositionY" ) )
            str >> font.Letters[ cur_letter ].PosY;
        else if( Str::CompareCase( key, "Width" ) )
            str >> font.Letters[ cur_letter ].W;
        else if( Str::CompareCase( key, "Height" ) )
            str >> font.Letters[ cur_letter ].H;
        else if( Str::CompareCase( key, "OffsetX" ) )
            str >> font.Letters[ cur_letter ].OffsX;
        else if( Str::CompareCase( key, "OffsetY" ) )
            str >> font.Letters[ cur_letter ].OffsY;
        else if( Str::CompareCase( key, "XAdvance" ) )
            str >> font.Letters[ cur_letter ].XAdvance;
    }

    // Load image
    AnyFrames* image = LoadAnimation( image_name, PT_FONTS );
    if( !image )
    {
        WriteLogF( _FUNC_, " - File<%s> not found.\n", image_name );
        return false;
    }

    // Fix texture coordinates
    SpriteInfo* si = SprMngr.GetSpriteInfo( image->GetSprId( 0 ) );
    float       tex_w = (float) si->Surf->Width;
    float       tex_h = (float) si->Surf->Height;
    float       image_x = tex_w * si->SprRect.L;
    float       image_y = tex_h * si->SprRect.T;
    int         max_h = 0;
    for( uint i = 0; i < 256; i++ )
    {
        Letter& l = font.Letters[ i ];
        float   x = (float) l.PosX;
        float   y = (float) l.PosY;
        float   w = (float) l.W;
        float   h = (float) l.H;
        font.TexUV[ i ][ 0 ] = ( image_x + x - 1.f ) / tex_w;
        font.TexUV[ i ][ 1 ] = ( image_y + y - 1.f ) / tex_h;
        font.TexUV[ i ][ 2 ] = ( image_x + x + w + 1.f ) / tex_w;
        font.TexUV[ i ][ 3 ] = ( image_y + y + h + 1.f ) / tex_h;
        if( l.H > max_h )
            max_h = l.H;
    }

    // Fill new font
    font.FontTex = si->Surf->TextureOwner;
    font.MaxLetterHeight = max_h;
    font.SpaceWidth = font.Letters[ ' ' ].XAdvance;

    // Create bordered instance
    AnyFrames* image_bordered = LoadAnimation( image_name, PT_FONTS );
    if( !image_bordered )
    {
        WriteLogF( _FUNC_, " - Can't load twice file<%s>.\n", image_name );
        return false;
    }
    SpriteInfo* si_bordered = SprMngr.GetSpriteInfo( image_bordered->GetSprId( 0 ) );
    font.FontTexBordered = si_bordered->Surf->TextureOwner;

    // Fill border
    #ifdef FO_D3D
    int                x1 = (int) ( (float) si->Surf->Width * si->SprRect.L );
    int                y1 = (int) ( (float) si->Surf->Height * si->SprRect.T );
    int                x2 = (int) ( (float) si_bordered->Surf->Width * si_bordered->SprRect.L );
    int                y2 = (int) ( (float) si_bordered->Surf->Height * si_bordered->SprRect.T );
    RECT               to_lock1 = { x1, y1, x1 + si->Width, y1 + si->Height };
    RECT               to_lock2 = { x2, y2, x2 + si_bordered->Width, y2 + si_bordered->Height };
    LPDIRECT3DTEXTURE9 tex;
    uint               bw = si_bordered->Width;
    uint               bh = si_bordered->Height;
    D3D_HR( D3DXCreateTexture( d3dDevice, bw, bh, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex ) );
    LockRect_          lr, lrb;
    D3D_HR( font.FontTex->Instance->LockRect( 0, &lr, &to_lock1, 0 ) );
    D3D_HR( tex->LockRect( 0, &lrb, NULL, 0 ) );
    for( uint i = 0; i < bh; i++ )
        memcpy( (uchar*) lrb.pBits + lrb.Pitch * i, (uchar*) lr.pBits + lr.Pitch * i, bw * 4 );
    for( uint y = 0; y < bh; y++ )
        for( uint x = 0; x < bw; x++ )
            if( SURF_POINT( lr, x, y ) )
                for( int xx = -1; xx <= 1; xx++ )
                    for( int yy = -1; yy <= 1; yy++ )
                        if( !SURF_POINT( lrb, x + xx, y + yy ) )
                            SURF_POINT( lrb, x + xx, y + yy ) = 0xFF000000;
    D3D_HR( font.FontTex->Instance->UnlockRect( 0 ) );
    D3D_HR( font.FontTexBordered->Instance->LockRect( 0, &lr, &to_lock2, 0 ) );
    for( uint i = 0; i < bh; i++ )
        memcpy( (uchar*) lr.pBits + lr.Pitch * i, (uchar*) lrb.pBits + lrb.Pitch * i, bw * 4 );
    D3D_HR( font.FontTexBordered->Instance->UnlockRect( 0 ) );
    D3D_HR( tex->UnlockRect( 0 ) );
    tex->Release();
    #else
    uint normal_ox = (uint) ( tex_w * si->SprRect.L );
    uint normal_oy = (uint) ( tex_h * si->SprRect.T );
    uint bordered_ox = (uint) ( tex_w * si_bordered->SprRect.L );
    uint bordered_oy = (uint) ( tex_h * si_bordered->SprRect.T );
    for( uint y = 0; y < (uint) si_bordered->Height; y++ )
    {
        for( uint x = 0; x < (uint) si_bordered->Width; x++ )
        {
            if( si->Surf->TextureOwner->Pixel( normal_ox + x, normal_oy + y ) )
            {
                for( int xx = -1; xx <= 1; xx++ )
                {
                    for( int yy = -1; yy <= 1; yy++ )
                    {
                        uint ox = bordered_ox + x + xx;
                        uint oy = bordered_oy + y + yy;
                        if( !si_bordered->Surf->TextureOwner->Pixel( ox, oy ) )
                            si_bordered->Surf->TextureOwner->Pixel( ox, oy ) = COLOR_XRGB( 0, 0, 0 );
                    }
                }
            }
        }
    }
    Rect r = Rect( bordered_ox, bordered_oy, bordered_ox + si->Width - 1, bordered_oy + si->Height - 1 );
    si_bordered->Surf->TextureOwner->Update( r );
    #endif

    // Fix texture coordinates on bordered texture
    tex_w = (float) si_bordered->Surf->Width;
    tex_h = (float) si_bordered->Surf->Height;
    image_x = tex_w * si_bordered->SprRect.L;
    image_y = tex_h * si_bordered->SprRect.T;
    for( uint i = 0; i < 256; i++ )
    {
        Letter& l = font.Letters[ i ];
        float   x = (float) l.PosX;
        float   y = (float) l.PosY;
        float   w = (float) l.W;
        float   h = (float) l.H;
        font.TexBorderedUV[ i ][ 0 ] = ( image_x + x - 1.f ) / tex_w;
        font.TexBorderedUV[ i ][ 1 ] = ( image_y + y - 1.f ) / tex_h;
        font.TexBorderedUV[ i ][ 2 ] = ( image_x + x + w + 1.f ) / tex_w;
        font.TexBorderedUV[ i ][ 3 ] = ( image_y + y + h + 1.f ) / tex_h;
    }

    // Register
    if( index >= (int) Fonts.size() )
        Fonts.resize( index + 1 );
    SAFEDEL( Fonts[ index ] );
    Fonts[ index ] = new FontData( font );

    return true;
}

bool SpriteManager::LoadFontBMF( int index, const char* font_name )
{
    if( index < 0 )
    {
        WriteLogF( _FUNC_, " - Invalid index.\n" );
        return false;
    }

    FontData    font;
    FileManager fm;
    FileManager fm_tex;

    if( !fm.LoadFile( Str::FormatBuf( "%s.fnt", font_name ), PT_FONTS ) )
    {
        WriteLogF( _FUNC_, " - Font file<%s> not found.\n", Str::FormatBuf( "%s.fnt", font_name ) );
        return false;
    }

    uint signature = fm.GetLEUInt();
    if( signature != MAKEUINT( 'B', 'M', 'F', 3 ) )
    {
        WriteLogF( _FUNC_, " - Invalid signature of font<%s>.\n", font_name );
        return false;
    }

    // Info
    fm.GetUChar();
    uint block_len = fm.GetLEUInt();
    uint next_block = block_len + fm.GetCurPos() + 1;

    fm.GoForward( 7 );
    if( fm.GetUChar() != 1 || fm.GetUChar() != 1 || fm.GetUChar() != 1 || fm.GetUChar() != 1 )
    {
        WriteLogF( _FUNC_, " - Wrong padding in font<%s>.\n", font_name );
        return false;
    }

    // Common
    fm.SetCurPos( next_block );
    block_len = fm.GetLEUInt();
    next_block = block_len + fm.GetCurPos() + 1;

    int line_height = fm.GetLEUShort();
    int base_height = fm.GetLEUShort();
    fm.GetLEUShort(); // Texture width
    fm.GetLEUShort(); // Texture height

    if( fm.GetLEUShort() != 1 )
    {
        WriteLogF( _FUNC_, " - Texture for font<%s> must be single.\n", font_name );
        return false;
    }

    // Pages
    fm.SetCurPos( next_block );
    block_len = fm.GetLEUInt();
    next_block = block_len + fm.GetCurPos() + 1;

    // Image name
    char image_name[ MAX_FOPATH ] = { 0 };
    fm.GetStr( image_name );

    // Chars
    fm.SetCurPos( next_block );
    int count = fm.GetLEUInt() / 20;
    for( int i = 0; i < count; i++ )
    {
        // Read data
        int id = fm.GetLEUInt();
        int x = fm.GetLEUShort();
        int y = fm.GetLEUShort();
        int w = fm.GetLEUShort();
        int h = fm.GetLEUShort();
        int ox = fm.GetLEUShort();
        int oy = fm.GetLEUShort();
        int xa = fm.GetLEUShort();
        fm.GoForward( 2 );

        if( id < 0 || id > 255 )
            continue;

        // Fill data
        Letter& let = font.Letters[ id ];
        let.PosX = x;
        let.PosY = y;
        let.W = w - 2;
        let.H = h - 2;
        let.OffsX = -ox;
        let.OffsY = -oy + ( line_height - base_height );
        let.XAdvance = xa + 1;
    }

    font.YAdvance = 1;
    font.SpaceWidth = font.Letters[ ' ' ].XAdvance;
    font.MaxLetterHeight = base_height;

    // Load image
    AnyFrames* image = LoadAnimation( image_name, PT_FONTS );
    if( !image )
    {
        WriteLogF( _FUNC_, " - File<%s> not found.\n", image_name );
        return false;
    }

    // Fix texture coordinates
    SpriteInfo* si = SprMngr.GetSpriteInfo( image->GetSprId( 0 ) );
    font.FontTex = si->Surf->TextureOwner;
    float       tex_w = (float) si->Surf->Width;
    float       tex_h = (float) si->Surf->Height;
    float       image_x = tex_w * si->SprRect.L;
    float       image_y = tex_h * si->SprRect.T;
    for( uint i = 0; i < 256; i++ )
    {
        Letter& l = font.Letters[ i ];
        float   x = (float) l.PosX;
        float   y = (float) l.PosY;
        float   w = (float) l.W;
        float   h = (float) l.H;
        font.TexUV[ i ][ 0 ] = ( image_x + x - 0.f ) / tex_w;
        font.TexUV[ i ][ 1 ] = ( image_y + y - 0.f ) / tex_h;
        font.TexUV[ i ][ 2 ] = ( image_x + x + w + 2.f ) / tex_w;
        font.TexUV[ i ][ 3 ] = ( image_y + y + h + 2.f ) / tex_h;
    }

    // Create bordered instance
    AnyFrames* image_bordered = LoadAnimation( image_name, PT_FONTS );
    if( !image_bordered )
    {
        WriteLogF( _FUNC_, " - Can't load twice file<%s>.\n", image_name );
        return false;
    }
    SpriteInfo* si_bordered = SprMngr.GetSpriteInfo( image_bordered->GetSprId( 0 ) );
    font.FontTexBordered = si_bordered->Surf->TextureOwner;

    // Fill border
    #ifdef FO_D3D
    int                x1 = (int) ( (float) si->Surf->Width * si->SprRect.L );
    int                y1 = (int) ( (float) si->Surf->Height * si->SprRect.T );
    int                x2 = (int) ( (float) si_bordered->Surf->Width * si_bordered->SprRect.L );
    int                y2 = (int) ( (float) si_bordered->Surf->Height * si_bordered->SprRect.T );
    RECT               to_lock1 = { x1, y1, x1 + si->Width, y1 + si->Height };
    RECT               to_lock2 = { x2, y2, x2 + si_bordered->Width, y2 + si_bordered->Height };
    LPDIRECT3DTEXTURE9 tex;
    uint               bw = si_bordered->Width;
    uint               bh = si_bordered->Height;
    D3D_HR( D3DXCreateTexture( d3dDevice, bw, bh, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex ) );
    LockRect_          lr, lrb;
    D3D_HR( font.FontTex->Instance->LockRect( 0, &lr, &to_lock1, 0 ) );
    D3D_HR( tex->LockRect( 0, &lrb, NULL, 0 ) );
    for( uint y = 0; y < bh; y++ )
        for( uint x = 0; x < bw; x++ )
            if( SURF_POINT( lr, x, y ) & 0xFF000000 )
                SURF_POINT( lr, x, y ) = ( SURF_POINT( lr, x, y ) & 0xFF000000 ) | ( 0x7F7F7F );
            else
                SURF_POINT( lr, x, y ) = 0;
    for( uint i = 0; i < bh; i++ )
        memcpy( (uchar*) lrb.pBits + lrb.Pitch * i, (uchar*) lr.pBits + lr.Pitch * i, bw * 4 );
    for( uint y = 0; y < bh; y++ )
        for( uint x = 0; x < bw; x++ )
            if( SURF_POINT( lr, x, y ) )
                for( int xx = -1; xx <= 1; xx++ )
                    for( int yy = -1; yy <= 1; yy++ )
                        if( !SURF_POINT( lrb, x + xx, y + yy ) )
                            SURF_POINT( lrb, x + xx, y + yy ) = 0xFF000000;
    D3D_HR( font.FontTex->Instance->UnlockRect( 0 ) );
    D3D_HR( font.FontTexBordered->Instance->LockRect( 0, &lr, &to_lock2, 0 ) );
    for( uint i = 0; i < bh; i++ )
        memcpy( (uchar*) lr.pBits + lr.Pitch * i, (uchar*) lrb.pBits + lrb.Pitch * i, bw * 4 );
    D3D_HR( font.FontTexBordered->Instance->UnlockRect( 0 ) );
    D3D_HR( tex->UnlockRect( 0 ) );
    tex->Release();
    #endif

    // Fix texture coordinates on bordered texture
    tex_w = (float) si_bordered->Surf->Width;
    tex_h = (float) si_bordered->Surf->Height;
    image_x = tex_w * si_bordered->SprRect.L;
    image_y = tex_h * si_bordered->SprRect.T;
    for( uint i = 0; i < 256; i++ )
    {
        Letter& l = font.Letters[ i ];
        float   x = (float) l.PosX;
        float   y = (float) l.PosY;
        float   w = (float) l.W;
        float   h = (float) l.H;
        font.TexBorderedUV[ i ][ 0 ] = ( image_x + x - 0.f ) / tex_w;
        font.TexBorderedUV[ i ][ 1 ] = ( image_y + y - 0.f ) / tex_h;
        font.TexBorderedUV[ i ][ 2 ] = ( image_x + x + w + 2.f ) / tex_w;
        font.TexBorderedUV[ i ][ 3 ] = ( image_y + y + h + 2.f ) / tex_h;
    }

    // Register
    if( index >= (int) Fonts.size() )
        Fonts.resize( index + 1 );
    SAFEDEL( Fonts[ index ] );
    Fonts[ index ] = new FontData( font );

    return true;
}

void FormatText( FontFormatInfo& fi, int fmt_type )
{
    char*     str = fi.PStr;
    uint      flags = fi.Flags;
    FontData* font = fi.CurFont;
    Rect&     r = fi.Region;
    int&      curx = fi.CurX;
    int&      cury = fi.CurY;
    uint&     offs_col = fi.OffsColDots;

    if( fmt_type != FORMAT_TYPE_DRAW && fmt_type != FORMAT_TYPE_LCOUNT && fmt_type != FORMAT_TYPE_SPLIT )
    {
        fi.IsError = true;
        return;
    }

    if( fmt_type == FORMAT_TYPE_SPLIT && !fi.StrLines )
    {
        fi.IsError = true;
        return;
    }

    // Colorize
    uint* dots = NULL;
    uint  d_offs = 0;
    char* str_ = str;
    char* big_buf = Str::GetBigBuf();
    big_buf[ 0 ] = 0;
    if( fmt_type == FORMAT_TYPE_DRAW && !FLAG( flags, FT_NO_COLORIZE ) )
        dots = fi.ColorDots;

    while( *str_ )
    {
        char* s0 = str_;
        Str::GoTo( str_, '|' );
        char* s1 = str_;
        Str::GoTo( str_, ' ' );
        char* s2 = str_;

        // TODO: optimize
        // if(!_str[0] && !*s1) break;
        if( dots )
        {
            uint d_len = (uint) s2 - (uint) s1 + 1;
            uint d = strtoul( s1 + 1, NULL, 0 );

            dots[ (uint) s1 - (uint) str - d_offs ] = d;
            d_offs += d_len;
        }

        *s1 = 0;
        Str::Append( big_buf, 0x10000, s0 );

        if( !*str_ )
            break;
        str_++;
    }

    Str::Copy( str, FONT_BUF_LEN, big_buf );

    // Skip lines
    uint skip_line = ( FLAG( flags, FT_SKIPLINES( 0 ) ) ? flags >> 16 : 0 );
    uint skip_line_end = ( FLAG( flags, FT_SKIPLINES_END( 0 ) ) ? flags >> 16 : 0 );

    // Format
    curx = r.L;
    cury = r.T;
    for( int i = 0; str[ i ]; i++ )
    {
        int lett_len;
        switch( str[ i ] )
        {
        case '\r':
            continue;
        case ' ':
            lett_len = font->SpaceWidth;
            break;
        case '\t':
            lett_len = font->SpaceWidth * 4;
            break;
        default:
            lett_len = font->Letters[ (uchar) str[ i ] ].XAdvance;
            break;
        }

        if( curx + lett_len > r.R )
        {
            if( curx - r.L > fi.MaxCurX )
                fi.MaxCurX = curx - r.L;

            if( fmt_type == FORMAT_TYPE_DRAW && FLAG( flags, FT_NOBREAK ) )
            {
                str[ i ] = '\0';
                break;
            }
            else if( FLAG( flags, FT_NOBREAK_LINE ) )
            {
                int j = i;
                for( ; str[ j ]; j++ )
                    if( str[ j ] == '\n' )
                        break;

                Str::EraseInterval( &str[ i ], j - i );
                if( fmt_type == FORMAT_TYPE_DRAW )
                    for( int k = i, l = MAX_FOTEXT - ( j - i ); k < l; k++ )
                        fi.ColorDots[ k ] = fi.ColorDots[ k + ( j - i ) ];
            }
            else if( str[ i ] != '\n' )
            {
                int j = i;
                for( ; j >= 0; j-- )
                {
                    if( str[ j ] == ' ' || str[ j ] == '\t' )
                    {
                        str[ j ] = '\n';
                        i = j;
                        break;
                    }
                    else if( str[ j ] == '\n' )
                    {
                        j = -1;
                        break;
                    }
                }

                if( j < 0 )
                {
                    Str::Insert( &str[ i ], "\n" );
                    if( fmt_type == FORMAT_TYPE_DRAW )
                        for( int k = MAX_FOTEXT - 1; k > i; k-- )
                            fi.ColorDots[ k ] = fi.ColorDots[ k - 1 ];
                }

                if( FLAG( flags, FT_ALIGN ) && !skip_line )
                {
                    fi.LineSpaceWidth[ fi.LinesAll - 1 ] = 1;
                    // Erase next first spaces
                    int ii = i + 1;
                    for( j = ii; ; j++ )
                        if( str[ j ] != ' ' )
                            break;
                    if( j > ii )
                    {
                        Str::EraseInterval( &str[ ii ], j - ii );
                        if( fmt_type == FORMAT_TYPE_DRAW )
                            for( int k = ii, l = MAX_FOTEXT - ( j - ii ); k < l; k++ )
                                fi.ColorDots[ k ] = fi.ColorDots[ k + ( j - ii ) ];
                    }
                }
            }
        }

        switch( str[ i ] )
        {
        case '\n':
            if( !skip_line )
            {
                cury += font->MaxLetterHeight + font->YAdvance;
                if( cury + font->MaxLetterHeight > r.B && !fi.LinesInRect )
                    fi.LinesInRect = fi.LinesAll;

                if( fmt_type == FORMAT_TYPE_DRAW )
                {
                    if( fi.LinesInRect && !FLAG( flags, FT_UPPER ) )
                    {
                        // fi.LinesAll++;
                        str[ i ] = '\0';
                        break;
                    }
                }
                else if( fmt_type == FORMAT_TYPE_SPLIT )
                {
                    if( fi.LinesInRect && !( fi.LinesAll % fi.LinesInRect ) )
                    {
                        str[ i ] = '\0';
                        ( *fi.StrLines ).push_back( str );
                        str = &str[ i + 1 ];
                        i = -1;
                    }
                }

                if( str[ i + 1 ] )
                    fi.LinesAll++;
            }
            else
            {
                skip_line--;
                Str::EraseInterval( str, i + 1 );
                offs_col += i + 1;
                //	if(fmt_type==FORMAT_TYPE_DRAW)
                //		for(int k=0,l=MAX_FOTEXT-(i+1);k<l;k++) fi.ColorDots[k]=fi.ColorDots[k+i+1];
                i = -1;
            }

            curx = r.L;
            continue;
        case '\0':
            break;
        default:
            curx += lett_len;
            continue;
        }

        if( !str[ i ] )
            break;
    }
    if( curx - r.L > fi.MaxCurX )
        fi.MaxCurX = curx - r.L;

    if( skip_line_end )
    {
        int len = (int) Str::Length( str );
        for( int i = len - 2; i >= 0; i-- )
        {
            if( str[ i ] == '\n' )
            {
                str[ i ] = 0;
                fi.LinesAll--;
                if( !--skip_line_end )
                    break;
            }
        }

        if( skip_line_end )
        {
            fi.IsError = true;
            return;
        }
    }

    if( skip_line )
    {
        fi.IsError = true;
        return;
    }

    if( !fi.LinesInRect )
        fi.LinesInRect = fi.LinesAll;

    if( fi.LinesAll > FONT_MAX_LINES )
    {
        fi.IsError = true;
        return;
    }

    if( fmt_type == FORMAT_TYPE_SPLIT )
    {
        ( *fi.StrLines ).push_back( string( str ) );
        return;
    }
    else if( fmt_type == FORMAT_TYPE_LCOUNT )
    {
        return;
    }

    // Up text
    if( FLAG( flags, FT_UPPER ) && fi.LinesAll > fi.LinesInRect )
    {
        uint j = 0;
        uint line_cur = 0;
        uint last_col = 0;
        for( ; str[ j ]; ++j )
        {
            if( str[ j ] == '\n' )
            {
                line_cur++;
                if( line_cur >= ( fi.LinesAll - fi.LinesInRect ) )
                    break;
            }

            if( fi.ColorDots[ j ] )
                last_col = fi.ColorDots[ j ];
        }

        if( !FLAG( flags, FT_NO_COLORIZE ) )
        {
            offs_col += j + 1;
            if( last_col && !fi.ColorDots[ j + 1 ] )
                fi.ColorDots[ j + 1 ] = last_col;
        }

        str = &str[ j + 1 ];
        fi.PStr = str;

        fi.LinesAll = fi.LinesInRect;
    }

    // Align
    curx = r.L;
    cury = r.T;

    for( uint i = 0; i < fi.LinesAll; i++ )
        fi.LineWidth[ i ] = curx;

    bool can_count = false;
    int  spaces = 0;
    int  curstr = 0;
    for( int i = 0; ; i++ )
    {
        switch( str[ i ] )
        {
        case ' ':
            curx += font->SpaceWidth;
            if( can_count )
                spaces++;
            break;
        case '\t':
            curx += font->SpaceWidth * 4;
            break;
        case '\0':
        case '\n':
            fi.LineWidth[ curstr ] = curx;
            cury += font->MaxLetterHeight + font->YAdvance;
            curx = r.L;

            // Erase last spaces
            /*for(int j=i-1;spaces>0 && j>=0;j--)
               {
               if(str[j]==' ')
               {
               spaces--;
               str[j]='\r';
               }
               else if(str[j]!='\r') break;
               }*/

            // Align
            if( fi.LineSpaceWidth[ curstr ] == 1 && spaces > 0 )
            {
                fi.LineSpaceWidth[ curstr ] = font->SpaceWidth + ( r.R - fi.LineWidth[ curstr ] ) / spaces;
                // WriteLog("%d) %d + ( %d - %d ) / %d = %u\n",curstr,font->SpaceWidth,r.R,fi.LineWidth[curstr],spaces,fi.LineSpaceWidth[curstr]);
            }
            else
                fi.LineSpaceWidth[ curstr ] = font->SpaceWidth;

            curstr++;
            can_count = false;
            spaces = 0;
            break;
        case '\r':
            break;
        default:
            curx += font->Letters[ (uchar) str[ i ] ].XAdvance;
            // if(curx>fi.LineWidth[curstr]) fi.LineWidth[curstr]=curx;
            can_count = true;
            break;
        }

        if( !str[ i ] )
            break;
    }

    curx = r.L;
    cury = r.T;

    // Align X
    if( FLAG( flags, FT_CENTERX ) )
        curx += ( r.R - fi.LineWidth[ 0 ] ) / 2;
    else if( FLAG( flags, FT_CENTERR ) )
        curx += r.R - fi.LineWidth[ 0 ];
    // Align Y
    if( FLAG( flags, FT_CENTERY ) )
        cury = r.T + ( r.H() - fi.LinesAll * font->MaxLetterHeight - ( fi.LinesAll - 1 ) * font->YAdvance ) / 2 + 1;
    else if( FLAG( flags, FT_BOTTOM ) )
        cury = r.B - ( fi.LinesAll * font->MaxLetterHeight + ( fi.LinesAll - 1 ) * font->YAdvance );
}

bool SpriteManager::DrawStr( const Rect& r, const char* str, uint flags, uint color /* = 0 */, int num_font /* = -1 */ )
{
    // Check
    if( !str || !str[ 0 ] )
        return false;

    // Get font
    FontData* font = GetFont( num_font );
    if( !font )
        return false;

    // FormatBuf
    if( !color && DefFontColor )
        color = DefFontColor;

    static FontFormatInfo fi;
    fi.Init( font, flags, r, str );
    fi.DefColor = color;
    FormatText( fi, FORMAT_TYPE_DRAW );
    if( fi.IsError )
        return false;

    char*    str_ = fi.PStr;
    uint     offs_col = fi.OffsColDots;
    int      curx = fi.CurX;
    int      cury = fi.CurY;
    int      curstr = 0;
    Texture* texture = ( FLAG( flags, FT_BORDERED ) ? font->FontTexBordered : font->FontTex );
    RectF*   textture_uv = ( FLAG( flags, FT_BORDERED ) ? font->TexBorderedUV : font->TexUV );

    if( curSprCnt )
        Flush();

    if( !FLAG( flags, FT_NO_COLORIZE ) )
    {
        for( int i = offs_col; i >= 0; i-- )
        {
            if( fi.ColorDots[ i ] )
            {
                if( fi.ColorDots[ i ] & 0xFF000000 )
                    color = fi.ColorDots[ i ];                                            // With alpha
                else
                    color = ( color & 0xFF000000 ) | ( fi.ColorDots[ i ] & 0x00FFFFFF );  // Still old alpha
                break;
            }
        }
    }

    bool variable_space = false;
    for( int i = 0; str_[ i ]; i++ )
    {
        if( !FLAG( flags, FT_NO_COLORIZE ) )
        {
            uint new_color = fi.ColorDots[ i + offs_col ];
            if( new_color )
            {
                if( new_color & 0xFF000000 )
                    color = new_color;                                            // With alpha
                else
                    color = ( color & 0xFF000000 ) | ( new_color & 0x00FFFFFF );  // Still old alpha
            }
        }

        switch( str_[ i ] )
        {
        case ' ':
            curx += ( variable_space ? fi.LineSpaceWidth[ curstr ] : font->SpaceWidth );
            continue;
        case '\t':
            curx += font->SpaceWidth * 4;
            continue;
        case '\n':
            cury += font->MaxLetterHeight + font->YAdvance;
            curx = r.L;
            curstr++;
            variable_space = false;
            if( FLAG( flags, FT_CENTERX ) )
                curx += ( r.R - fi.LineWidth[ curstr ] ) / 2;
            else if( FLAG( flags, FT_CENTERR ) )
                curx += r.R - fi.LineWidth[ curstr ];
            continue;
        case '\r':
            continue;
        default:
            Letter& l = font->Letters[ (uchar) str_[ i ] ];
            int   mulpos = curSprCnt * 4;
            int   x = curx - l.OffsX - 1;
            int   y = cury - l.OffsY - 1;
            int   w = l.W + 2;
            int   h = l.H + 2;

            float x1 = textture_uv[ (uchar) str_[ i ] ][ 0 ];
            float y1 = textture_uv[ (uchar) str_[ i ] ][ 1 ];
            float x2 = textture_uv[ (uchar) str_[ i ] ][ 2 ];
            float y2 = textture_uv[ (uchar) str_[ i ] ][ 3 ];

            vBuffer[ mulpos ].x = (float) x;
            vBuffer[ mulpos ].y = (float) y + h;
            vBuffer[ mulpos ].tu = x1;
            vBuffer[ mulpos ].tv = y2;
            vBuffer[ mulpos++ ].diffuse = color;

            vBuffer[ mulpos ].x = (float) x;
            vBuffer[ mulpos ].y = (float) y;
            vBuffer[ mulpos ].tu = x1;
            vBuffer[ mulpos ].tv = y1;
            vBuffer[ mulpos++ ].diffuse = color;

            vBuffer[ mulpos ].x = (float) x + w;
            vBuffer[ mulpos ].y = (float) y;
            vBuffer[ mulpos ].tu = x2;
            vBuffer[ mulpos ].tv = y1;
            vBuffer[ mulpos++ ].diffuse = color;

            vBuffer[ mulpos ].x = (float) x + w;
            vBuffer[ mulpos ].y = (float) y + h;
            vBuffer[ mulpos ].tu = x2;
            vBuffer[ mulpos ].tv = y2;
            vBuffer[ mulpos ].diffuse = color;

            if( ++curSprCnt == flushSprCnt )
            {
                dipQueue.push_back( DipData( texture, font->DrawEffect ) );
                dipQueue.back().SpritesCount = curSprCnt;
                Flush();
            }

            curx += l.XAdvance;
            variable_space = true;
        }
    }

    if( curSprCnt )
    {
        dipQueue.push_back( DipData( texture, font->DrawEffect ) );
        dipQueue.back().SpritesCount = curSprCnt;
        Flush();
    }

    return true;
}

int SpriteManager::GetLinesCount( int width, int height, const char* str, int num_font /* = -1 */ )
{
    FontData* font = GetFont( num_font );
    if( !font )
        return 0;

    if( !str )
        return height / ( font->MaxLetterHeight + font->YAdvance );

    static FontFormatInfo fi;
    fi.Init( font, 0, Rect( 0, 0, width ? width : modeWidth, height ? height : modeHeight ), str );
    FormatText( fi, FORMAT_TYPE_LCOUNT );
    if( fi.IsError )
        return 0;
    return fi.LinesInRect;
}

int SpriteManager::GetLinesHeight( int width, int height, const char* str, int num_font /* = -1 */ )
{
    FontData* font = GetFont( num_font );
    if( !font )
        return 0;
    int cnt = GetLinesCount( width, height, str, num_font );
    if( cnt <= 0 )
        return 0;
    return cnt * font->MaxLetterHeight + ( cnt - 1 ) * font->YAdvance;
}

int SpriteManager::GetLineHeight( int num_font /* = -1 */ )
{
    FontData* font = GetFont( num_font );
    if( !font )
        return 0;
    return font->MaxLetterHeight;
}

void SpriteManager::GetTextInfo( int width, int height, const char* str, int num_font, int flags, int& tw, int& th, int& lines )
{
    tw = th = lines = 0;
    if( width <= 0 )
        width = GameOpt.ScreenWidth;
    if( height <= 0 )
        height = GameOpt.ScreenHeight;

    FontData* font = GetFont( num_font );
    if( !font )
        return;

    static FontFormatInfo fi;
    fi.Init( font, flags, Rect( 0, 0, width, height ), str );
    FormatText( fi, FORMAT_TYPE_LCOUNT );
    if( fi.IsError )
        return;

    lines = fi.LinesInRect;
    th = fi.LinesInRect * font->MaxLetterHeight + ( fi.LinesInRect - 1 ) * font->YAdvance;
    tw = fi.MaxCurX;
}

int SpriteManager::SplitLines( const Rect& r, const char* cstr, int num_font, StrVec& str_vec )
{
    // Check & Prepare
    str_vec.clear();
    if( !cstr || !cstr[ 0 ] )
        return 0;

    // Get font
    FontData* font = GetFont( num_font );
    if( !font )
        return 0;
    static FontFormatInfo fi;
    fi.Init( font, 0, r, cstr );
    fi.StrLines = &str_vec;
    FormatText( fi, FORMAT_TYPE_SPLIT );
    if( fi.IsError )
        return 0;
    return (int) str_vec.size();
}
