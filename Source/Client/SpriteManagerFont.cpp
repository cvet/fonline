#include "EffectManager.h"
#include "Log.h"
#include "SpriteManager.h"
#include "StringUtils.h"
#include "Testing.h"

#define FONT_BUF_LEN (0x5000)
#define FONT_MAX_LINES (1000)
#define FORMAT_TYPE_DRAW (0)
#define FORMAT_TYPE_SPLIT (1)
#define FORMAT_TYPE_LCOUNT (2)

struct Letter
{
    short PosX;
    short PosY;
    short W;
    short H;
    short OffsX;
    short OffsY;
    short XAdvance;
    RectF TexUV;
    RectF TexBorderedUV;

    Letter() : PosX(0), PosY(0), W(0), H(0), OffsX(0), OffsY(0), XAdvance(0) {};
};
typedef map<uint, Letter> LetterMap;
typedef map<uint, Letter>::iterator LetterMapIt;

struct FontData
{
    bool Builded;

    Texture* FontTex;
    Texture* FontTexBordered;

    LetterMap Letters;
    int SpaceWidth;
    int LineHeight;
    int YAdvance;
    Effect* DrawEffect;

    AnyFrames* ImageNormal;
    AnyFrames* ImageBordered;
    bool MakeGray;

    FontData(Effect* effect)
    {
        Builded = false;
        FontTex = nullptr;
        FontTexBordered = nullptr;
        SpaceWidth = 0;
        LineHeight = 0;
        YAdvance = 0;
        DrawEffect = effect;
        ImageNormal = nullptr;
        ImageBordered = nullptr;
        MakeGray = false;
    }
};
typedef vector<FontData*> FontDataVec;

FontDataVec Fonts;

int DefFontIndex = -1;
uint DefFontColor = 0;

FontData* GetFont(int num)
{
    if (num < 0)
        num = DefFontIndex;
    if (num < 0 || num >= (int)Fonts.size())
        return nullptr;
    return Fonts[num];
}

struct FontFormatInfo
{
    FontData* CurFont;
    uint Flags;
    Rect Region;
    char Str[FONT_BUF_LEN];
    char* PStr;
    uint LinesAll;
    uint LinesInRect;
    int CurX, CurY, MaxCurX;
    uint ColorDots[FONT_BUF_LEN];
    short LineWidth[FONT_MAX_LINES];
    ushort LineSpaceWidth[FONT_MAX_LINES];
    uint OffsColDots;
    uint DefColor;
    StrVec* StrLines;
    bool IsError;

    void Init(FontData* font, uint flags, const Rect& region, const string& str_in)
    {
        CurFont = font;
        Flags = flags;
        LinesAll = 1;
        LinesInRect = 0;
        IsError = false;
        CurX = CurY = MaxCurX = 0;
        Region = region;
        memzero(ColorDots, sizeof(ColorDots));
        memzero(LineWidth, sizeof(LineWidth));
        memzero(LineSpaceWidth, sizeof(LineSpaceWidth));
        OffsColDots = 0;
        Str::Copy(Str, str_in.c_str());
        PStr = Str;
        DefColor = COLOR_TEXT;
        StrLines = nullptr;
    }

    FontFormatInfo& operator=(const FontFormatInfo& _fi)
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
        memcpy(Str, _fi.Str, sizeof(Str));
        memcpy(ColorDots, _fi.ColorDots, sizeof(ColorDots));
        memcpy(LineWidth, _fi.LineWidth, sizeof(LineWidth));
        memcpy(LineSpaceWidth, _fi.LineSpaceWidth, sizeof(LineSpaceWidth));
        OffsColDots = _fi.OffsColDots;
        DefColor = _fi.DefColor;
        PStr = Str;
        StrLines = _fi.StrLines;
        return *this;
    }
};

void SpriteManager::ClearFonts()
{
    for (size_t i = 0; i < Fonts.size(); i++)
        SAFEDEL(Fonts[i]);
    Fonts.clear();
}

void SpriteManager::SetDefaultFont(int index, uint color)
{
    DefFontIndex = index;
    DefFontColor = color;
}

void SpriteManager::SetFontEffect(int index, Effect* effect)
{
    FontData* font = GetFont(index);
    if (font)
        font->DrawEffect = (effect ? effect : effectMngr.Effects.Font);
}

void SpriteManager::BuildFonts()
{
    for (size_t i = 0; i < Fonts.size(); i++)
        if (Fonts[i] && !Fonts[i]->Builded)
            BuildFont((int)i);
}

