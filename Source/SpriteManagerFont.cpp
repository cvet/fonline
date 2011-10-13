#include "StdAfx.h"
#include "SpriteManager.h"

#define SURF_POINT( lr, x, y )    ( *( (uint*) ( (uchar*) lr.pBits + lr.Pitch * ( y ) + ( x ) * 4 ) ) )

#define FONT_BUF_LEN          ( 0x5000 )
#define FONT_MAX_LINES        ( 1000 )
#define FORMAT_TYPE_DRAW      ( 0 )
#define FORMAT_TYPE_SPLIT     ( 1 )
#define FORMAT_TYPE_LCOUNT    ( 2 )

struct Letter
{
    short W;
    short H;
    short OffsW;
    short OffsH;
    short XAdvance;

    Letter(): W( 0 ), H( 0 ), OffsW( 0 ), OffsH( 0 ), XAdvance( 0 ) {};
};

struct Font
{
    LPDIRECT3DTEXTURE9 FontTex;
    LPDIRECT3DTEXTURE9 FontTexBordered;

    Letter             Letters[ 256 ];
    int                SpaceWidth;
    int                MaxLettHeight;
    int                EmptyVer;
    FLOAT              ArrXY[ 256 ][ 4 ];
    EffectEx*          Effect;

    Font()
    {
        FontTex = NULL;
        FontTexBordered = NULL;
        SpaceWidth = 0;
        MaxLettHeight = 0;
        EmptyVer = 0;
        for( int i = 0; i < 256; i++ )
        {
            ArrXY[ i ][ 0 ] = 0.0f;
            ArrXY[ i ][ 1 ] = 0.0f;
            ArrXY[ i ][ 2 ] = 0.0f;
            ArrXY[ i ][ 3 ] = 0.0f;
        }
        Effect = NULL;
    }
};
typedef vector< Font* > FontVec;

FontVec Fonts;

int     DefFontIndex = -1;
uint    DefFontColor = 0;

Font* GetFont( int num )
{
    if( num < 0 )
        num = DefFontIndex;
    if( num < 0 || num >= (int) Fonts.size() )
        return NULL;
    return Fonts[ num ];
}

struct FontFormatInfo
{
    Font*   CurFont;
    uint    Flags;
    INTRECT Rect;
    char    Str[ FONT_BUF_LEN ];
    char*   PStr;
    uint    LinesAll;
    uint    LinesInRect;
    int     CurX, CurY, MaxCurX;
    uint    ColorDots[ FONT_BUF_LEN ];
    short   LineWidth[ FONT_MAX_LINES ];
    ushort  LineSpaceWidth[ FONT_MAX_LINES ];
    uint    OffsColDots;
    uint    DefColor;
    StrVec* StrLines;
    bool    IsError;