void SpriteManager::BuildFont(int index)
{
    FontData& font = *Fonts[index];
    font.Builded = true;

    // Fix texture coordinates
    SpriteInfo* si = GetSpriteInfo(font.ImageNormal->GetSprId(0));
    float tex_w = (float)si->Atlas->Width;
    float tex_h = (float)si->Atlas->Height;
    float image_x = tex_w * si->SprRect.L;
    float image_y = tex_h * si->SprRect.T;
    int max_h = 0;
    for (LetterMapIt it = font.Letters.begin(), end = font.Letters.end(); it != end; ++it)
    {
        Letter& l = it->second;
        float x = (float)l.PosX;
        float y = (float)l.PosY;
        float w = (float)l.W;
        float h = (float)l.H;
        l.TexUV[0] = (image_x + x - 1.0f) / tex_w;
        l.TexUV[1] = (image_y + y - 1.0f) / tex_h;
        l.TexUV[2] = (image_x + x + w + 1.0f) / tex_w;
        l.TexUV[3] = (image_y + y + h + 1.0f) / tex_h;
        if (l.H > max_h)
            max_h = l.H;
    }

    // Fill data
    font.FontTex = si->Atlas->TextureOwner;
    if (font.LineHeight == 0)
        font.LineHeight = max_h;
    if (font.Letters.count(' '))
        font.SpaceWidth = font.Letters[' '].XAdvance;

    SpriteInfo* si_bordered = (font.ImageBordered ? GetSpriteInfo(font.ImageBordered->GetSprId(0)) : nullptr);
    font.FontTexBordered = (si_bordered ? si_bordered->Atlas->TextureOwner : nullptr);

    uint normal_ox = (uint)(tex_w * si->SprRect.L);
    uint normal_oy = (uint)(tex_h * si->SprRect.T);
    uint bordered_ox = (si_bordered ? (uint)((float)si_bordered->Atlas->Width * si_bordered->SprRect.L) : 0);
    uint bordered_oy = (si_bordered ? (uint)((float)si_bordered->Atlas->Height * si_bordered->SprRect.T) : 0);

// Read texture data
#define PIXEL_AT(data, width, x, y) (*((uint*)data + y * width + x))

    PushRenderTarget(si->Atlas->RT);
    uint* data_normal = new uint[si->Width * si->Height];
    GL(glReadPixels(normal_ox, normal_oy, si->Width, si->Height, GL_RGBA, GL_UNSIGNED_BYTE, data_normal));
    PopRenderTarget();

    uint* data_bordered = nullptr;
    if (si_bordered)
    {
        PushRenderTarget(si_bordered->Atlas->RT);
        data_bordered = new uint[si_bordered->Width * si_bordered->Height];
        GL(glReadPixels(bordered_ox, bordered_oy, si_bordered->Width, si_bordered->Height, GL_RGBA, GL_UNSIGNED_BYTE,
            data_bordered));
        PopRenderTarget();
    }

    // Normalize color to gray
    if (font.MakeGray)
    {
        for (uint y = 0; y < (uint)si->Height; y++)
        {
            for (uint x = 0; x < (uint)si->Width; x++)
            {
                uchar a = ((uchar*)&PIXEL_AT(data_normal, si->Width, x, y))[3];
                if (a)
                {
                    PIXEL_AT(data_normal, si->Width, x, y) = COLOR_RGBA(a, 128, 128, 128);
                    if (si_bordered)
                        PIXEL_AT(data_bordered, si_bordered->Width, x, y) = COLOR_RGBA(a, 128, 128, 128);
                }
                else
                {
                    PIXEL_AT(data_normal, si->Width, x, y) = COLOR_RGBA(0, 0, 0, 0);
                    if (si_bordered)
                        PIXEL_AT(data_bordered, si_bordered->Width, x, y) = COLOR_RGBA(0, 0, 0, 0);
                }
            }
        }
        Rect r = Rect(normal_ox, normal_oy, normal_ox + si->Width - 1, normal_oy + si->Height - 1);
        si->Atlas->TextureOwner->UpdateRegion(r, (uchar*)data_normal);
    }

    // Fill border
    if (si_bordered)
    {
        for (uint y = 1; y < (uint)si_bordered->Height - 2; y++)
        {
            for (uint x = 1; x < (uint)si_bordered->Width - 2; x++)
            {
                if (PIXEL_AT(data_normal, si->Width, x, y))
                {
                    for (int xx = -1; xx <= 1; xx++)
                    {
                        for (int yy = -1; yy <= 1; yy++)
                        {
                            uint ox = x + xx;
                            uint oy = y + yy;
                            if (!PIXEL_AT(data_bordered, si_bordered->Width, ox, oy))
                                PIXEL_AT(data_bordered, si_bordered->Width, ox, oy) = COLOR_RGB(0, 0, 0);
                        }
                    }
                }
            }
        }
        Rect r_bordered =
            Rect(bordered_ox, bordered_oy, bordered_ox + si_bordered->Width - 1, bordered_oy + si_bordered->Height - 1);
        si_bordered->Atlas->TextureOwner->UpdateRegion(r_bordered, (uchar*)data_bordered);

        // Fix texture coordinates on bordered texture
        tex_w = (float)si_bordered->Atlas->Width;
        tex_h = (float)si_bordered->Atlas->Height;
        image_x = tex_w * si_bordered->SprRect.L;
        image_y = tex_h * si_bordered->SprRect.T;
        for (LetterMapIt it = font.Letters.begin(), end = font.Letters.end(); it != end; ++it)
        {
            Letter& l = it->second;
            float x = (float)l.PosX;
            float y = (float)l.PosY;
            float w = (float)l.W;
            float h = (float)l.H;
            l.TexBorderedUV[0] = (image_x + x - 1.0f) / tex_w;
            l.TexBorderedUV[1] = (image_y + y - 1.0f) / tex_h;
            l.TexBorderedUV[2] = (image_x + x + w + 1.0f) / tex_w;
            l.TexBorderedUV[3] = (image_y + y + h + 1.0f) / tex_h;
        }
    }

// Clean up
#undef PIXEL_AT
    SAFEDELA(data_normal);
    SAFEDELA(data_bordered);
}

bool SpriteManager::LoadFontFO(int index, const string& font_name, bool not_bordered, bool skip_if_loaded /* = true */)
{
    // Skip if loaded
    if (skip_if_loaded && index < (int)Fonts.size() && Fonts[index])
        return true;

    // Load font data
    string fname = _str("Fonts/{}.fofnt", font_name);
    File file = fileMngr.ReadFile(fname);
    if (!file)
    {
        WriteLog("File '{}' not found.\n", fname);
        return false;
    }

    FontData font(effectMngr.Effects.Font);
    string image_name;

    // Parse data
    istringstream str((char*)file.GetBuf());
    string key;
    string letter_buf;
    Letter* cur_letter = nullptr;
    int version = -1;
    while (!str.eof() && !str.fail())
    {
        // Get key
        str >> key;

        // Cut off comments
        size_t comment = key.find('#');
        if (comment != string::npos)
            key.erase(comment);
        comment = key.find(';');
        if (comment != string::npos)
            key.erase(comment);

        // Check version
        if (version == -1)
        {
            if (key != "Version")
            {
                WriteLog("Font '{}' 'Version' signature not found (used deprecated format of 'fofnt').\n", fname);
                return false;
            }
            str >> version;
            if (version > 2)
            {
                WriteLog("Font '{}' version {} not supported (try update client).\n", fname, version);
                return false;
            }
            continue;
        }

        // Get value
        if (key == "Image")
        {
            str >> image_name;
        }
        else if (key == "LineHeight")
        {
            str >> font.LineHeight;
        }
        else if (key == "YAdvance")
        {
            str >> font.YAdvance;
        }
        else if (key == "End")
        {
            break;
        }
        else if (key == "Letter")
        {
            std::getline(str, letter_buf, '\n');
            size_t utf8_letter_begin = letter_buf.find('\'');
            if (utf8_letter_begin == string::npos)
            {
                WriteLog("Font '{}' invalid letter specification.\n", fname);
                return false;
            }
            utf8_letter_begin++;

            uint letter_len;
            uint letter = utf8::Decode(letter_buf.c_str() + utf8_letter_begin, &letter_len);
            if (!utf8::IsValid(letter))
            {
                WriteLog("Font '{}' invalid UTF8 letter at  '{}'.\n", fname, letter_buf);
                return false;
            }

            cur_letter = &font.Letters[letter];
        }

        if (!cur_letter)
            continue;

        if (key == "PositionX")
            str >> cur_letter->PosX;
        else if (key == "PositionY")
            str >> cur_letter->PosY;
        else if (key == "Width")
            str >> cur_letter->W;
        else if (key == "Height")
            str >> cur_letter->H;
        else if (key == "OffsetX")
            str >> cur_letter->OffsX;
        else if (key == "OffsetY")
            str >> cur_letter->OffsY;
        else if (key == "XAdvance")
            str >> cur_letter->XAdvance;
    }

    bool make_gray = false;
    if (image_name.back() == '*')
    {
        make_gray = true;
        image_name = image_name.substr(0, image_name.size() - 1);
    }
    font.MakeGray = make_gray;

    // Load image
    image_name.insert(0, "Fonts/");
    AnyFrames* image_normal = LoadAnimation(image_name);
    if (!image_normal)
    {
        WriteLog("Image file '{}' not found.\n", image_name);
        return false;
    }
    font.ImageNormal = image_normal;

    // Create bordered instance
    if (!not_bordered)
    {
        AnyFrames* image_bordered = LoadAnimation(image_name);
        if (!image_bordered)
        {
            WriteLog("Can't load twice file '{}'.\n", image_name);
            return false;
        }
        font.ImageBordered = image_bordered;
    }

    // Register
    if (index >= (int)Fonts.size())
        Fonts.resize(index + 1);
    SAFEDEL(Fonts[index]);
    Fonts[index] = new FontData(font);

    return true;
}