    void    Init( Font* font, uint flags, INTRECT& rect, const char* str_in )
    {
        CurFont = font;
        Flags = flags;
        LinesAll = 1;
        LinesInRect = 0;
        IsError = false;
        CurX = CurY = MaxCurX = 0;
        Rect = rect;
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
        Rect = _fi.Rect;
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

void SpriteManager::SetFontEffect( int index, EffectEx* effect )
{
    Font* font = GetFont( index );
    if( font )
        font->Effect = effect;
}

bool SpriteManager::LoadFontOld( int index, const char* font_name, int size_mod )
{
    if( index < 0 )
    {
        WriteLogF( _FUNC_, " - Invalid index.\n" );
        return false;
    }

    Font   font;
    int    tex_w = 256 * size_mod;
    int    tex_h = 256 * size_mod;

    uchar* data = new uchar[ tex_w * tex_h * 4 ]; // TODO: Leak
    if( !data )
    {
        WriteLogF( _FUNC_, " - Data allocation fail.\n" );
        return false;
    }
    memzero( data, tex_w * tex_h * 4 );

    FileManager fm;
    if( !fm.LoadFile( Str::FormatBuf( "%s.bmp", font_name ), PT_FONTS ) )
    {
        WriteLogF( _FUNC_, " - File<%s> not found.\n", Str::FormatBuf( "%s.bmp", font_name ) );
        delete[] data;
        return false;
    }

    LPDIRECT3DTEXTURE9 image = NULL;
    D3D_HR( D3DXCreateTextureFromFileInMemoryEx( d3dDevice, fm.GetBuf(), fm.GetFsize(), D3DX_DEFAULT, D3DX_DEFAULT, 1, 0,
                                                 D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, D3DCOLOR_ARGB( 255, 0, 0, 0 ), NULL, NULL, &image ) );

    D3DLOCKED_RECT lr;
    D3D_HR( image->LockRect( 0, &lr, NULL, D3DLOCK_READONLY ) );

    if( !fm.LoadFile( Str::FormatBuf( "%s.fnt0", font_name ), PT_FONTS ) )
    {
        WriteLogF( _FUNC_, " - File<%s> not found.\n", Str::FormatBuf( "%s.fnt0", font_name ) );
        delete[] data;
        SAFEREL( image );
        return false;
    }

    int empty_hor = fm.GetBEUInt();
    font.EmptyVer = fm.GetBEUInt();
    font.MaxLettHeight = fm.GetBEUInt();
    font.SpaceWidth = fm.GetBEUInt();

    struct LetterOldFont
    {
        ushort Dx;
        ushort Dy;
        uchar  W;
        uchar  H;
        short  OffsH;
    } letters[ 256 ];

    if( !fm.CopyMem( letters, sizeof( LetterOldFont ) * 256 ) )
    {
        WriteLogF( _FUNC_, " - Incorrect size in file<%s>.\n", Str::FormatBuf( "%s.fnt0", font_name ) );
        delete[] data;
        SAFEREL( image );
        return false;
    }

    D3DSURFACE_DESC sd;
    D3D_HR( image->GetLevelDesc( 0, &sd ) );
    uint            wd = sd.Width;

    int             cur_x = 0;
    int             cur_y = 0;
    for( int i = 0; i < 256; i++ )
    {
        LetterOldFont& letter_old = letters[ i ];
        Letter&        letter = font.Letters[ i ];

        int            w = letter_old.W;
        int            h = letter_old.H;
        if( !w || !h )
            continue;

        letter.W = letter_old.W;
        letter.H = letter_old.H;
        letter.OffsH = letter.OffsH;
        letter.XAdvance = w + empty_hor;

        if( cur_x + w + 2 >= tex_w )
        {
            cur_x = 0;
            cur_y += font.MaxLettHeight + 2;
            if( cur_y + font.MaxLettHeight + 2 >= tex_h )
            {
                delete[] data;
                SAFEREL( image );
                // WriteLog("<%s> growed to %d\n",font_name,size_mod*2);
                return LoadFontOld( index, font_name, size_mod * 2 );
            }
        }

        for( int j = 0; j < h; j++ )
            memcpy( data + ( cur_y + j + 1 ) * tex_w * 4 + ( cur_x + 1 ) * 4, (uchar*) lr.pBits + ( letter_old.Dy + j ) * sd.Width * 4 + letter_old.Dx * 4, w * 4 );

        font.ArrXY[ i ][ 0 ] = (FLOAT) cur_x / tex_w;
        font.ArrXY[ i ][ 1 ] = (FLOAT) cur_y / tex_h;
        font.ArrXY[ i ][ 2 ] = (FLOAT) ( cur_x + w + 2 ) / tex_w;
        font.ArrXY[ i ][ 3 ] = (FLOAT) ( cur_y + h + 2 ) / tex_h;
        cur_x += w + 2;
    }

    D3D_HR( image->UnlockRect( 0 ) );
    SAFEREL( image );

    // Create texture
    D3D_HR( D3DXCreateTexture( d3dDevice, tex_w, tex_h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &font.FontTex ) );
    D3D_HR( font.FontTex->LockRect( 0, &lr, NULL, 0 ) );
    memcpy( lr.pBits, data, tex_w * tex_h * 4 );
    WriteContour8( (uint*) data, tex_w, lr, tex_w, tex_h, D3DCOLOR_ARGB( 0xFF, 0, 0, 0 ) ); // Create border
    D3D_HR( font.FontTex->UnlockRect( 0 ) );

    // Create bordered texture
    D3D_HR( D3DXCreateTexture( d3dDevice, tex_w, tex_h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &font.FontTexBordered ) );
    D3D_HR( font.FontTexBordered->LockRect( 0, &lr, NULL, 0 ) );
    memcpy( lr.pBits, data, tex_w * tex_h * 4 );
    D3D_HR( font.FontTexBordered->UnlockRect( 0 ) );
    delete[] data;

    // Register
    if( index >= (int) Fonts.size() )
        Fonts.resize( index + 1 );
    SAFEDEL( Fonts[ index ] );
    Fonts[ index ] = new Font( font );
    return true;
}

bool SpriteManager::LoadFontAAF( int index, const char* font_name, int size_mod )
{
    int tex_w = 256 * size_mod;
    int tex_h = 256 * size_mod;

    // Read file in buffer
    if( index < 0 )
    {
        WriteLogF( _FUNC_, " - Invalid index.\n" );
        return false;
    }

    Font        font;
    FileManager fm;

    if( !fm.LoadFile( Str::FormatBuf( "%s.aaf", font_name ), PT_FONTS ) )
    {
        WriteLogF( _FUNC_, " - File<%s> not found.\n", Str::FormatBuf( "%s.aaf", font_name ) );
        return false;
    }

    // Check signature
    uint sign = fm.GetBEUInt();
    if( sign != MAKEFOURCC( 'F', 'F', 'A', 'A' ) )
    {
        WriteLogF( _FUNC_, " - Signature AAFF not found.\n" );
        return false;
    }

    // Read params
    // Максимальная высота изображения символа, включая надстрочные и подстрочные элементы.
    font.MaxLettHeight = fm.GetBEUShort();
    // Горизонтальный зазор.
    // Зазор (в пикселах) между соседними изображениями символов.
    int empty_hor = fm.GetBEUShort();
    // Ширина пробела.
    // Ширина символа 'Пробел'.
    font.SpaceWidth = fm.GetBEUShort();
    // Вертикальный зазор.
    // Зазор (в пикселах) между двумя строками символов.
    font.EmptyVer = fm.GetBEUShort();

    // Write font image
    const uint pix_light[ 9 ] = { 0x22808080, 0x44808080, 0x66808080, 0x88808080, 0xAA808080, 0xDD808080, 0xFF808080, 0xFF808080, 0xFF808080 };
    uchar*     data = new uchar[ tex_w * tex_h * 4 ];
    if( !data )
    {
        WriteLogF( _FUNC_, " - Data allocation fail.\n" );
        return false;
    }
    memzero( data, tex_w * tex_h * 4 );
    uchar* begin_buf = fm.GetBuf();
    int    cur_x = 0;
    int    cur_y = 0;

    for( int i = 0; i < 256; i++ )
    {
        Letter& l = font.Letters[ i ];

        l.W = fm.GetBEUShort();
        l.H = fm.GetBEUShort();
        uint offs = fm.GetBEUInt();
        l.OffsH = -( font.MaxLettHeight - l.H );

        if( cur_x + l.W + 2 >= tex_w )
        {
            cur_x = 0;
            cur_y += font.MaxLettHeight + 2;
            if( cur_y + font.MaxLettHeight + 2 >= tex_h )
            {
                delete[] data;
                // WriteLog("<%s> growed to %d\n",font_name,size_mod*2);
                return LoadFontAAF( index, font_name, size_mod * 2 );
            }
        }

        uchar* pix = &begin_buf[ offs + 0x080C ];

        for( int h = 0; h < l.H; h++ )
        {
            uint* cur_data = (uint*) ( data + ( cur_y + h + 1 ) * tex_w * 4 + ( cur_x + 1 ) * 4 );

            for( int w = 0; w < l.W; w++, pix++, cur_data++ )
            {
                int val = *pix;
                if( val > 9 )
                    val = 0;
                if( !val )
                    continue;
                *cur_data = pix_light[ val - 1 ];
            }
        }

        l.XAdvance = l.W + empty_hor;

        font.ArrXY[ i ][ 0 ] = (FLOAT) cur_x / tex_w;
        font.ArrXY[ i ][ 1 ] = (FLOAT) cur_y / tex_h;
        font.ArrXY[ i ][ 2 ] = (FLOAT) ( cur_x + int(l.W) + 2 ) / tex_w;
        font.ArrXY[ i ][ 3 ] = (FLOAT) ( cur_y + int(l.H) + 2 ) / tex_h;
        cur_x += l.W + 2;
    }

    // Create texture
    D3D_HR( D3DXCreateTexture( d3dDevice, tex_w, tex_h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &font.FontTex ) );
    D3DLOCKED_RECT lr;
    D3D_HR( font.FontTex->LockRect( 0, &lr, NULL, 0 ) );
    memcpy( lr.pBits, data, tex_w * tex_h * 4 );
    WriteContour8( (uint*) data, tex_w, lr, tex_w, tex_h, D3DCOLOR_ARGB( 0xFF, 0, 0, 0 ) ); // Create border
    D3D_HR( font.FontTex->UnlockRect( 0 ) );

    // Create bordered texture
    D3D_HR( D3DXCreateTexture( d3dDevice, tex_w, tex_h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &font.FontTexBordered ) );
    D3D_HR( font.FontTexBordered->LockRect( 0, &lr, NULL, 0 ) );
    memcpy( lr.pBits, data, tex_w * tex_h * 4 );
    D3D_HR( font.FontTexBordered->UnlockRect( 0 ) );
    delete[] data;

    // Register
    if( index >= (int) Fonts.size() )
        Fonts.resize( index + 1 );
    SAFEDEL( Fonts[ index ] );
    Fonts[ index ] = new Font( font );
    return true;
}

bool SpriteManager::LoadFontBMF( int index, const char* font_name )
{
    if( index < 0 )
    {
        WriteLogF( _FUNC_, " - Invalid index.\n" );
        return false;
    }

    Font        font;
    FileManager fm;
    FileManager fm_tex;

    if( !fm.LoadFile( Str::FormatBuf( "%s.fnt", font_name ), PT_FONTS ) )
    {
        WriteLogF( _FUNC_, " - Font file<%s> not found.\n", Str::FormatBuf( "%s.fnt", font_name ) );
        return false;
    }

    uint signature = fm.GetLEUInt();
    if( signature != MAKEFOURCC( 'B', 'M', 'F', 3 ) )
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
    font.MaxLettHeight = base_height;

    uint tex_w = fm.GetLEUShort();
    uint tex_h = fm.GetLEUShort();

    if( fm.GetLEUShort() != 1 )
    {
        WriteLogF( _FUNC_, " - Texture for font<%s> must be single.\n", font_name );
        return false;
    }

    // Pages
    fm.SetCurPos( next_block );
    block_len = fm.GetLEUInt();
    next_block = block_len + fm.GetCurPos() + 1;

    char texture_name[ MAX_FOPATH ] = { 0 };
    fm.GetStr( texture_name );

    if( !fm_tex.LoadFile( texture_name, PT_FONTS ) )
    {
        WriteLogF( _FUNC_, " - Texture file<%s> not found.\n", texture_name );
        return false;
    }

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
        let.W = w - 2;
        let.H = h - 2;
        let.OffsW = -ox;
        let.OffsH = -oy + ( line_height - base_height );
        let.XAdvance = xa + 1;

        // Texture coordinates
        font.ArrXY[ id ][ 0 ] = (FLOAT) x / tex_w;
        font.ArrXY[ id ][ 1 ] = (FLOAT) y / tex_h;
        font.ArrXY[ id ][ 2 ] = (FLOAT) ( x + w ) / tex_w;
        font.ArrXY[ id ][ 3 ] = (FLOAT) ( y + h ) / tex_h;
    }

    font.EmptyVer = 1;
    font.SpaceWidth = font.Letters[ ' ' ].XAdvance;

    // Create texture
    D3D_HR( D3DXCreateTextureFromFileInMemoryEx( d3dDevice, fm_tex.GetBuf(), fm_tex.GetFsize(), D3DX_DEFAULT, D3DX_DEFAULT, 1, 0,
                                                 D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, D3DCOLOR_ARGB( 255, 0, 0, 0 ), NULL, NULL, &font.FontTex ) );

    // Create bordered texture
    D3D_HR( D3DXCreateTexture( d3dDevice, tex_w, tex_h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &font.FontTexBordered ) );

    D3DLOCKED_RECT lr, lrb;
    D3D_HR( font.FontTex->LockRect( 0, &lr, NULL, 0 ) );
    D3D_HR( font.FontTexBordered->LockRect( 0, &lrb, NULL, 0 ) );

    for( uint y = 0; y < tex_h; y++ )
        for( uint x = 0; x < tex_w; x++ )
            if( SURF_POINT( lr, x, y ) & 0xFF000000 )
                SURF_POINT( lr, x, y ) = ( SURF_POINT( lr, x, y ) & 0xFF000000 ) | ( 0x7F7F7F );
            else
                SURF_POINT( lr, x, y ) = 0;

    memcpy( lrb.pBits, lr.pBits, lr.Pitch * tex_h );

    for( uint y = 0; y < tex_h; y++ )
        for( uint x = 0; x < tex_w; x++ )
            if( SURF_POINT( lr, x, y ) )
                for( int xx = -1; xx <= 1; xx++ )
                    for( int yy = -1; yy <= 1; yy++ )
                        if( !SURF_POINT( lrb, x + xx, y + yy ) )
                            SURF_POINT( lrb, x + xx, y + yy ) = 0xFF000000;

    D3D_HR( font.FontTexBordered->UnlockRect( 0 ) );
    D3D_HR( font.FontTex->UnlockRect( 0 ) );

    // Register
    if( index >= (int) Fonts.size() )
        Fonts.resize( index + 1 );
    SAFEDEL( Fonts[ index ] );
    Fonts[ index ] = new Font( font );
    return true;
}

void FormatText( FontFormatInfo& fi, int fmt_type )
{
    char*    str = fi.PStr;
    uint     flags = fi.Flags;
    Font*    font = fi.CurFont;
    INTRECT& r = fi.Rect;
    int&     curx = fi.CurX;
    int&     cury = fi.CurY;
    uint&    offs_col = fi.OffsColDots;

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
                cury += font->MaxLettHeight + font->EmptyVer;
                if( cury + font->MaxLettHeight > r.B && !fi.LinesInRect )
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
            cury += font->MaxLettHeight + font->EmptyVer;
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
        cury = r.T + ( r.H() - fi.LinesAll * font->MaxLettHeight - ( fi.LinesAll - 1 ) * font->EmptyVer ) / 2 + 1;
    else if( FLAG( flags, FT_BOTTOM ) )
        cury = r.B - ( fi.LinesAll * font->MaxLettHeight + ( fi.LinesAll - 1 ) * font->EmptyVer );
}

bool SpriteManager::DrawStr( INTRECT& r, const char* str, uint flags, uint col /* = 0 */, int num_font /* = -1 */ )
{
    // Check
    if( !str || !str[ 0 ] )
        return false;

    // Get font
    Font* font = GetFont( num_font );
    if( !font )
        return false;

    // FormatBuf
    if( !col && DefFontColor )
        col = DefFontColor;

    static FontFormatInfo fi;
    fi.Init( font, flags, r, str );
    fi.DefColor = col;
    FormatText( fi, FORMAT_TYPE_DRAW );
    if( fi.IsError )
        return false;

    char* str_ = fi.PStr;
    uint  offs_col = fi.OffsColDots;
    int   curx = fi.CurX;
    int   cury = fi.CurY;
    int   curstr = 0;

    if( curSprCnt )
        Flush();

    if( !FLAG( flags, FT_NO_COLORIZE ) )
    {
        for( int i = offs_col; i >= 0; i-- )
        {
            if( fi.ColorDots[ i ] )
            {
                if( fi.ColorDots[ i ] & 0xFF000000 )
                    col = fi.ColorDots[ i ];                                          // With alpha
                else
                    col = ( col & 0xFF000000 ) | ( fi.ColorDots[ i ] & 0x00FFFFFF );  // Still old alpha
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
                    col = new_color;                                          // With alpha
                else
                    col = ( col & 0xFF000000 ) | ( new_color & 0x00FFFFFF );  // Still old alpha
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
            cury += font->MaxLettHeight + font->EmptyVer;
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
            int   x = curx - l.OffsW - 1;
            int   y = cury - l.OffsH - 1;
            int   w = l.W + 2;
            int   h = l.H + 2;

            FLOAT x1 = font->ArrXY[ (uchar) str_[ i ] ][ 0 ];
            FLOAT y1 = font->ArrXY[ (uchar) str_[ i ] ][ 1 ];
            FLOAT x2 = font->ArrXY[ (uchar) str_[ i ] ][ 2 ];
            FLOAT y2 = font->ArrXY[ (uchar) str_[ i ] ][ 3 ];

            waitBuf[ mulpos ].x = x - 0.5f;
            waitBuf[ mulpos ].y = y + h - 0.5f;
            waitBuf[ mulpos ].tu = x1;
            waitBuf[ mulpos ].tv = y2;
            waitBuf[ mulpos++ ].Diffuse = col;

            waitBuf[ mulpos ].x = x - 0.5f;
            waitBuf[ mulpos ].y = y - 0.5f;
            waitBuf[ mulpos ].tu = x1;
            waitBuf[ mulpos ].tv = y1;
            waitBuf[ mulpos++ ].Diffuse = col;

            waitBuf[ mulpos ].x = x + w - 0.5f;
            waitBuf[ mulpos ].y = y - 0.5f;
            waitBuf[ mulpos ].tu = x2;
            waitBuf[ mulpos ].tv = y1;
            waitBuf[ mulpos++ ].Diffuse = col;

            waitBuf[ mulpos ].x = x + w - 0.5f;
            waitBuf[ mulpos ].y = y + h - 0.5f;
            waitBuf[ mulpos ].tu = x2;
            waitBuf[ mulpos ].tv = y2;
            waitBuf[ mulpos ].Diffuse = col;

            if( ++curSprCnt == flushSprCnt )
            {
                dipQueue.push_back( DipData( FLAG( flags, FT_BORDERED ) ? font->FontTexBordered : font->FontTex, font->Effect ) );
                dipQueue.back().SprCount = curSprCnt;
                Flush();
            }

            curx += l.XAdvance;
            variable_space = true;
        }
    }

    if( curSprCnt )
    {
        dipQueue.push_back( DipData( FLAG( flags, FT_BORDERED ) ? font->FontTexBordered : font->FontTex, font->Effect ) );
        dipQueue.back().SprCount = curSprCnt;
        Flush();
    }

    return true;
}

int SpriteManager::GetLinesCount( int width, int height, const char* str, int num_font /* = -1 */ )
{
    Font* font = GetFont( num_font );
    if( !font )
        return 0;

    if( !str )
        return height / ( font->MaxLettHeight + font->EmptyVer );

    static FontFormatInfo fi;
    fi.Init( font, 0, INTRECT( 0, 0, width ? width : modeWidth, height ? height : modeHeight ), str );
    FormatText( fi, FORMAT_TYPE_LCOUNT );
    if( fi.IsError )
        return 0;
    return fi.LinesInRect;
}

int SpriteManager::GetLinesHeight( int width, int height, const char* str, int num_font /* = -1 */ )
{
    Font* font = GetFont( num_font );
    if( !font )
        return 0;
    int cnt = GetLinesCount( width, height, str, num_font );
    if( cnt <= 0 )
        return 0;
    return cnt * font->MaxLettHeight + ( cnt - 1 ) * font->EmptyVer;
}

int SpriteManager::GetLineHeight( int num_font /* = -1 */ )
{
    Font* font = GetFont( num_font );
    if( !font )
        return 0;
    return font->MaxLettHeight;
}

void SpriteManager::GetTextInfo( int width, int height, const char* str, int num_font, int flags, int& tw, int& th, int& lines )
{
    tw = th = lines = 0;
    if( width <= 0 )
        width = GameOpt.ScreenWidth;
    if( height <= 0 )
        height = GameOpt.ScreenHeight;

    Font* font = GetFont( num_font );
    if( !font )
        return;

    static FontFormatInfo fi;
    fi.Init( font, flags, INTRECT( 0, 0, width, height ), str );
    FormatText( fi, FORMAT_TYPE_LCOUNT );
    if( fi.IsError )
        return;

    lines = fi.LinesInRect;
    th = fi.LinesInRect * font->MaxLettHeight + ( fi.LinesInRect - 1 ) * font->EmptyVer;
    tw = fi.MaxCurX;
}

int SpriteManager::SplitLines( INTRECT& r, const char* cstr, int num_font, StrVec& str_vec )
{
    // Check & Prepare
    str_vec.clear();
    if( !cstr || !cstr[ 0 ] )
        return 0;

    // Get font
    Font* font = GetFont( num_font );
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