bool SpriteManager::LoadFontBMF(int index, const string& font_name)
{
    if (index < 0)
    {
        WriteLog("Invalid index.\n");
        return false;
    }

    FontData font(effectMngr.Effects.Font);
    File file = fileMngr.ReadFile(_str("Fonts/{}.fnt", font_name));
    if (!file)
    {
        WriteLog("Font file '{}.fnt' not found.\n", font_name);
        return false;
    }

    uint signature = file.GetLEUInt();
    if (signature != MAKEUINT('B', 'M', 'F', 3))
    {
        WriteLog("Invalid signature of font '{}'.\n", font_name);
        return false;
    }

    // Info
    file.GetUChar();
    uint block_len = file.GetLEUInt();
    uint next_block = block_len + file.GetCurPos() + 1;

    file.GoForward(7);
    if (file.GetUChar() != 1 || file.GetUChar() != 1 || file.GetUChar() != 1 || file.GetUChar() != 1)
    {
        WriteLog("Wrong padding in font '{}'.\n", font_name);
        return false;
    }

    // Common
    file.SetCurPos(next_block);
    block_len = file.GetLEUInt();
    next_block = block_len + file.GetCurPos() + 1;

    int line_height = file.GetLEUShort();
    int base_height = file.GetLEUShort();
    file.GetLEUShort(); // Texture width
    file.GetLEUShort(); // Texture height

    if (file.GetLEUShort() != 1)
    {
        WriteLog("Texture for font '{}' must be one.\n", font_name);
        return false;
    }

    // Pages
    file.SetCurPos(next_block);
    block_len = file.GetLEUInt();
    next_block = block_len + file.GetCurPos() + 1;

    // Image name
    string image_name = file.GetStrNT();
    image_name.insert(0, "Fonts/");

    // Chars
    file.SetCurPos(next_block);
    int count = file.GetLEUInt() / 20;
    for (int i = 0; i < count; i++)
    {
        // Read data
        uint id = file.GetLEUInt();
        int x = file.GetLEUShort();
        int y = file.GetLEUShort();
        int w = file.GetLEUShort();
        int h = file.GetLEUShort();
        int ox = file.GetLEUShort();
        int oy = file.GetLEUShort();
        int xa = file.GetLEUShort();
        file.GoForward(2);

        // Fill data
        Letter& let = font.Letters[id];
        let.PosX = x + 1;
        let.PosY = y + 1;
        let.W = w - 2;
        let.H = h - 2;
        let.OffsX = -ox;
        let.OffsY = -oy + (line_height - base_height);
        let.XAdvance = xa + 1;
    }

    font.LineHeight = (font.Letters.count('W') ? font.Letters['W'].H : base_height);
    font.YAdvance = font.LineHeight / 2;
    font.MakeGray = true;

    // Load image
    AnyFrames* image_normal = LoadAnimation(image_name);
    if (!image_normal)
    {
        WriteLog("Image file '{}' not found.\n", image_name);
        return false;
    }
    font.ImageNormal = image_normal;

    // Create bordered instance
    AnyFrames* image_bordered = LoadAnimation(image_name);
    if (!image_bordered)
    {
        WriteLog("Can't load twice file '{}'.\n", image_name);
        return false;
    }
    font.ImageBordered = image_bordered;

    // Register
    if (index >= (int)Fonts.size())
        Fonts.resize(index + 1);
    SAFEDEL(Fonts[index]);
    Fonts[index] = new FontData(font);
    return true;
}

static void Str_GoTo(char*& str, char ch, bool skip_char = false)
{
    while (*str && *str != ch)
        ++str;
    if (skip_char && *str)
        ++str;
}

static void Str_EraseInterval(char* str, uint len)
{
    if (!str || !len)
        return;

    char* str2 = str + len;
    while (*str2)
    {
        *str = *str2;
        ++str;
        ++str2;
    }

    *str = 0;
}

static void Str_Insert(char* to, const char* from, uint from_len = 0)
{
    if (!to || !from)
        return;

    if (!from_len)
        from_len = (uint)strlen(from);
    if (!from_len)
        return;

    char* end_to = to;
    while (*end_to)
        ++end_to;

    for (; end_to >= to; --end_to)
        *(end_to + from_len) = *end_to;

    while (from_len--)
    {
        *to = *from;
        ++to;
        ++from;
    }
}

void FormatText(FontFormatInfo& fi, int fmt_type)
{
    char* str = fi.PStr;
    uint flags = fi.Flags;
    FontData* font = fi.CurFont;
    Rect& r = fi.Region;
    bool infinity_w = (r.L == r.R);
    bool infinity_h = (r.T == r.B);
    int curx = 0;
    int cury = 0;
    uint& offs_col = fi.OffsColDots;

    if (fmt_type != FORMAT_TYPE_DRAW && fmt_type != FORMAT_TYPE_LCOUNT && fmt_type != FORMAT_TYPE_SPLIT)
    {
        fi.IsError = true;
        return;
    }

    if (fmt_type == FORMAT_TYPE_SPLIT && !fi.StrLines)
    {
        fi.IsError = true;
        return;
    }

    // Colorize
    uint* dots = nullptr;
    uint d_offs = 0;
    char* str_ = str;
    string buf;
    if (fmt_type == FORMAT_TYPE_DRAW && !FLAG(flags, FT_NO_COLORIZE))
        dots = fi.ColorDots;

    while (*str_)
    {
        char* s0 = str_;
        Str_GoTo(str_, '|');
        char* s1 = str_;
        Str_GoTo(str_, ' ');
        char* s2 = str_;

        // TODO: optimize
        // if(!_str[0] && !*s1) break;
        if (dots)
        {
            size_t d_len = (uint)((size_t)s2 - (size_t)s1) + 1;
            uint d = (uint)strtoul(s1 + 1, nullptr, 0);

            dots[(uint)((size_t)s1 - (size_t)str) - d_offs] = d;
            d_offs += (uint)d_len;
        }

        *s1 = 0;
        buf += s0;

        if (!*str_)
            break;
        str_++;
    }

    Str::Copy(str, FONT_BUF_LEN, buf.c_str());

    // Skip lines
    uint skip_line = (FLAG(flags, FT_SKIPLINES(0)) ? flags >> 16 : 0);
    uint skip_line_end = (FLAG(flags, FT_SKIPLINES_END(0)) ? flags >> 16 : 0);

    // Format
    curx = r.L;
    cury = r.T;
    for (int i = 0, i_advance = 1; str[i]; i += i_advance)
    {
        uint letter_len;
        uint letter = utf8::Decode(&str[i], &letter_len);
        i_advance = letter_len;

        int x_advance;
        switch (letter)
        {
        case '\r':
            continue;
        case ' ':
            x_advance = font->SpaceWidth;
            break;
        case '\t':
            x_advance = font->SpaceWidth * 4;
            break;
        default:
            LetterMapIt it = font->Letters.find(letter);
            if (it != font->Letters.end())
                x_advance = it->second.XAdvance;
            else
                x_advance = 0;
            break;
        }

        if (!infinity_w && curx + x_advance > r.R)
        {
            if (curx > fi.MaxCurX)
                fi.MaxCurX = curx;

            if (fmt_type == FORMAT_TYPE_DRAW && FLAG(flags, FT_NOBREAK))
            {
                str[i] = 0;
                break;
            }
            else if (FLAG(flags, FT_NOBREAK_LINE))
            {
                int j = i;
                for (; str[j]; j++)
                    if (str[j] == '\n')
                        break;

                Str_EraseInterval(&str[i], j - i);
                letter = str[i];
                i_advance = 1;
                if (fmt_type == FORMAT_TYPE_DRAW)
                    for (int k = i, l = MAX_FOTEXT - (j - i); k < l; k++)
                        fi.ColorDots[k] = fi.ColorDots[k + (j - i)];
            }
            else if (str[i] != '\n')
            {
                int j = i;
                for (; j >= 0; j--)
                {
                    if (str[j] == ' ' || str[j] == '\t')
                    {
                        str[j] = '\n';
                        i = j;
                        letter = '\n';
                        i_advance = 1;
                        break;
                    }
                    else if (str[j] == '\n')
                    {
                        j = -1;
                        break;
                    }
                }

                if (j < 0)
                {
                    letter = '\n';
                    i_advance = 1;
                    Str_Insert(&str[i], "\n");
                    if (fmt_type == FORMAT_TYPE_DRAW)
                        for (int k = MAX_FOTEXT - 1; k > i; k--)
                            fi.ColorDots[k] = fi.ColorDots[k - 1];
                }

                if (FLAG(flags, FT_ALIGN) && !skip_line)
                {
                    fi.LineSpaceWidth[fi.LinesAll - 1] = 1;
                    // Erase next first spaces
                    int ii = i + i_advance;
                    for (j = ii;; j++)
                        if (str[j] != ' ')
                            break;
                    if (j > ii)
                    {
                        Str_EraseInterval(&str[ii], j - ii);
                        if (fmt_type == FORMAT_TYPE_DRAW)
                            for (int k = ii, l = MAX_FOTEXT - (j - ii); k < l; k++)
                                fi.ColorDots[k] = fi.ColorDots[k + (j - ii)];
                    }
                }
            }
        }

        switch (letter)
        {
        case '\n':
            if (!skip_line)
            {
                cury += font->LineHeight + font->YAdvance;
                if (!infinity_h && cury + font->LineHeight > r.B && !fi.LinesInRect)
                    fi.LinesInRect = fi.LinesAll;

                if (fmt_type == FORMAT_TYPE_DRAW)
                {
                    if (fi.LinesInRect && !FLAG(flags, FT_UPPER))
                    {
                        // fi.LinesAll++;
                        str[i] = 0;
                        break;
                    }
                }
                else if (fmt_type == FORMAT_TYPE_SPLIT)
                {
                    if (fi.LinesInRect && !(fi.LinesAll % fi.LinesInRect))
                    {
                        str[i] = 0;
                        (*fi.StrLines).push_back(str);
                        str = &str[i + i_advance];
                        i = -i_advance;
                    }
                }

                if (str[i + i_advance])
                    fi.LinesAll++;
            }
            else
            {
                skip_line--;
                Str_EraseInterval(str, i + i_advance);
                offs_col += i + i_advance;
                //	if(fmt_type==FORMAT_TYPE_DRAW)
                //		for(int k=0,l=MAX_FOTEXT-(i+1);k<l;k++) fi.ColorDots[k]=fi.ColorDots[k+i+1];
                i = 0;
                i_advance = 0;
            }

            if (curx > fi.MaxCurX)
                fi.MaxCurX = curx;
            curx = r.L;
            continue;
        case 0:
            break;
        default:
            curx += x_advance;
            continue;
        }

        if (!str[i])
            break;
    }
    if (curx > fi.MaxCurX)
        fi.MaxCurX = curx;

    if (skip_line_end)
    {
        int len = (int)strlen(str);
        for (int i = len - 2; i >= 0; i--)
        {
            if (str[i] == '\n')
            {
                str[i] = 0;
                fi.LinesAll--;
                if (!--skip_line_end)
                    break;
            }
        }

        if (skip_line_end)
        {
            WriteLog("error3\n");
            fi.IsError = true;
            return;
        }
    }

    if (skip_line)
    {
        WriteLog("error4\n");
        fi.IsError = true;
        return;
    }

    if (!fi.LinesInRect)
        fi.LinesInRect = fi.LinesAll;

    if (fi.LinesAll > FONT_MAX_LINES)
    {
        WriteLog("error5 {}\n", fi.LinesAll);
        fi.IsError = true;
        return;
    }

    if (fmt_type == FORMAT_TYPE_SPLIT)
    {
        (*fi.StrLines).push_back(string(str));
        return;
    }
    else if (fmt_type == FORMAT_TYPE_LCOUNT)
    {
        return;
    }

    // Up text
    if (FLAG(flags, FT_UPPER) && fi.LinesAll > fi.LinesInRect)
    {
        uint j = 0;
        uint line_cur = 0;
        uint last_col = 0;
        for (; str[j]; ++j)
        {
            if (str[j] == '\n')
            {
                line_cur++;
                if (line_cur >= (fi.LinesAll - fi.LinesInRect))
                    break;
            }

            if (fi.ColorDots[j])
                last_col = fi.ColorDots[j];
        }

        if (!FLAG(flags, FT_NO_COLORIZE))
        {
            offs_col += j + 1;
            if (last_col && !fi.ColorDots[j + 1])
                fi.ColorDots[j + 1] = last_col;
        }

        str = &str[j + 1];
        fi.PStr = str;

        fi.LinesAll = fi.LinesInRect;
    }

    // Align
    curx = r.L;
    cury = r.T;

    for (uint i = 0; i < fi.LinesAll; i++)
        fi.LineWidth[i] = curx;

    bool can_count = false;
    int spaces = 0;
    int curstr = 0;
    for (int i = 0, i_advance = 1;; i += i_advance)
    {
        uint letter_len;
        uint letter = utf8::Decode(&str[i], &letter_len);
        i_advance = letter_len;

        switch (letter)
        {
        case ' ':
            curx += font->SpaceWidth;
            if (can_count)
                spaces++;
            break;
        case '\t':
            curx += font->SpaceWidth * 4;
            break;
        case 0:
        case '\n':
            fi.LineWidth[curstr] = curx;
            cury += font->LineHeight + font->YAdvance;
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
            if (fi.LineSpaceWidth[curstr] == 1 && spaces > 0 && !infinity_w)
                fi.LineSpaceWidth[curstr] = font->SpaceWidth + (r.R - fi.LineWidth[curstr]) / spaces;
            else
                fi.LineSpaceWidth[curstr] = font->SpaceWidth;

            curstr++;
            can_count = false;
            spaces = 0;
            break;
        case '\r':
            break;
        default:
            LetterMapIt it = font->Letters.find(letter);
            if (it != font->Letters.end())
                curx += it->second.XAdvance;
            // if(curx>fi.LineWidth[curstr]) fi.LineWidth[curstr]=curx;
            can_count = true;
            break;
        }

        if (!str[i])
            break;
    }

    // Initial position
    fi.CurX = r.L;
    fi.CurY = r.T;

    // Align X
    if (FLAG(flags, FT_CENTERX))
        fi.CurX += (r.R - fi.LineWidth[0]) / 2;
    else if (FLAG(flags, FT_CENTERR))
        fi.CurX += r.R - fi.LineWidth[0];
    // Align Y
    if (FLAG(flags, FT_CENTERY))
        fi.CurY = r.T +
            (int)((r.B - r.T + 1) - fi.LinesInRect * font->LineHeight - (fi.LinesInRect - 1) * font->YAdvance) / 2 +
            (FLAG(flags, FT_CENTERY_ENGINE) ? 1 : 0);
    else if (FLAG(flags, FT_BOTTOM))
        fi.CurY = r.B - (int)(fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance);
}

bool SpriteManager::DrawStr(const Rect& r, const string& str, uint flags, uint color /* = 0 */, int num_font /* = -1 */)
{
    // Check
    if (str.empty())
        return false;

    // Get font
    FontData* font = GetFont(num_font);
    if (!font)
        return false;

    // FormatBuf
    if (!color && DefFontColor)
        color = DefFontColor;
    color = COLOR_SWAP_RB(color);

    static FontFormatInfo fi;
    fi.Init(font, flags, r, str.c_str());
    fi.DefColor = color;
    FormatText(fi, FORMAT_TYPE_DRAW);
    if (fi.IsError)
        return false;

    char* str_ = fi.PStr;
    uint offs_col = fi.OffsColDots;
    int curx = fi.CurX;
    int cury = fi.CurY;
    int curstr = 0;
    Texture* texture = (FLAG(flags, FT_BORDERED) && font->FontTexBordered ? font->FontTexBordered : font->FontTex);

    if (curDrawQuad)
        Flush();

    if (!FLAG(flags, FT_NO_COLORIZE))
    {
        for (int i = offs_col; i >= 0; i--)
        {
            if (fi.ColorDots[i])
            {
                if (fi.ColorDots[i] & 0xFF000000)
                    color = fi.ColorDots[i]; // With alpha
                else
                    color = (color & 0xFF000000) | (fi.ColorDots[i] & 0x00FFFFFF); // Still old alpha
                color = COLOR_SWAP_RB(color);
                break;
            }
        }
    }

    bool variable_space = false;
    for (int i = 0, i_advance = 1; str_[i]; i += i_advance)
    {
        if (!FLAG(flags, FT_NO_COLORIZE))
        {
            uint new_color = fi.ColorDots[i + offs_col];
            if (new_color)
            {
                if (new_color & 0xFF000000)
                    color = new_color; // With alpha
                else
                    color = (color & 0xFF000000) | (new_color & 0x00FFFFFF); // Still old alpha
                color = COLOR_SWAP_RB(color);
            }
        }

        uint letter_len;
        uint letter = utf8::Decode(&str_[i], &letter_len);
        i_advance = letter_len;

        switch (letter)
        {
        case ' ':
            curx += (variable_space ? fi.LineSpaceWidth[curstr] : font->SpaceWidth);
            continue;
        case '\t':
            curx += font->SpaceWidth * 4;
            continue;
        case '\n':
            cury += font->LineHeight + font->YAdvance;
            curx = r.L;
            curstr++;
            variable_space = false;
            if (FLAG(flags, FT_CENTERX))
                curx += (r.R - fi.LineWidth[curstr]) / 2;
            else if (FLAG(flags, FT_CENTERR))
                curx += r.R - fi.LineWidth[curstr];
            continue;
        case '\r':
            continue;
        default:
            LetterMapIt it = font->Letters.find(letter);
            if (it == font->Letters.end())
                continue;

            Letter& l = it->second;

            int pos = curDrawQuad * 4;
            int x = curx - l.OffsX - 1;
            int y = cury - l.OffsY - 1;
            int w = l.W + 2;
            int h = l.H + 2;

            RectF& texture_uv = (FLAG(flags, FT_BORDERED) ? l.TexBorderedUV : l.TexUV);
            float x1 = texture_uv[0];
            float y1 = texture_uv[1];
            float x2 = texture_uv[2];
            float y2 = texture_uv[3];

            vBuffer[pos].X = (float)x;
            vBuffer[pos].Y = (float)y + h;
            vBuffer[pos].TU = x1;
            vBuffer[pos].TV = y2;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = (float)x;
            vBuffer[pos].Y = (float)y;
            vBuffer[pos].TU = x1;
            vBuffer[pos].TV = y1;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = (float)x + w;
            vBuffer[pos].Y = (float)y;
            vBuffer[pos].TU = x2;
            vBuffer[pos].TV = y1;
            vBuffer[pos++].Diffuse = color;

            vBuffer[pos].X = (float)x + w;
            vBuffer[pos].Y = (float)y + h;
            vBuffer[pos].TU = x2;
            vBuffer[pos].TV = y2;
            vBuffer[pos].Diffuse = color;

            if (++curDrawQuad == drawQuadCount)
            {
                dipQueue.push_back({texture, font->DrawEffect, 1});
                dipQueue.back().SpritesCount = curDrawQuad;
                Flush();
            }

            curx += l.XAdvance;
            variable_space = true;
        }
    }

    if (curDrawQuad)
    {
        dipQueue.push_back({texture, font->DrawEffect, 1});
        dipQueue.back().SpritesCount = curDrawQuad;
        Flush();
    }

    return true;
}

int SpriteManager::GetLinesCount(int width, int height, const string& str, int num_font /* = -1 */)
{
    if (width <= 0 || height <= 0)
        return 0;

    FontData* font = GetFont(num_font);
    if (!font)
        return 0;

    if (str.empty())
        return height / (font->LineHeight + font->YAdvance);

    static FontFormatInfo fi;
    fi.Init(font, 0, Rect(0, 0, width ? width : GameOpt.ScreenWidth, height ? height : GameOpt.ScreenHeight), str);
    FormatText(fi, FORMAT_TYPE_LCOUNT);
    if (fi.IsError)
        return 0;

    return fi.LinesInRect;
}

int SpriteManager::GetLinesHeight(int width, int height, const string& str, int num_font /* = -1 */)
{
    if (width <= 0 || height <= 0)
        return 0;

    FontData* font = GetFont(num_font);
    if (!font)
        return 0;

    int cnt = GetLinesCount(width, height, str, num_font);
    if (cnt <= 0)
        return 0;

    return cnt * font->LineHeight + (cnt - 1) * font->YAdvance;
}

int SpriteManager::GetLineHeight(int num_font /* = -1 */)
{
    FontData* font = GetFont(num_font);
    if (!font)
        return 0;

    return font->LineHeight;
}

void SpriteManager::GetTextInfo(
    int width, int height, const string& str, int num_font, int flags, int& tw, int& th, int& lines)
{
    tw = th = lines = 0;

    FontData* font = GetFont(num_font);
    if (!font)
        return;

    if (str.empty())
    {
        tw = width;
        th = height;
        lines = height / (font->LineHeight + font->YAdvance);
        return;
    }

    static FontFormatInfo fi;
    fi.Init(font, flags, Rect(0, 0, width, height), str);
    FormatText(fi, FORMAT_TYPE_LCOUNT);
    if (fi.IsError)
        return;

    lines = fi.LinesInRect;
    th = fi.LinesInRect * font->LineHeight + (fi.LinesInRect - 1) * font->YAdvance;
    tw = fi.MaxCurX - fi.Region.L;
}

int SpriteManager::SplitLines(const Rect& r, const string& cstr, int num_font, StrVec& str_vec)
{
    // Check & Prepare
    str_vec.clear();
    if (cstr.empty())
        return 0;

    // Get font
    FontData* font = GetFont(num_font);
    if (!font)
        return 0;
    static FontFormatInfo fi;
    fi.Init(font, 0, r, cstr);
    fi.StrLines = &str_vec;
    FormatText(fi, FORMAT_TYPE_SPLIT);
    if (fi.IsError)
        return 0;
    return (int)str_vec.size();
}

bool SpriteManager::HaveLetter(int num_font, uint letter)
{
    FontData* font = GetFont(num_font);
    if (!font)
        return false;
    return font->Letters.count(letter) > 0;
}
