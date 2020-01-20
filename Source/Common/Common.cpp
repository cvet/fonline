#include "Common.h"
#include "FileSystem.h"
#include "GenericUtils.h"
#include "Log.h"
#include "NetBuffer.h"
#include "Script.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#include "Timer.h"
#include "Version_Include.h"

#include "SDL.h"

// Some checks from AngelScript config
#include "as_config.h"
#ifdef AS_BIG_ENDIAN
#error "Big Endian architectures not supported."
#endif

void InitialSetup(const string& app_name, uint argc, char** argv)
{
    // Exceptions catcher
    CatchExceptions(app_name, FO_VERSION);

    // Timer
    Timer::Init();

    // Settings
    GameOpt.Init(argc, argv);

// Other
#ifdef FONLINE_EDITOR
    Script::SetRunTimeout(0, 0);
#endif
}

// Default randomizer
#ifndef FO_TESTING
static std::mt19937 RandomGenerator(std::random_device {}());
#else
static std::mt19937 RandomGenerator(42);
#endif
int Random(int minimum, int maximum)
{
    return std::uniform_int_distribution {minimum, maximum}(RandomGenerator);
}

// Math stuff
int Procent(int full, int peace)
{
    if (!full)
        return 0;
    int procent = peace * 100 / full;
    return CLAMP(procent, 0, 100);
}

uint NumericalNumber(uint num)
{
    if (num & 1)
        return num * (num / 2 + 1);
    else
        return num * num / 2 + num / 2;
}

uint DistSqrt(int x1, int y1, int x2, int y2)
{
    int dx = x1 - x2;
    int dy = y1 - y2;
    return (uint)sqrt(double(dx * dx + dy * dy));
}

uint DistGame(int x1, int y1, int x2, int y2)
{
    if (GameOpt.MapHexagonal)
    {
        int dx = (x1 > x2 ? x1 - x2 : x2 - x1);
        if (!(x1 & 1))
        {
            if (y2 <= y1)
            {
                int rx = y1 - y2 - dx / 2;
                return dx + (rx > 0 ? rx : 0);
            }
            else
            {
                int rx = y2 - y1 - (dx + 1) / 2;
                return dx + (rx > 0 ? rx : 0);
            }
        }
        else
        {
            if (y2 >= y1)
            {
                int rx = y2 - y1 - dx / 2;
                return dx + (rx > 0 ? rx : 0);
            }
            else
            {
                int rx = y1 - y2 - (dx + 1) / 2;
                return dx + (rx > 0 ? rx : 0);
            }
        }
    }
    else
    {
        int dx = abs(x2 - x1);
        int dy = abs(y2 - y1);
        return MAX(dx, dy);
    }
}

int GetNearDir(int x1, int y1, int x2, int y2)
{
    int dir = 0;

    if (GameOpt.MapHexagonal)
    {
        if (x1 & 1)
        {
            if (x1 > x2 && y1 > y2)
                dir = 0;
            else if (x1 > x2 && y1 == y2)
                dir = 1;
            else if (x1 == x2 && y1 < y2)
                dir = 2;
            else if (x1 < x2 && y1 == y2)
                dir = 3;
            else if (x1 < x2 && y1 > y2)
                dir = 4;
            else if (x1 == x2 && y1 > y2)
                dir = 5;
        }
        else
        {
            if (x1 > x2 && y1 == y2)
                dir = 0;
            else if (x1 > x2 && y1 < y2)
                dir = 1;
            else if (x1 == x2 && y1 < y2)
                dir = 2;
            else if (x1 < x2 && y1 < y2)
                dir = 3;
            else if (x1 < x2 && y1 == y2)
                dir = 4;
            else if (x1 == x2 && y1 > y2)
                dir = 5;
        }
    }
    else
    {
        if (x1 > x2 && y1 == y2)
            dir = 0;
        else if (x1 > x2 && y1 < y2)
            dir = 1;
        else if (x1 == x2 && y1 < y2)
            dir = 2;
        else if (x1 < x2 && y1 < y2)
            dir = 3;
        else if (x1 < x2 && y1 == y2)
            dir = 4;
        else if (x1 < x2 && y1 > y2)
            dir = 5;
        else if (x1 == x2 && y1 > y2)
            dir = 6;
        else if (x1 > x2 && y1 > y2)
            dir = 7;
    }

    return dir;
}

int GetFarDir(int x1, int y1, int x2, int y2)
{
    if (GameOpt.MapHexagonal)
    {
        float hx = (float)x1;
        float hy = (float)y1;
        float tx = (float)x2;
        float ty = (float)y2;
        float nx = 3 * (tx - hx);
        float ny = (ty - hy) * SQRT3T2_FLOAT - (float(x2 & 1) - float(x1 & 1)) * SQRT3_FLOAT;
        float dir = 180.0f + RAD2DEG * atan2f(ny, nx);

        if (dir >= 60.0f && dir < 120.0f)
            return 5;
        if (dir >= 120.0f && dir < 180.0f)
            return 4;
        if (dir >= 180.0f && dir < 240.0f)
            return 3;
        if (dir >= 240.0f && dir < 300.0f)
            return 2;
        if (dir >= 300.0f)
            return 1;
        return 0;
    }
    else
    {
        float dir = 180.0f + RAD2DEG * atan2((float)(x2 - x1), (float)(y2 - y1));

        if (dir >= 22.5f && dir < 67.5f)
            return 7;
        if (dir >= 67.5f && dir < 112.5f)
            return 0;
        if (dir >= 112.5f && dir < 157.5f)
            return 1;
        if (dir >= 157.5f && dir < 202.5f)
            return 2;
        if (dir >= 202.5f && dir < 247.5f)
            return 3;
        if (dir >= 247.5f && dir < 292.5f)
            return 4;
        if (dir >= 292.5f && dir < 337.5f)
            return 5;
        return 6;
    }
}

int GetFarDir(int x1, int y1, int x2, int y2, float offset)
{
    if (GameOpt.MapHexagonal)
    {
        float hx = (float)x1;
        float hy = (float)y1;
        float tx = (float)x2;
        float ty = (float)y2;
        float nx = 3 * (tx - hx);
        float ny = (ty - hy) * SQRT3T2_FLOAT - (float(x2 & 1) - float(x1 & 1)) * SQRT3_FLOAT;
        float dir = 180.0f + RAD2DEG * atan2f(ny, nx) + offset;
        if (dir < 0.0f)
            dir = 360.0f - fmod(-dir, 360.0f);
        else if (dir >= 360.0f)
            dir = fmod(dir, 360.0f);

        if (dir >= 60.0f && dir < 120.0f)
            return 5;
        if (dir >= 120.0f && dir < 180.0f)
            return 4;
        if (dir >= 180.0f && dir < 240.0f)
            return 3;
        if (dir >= 240.0f && dir < 300.0f)
            return 2;
        if (dir >= 300.0f)
            return 1;
        return 0;
    }
    else
    {
        float dir = 180.0f + RAD2DEG * atan2((float)(x2 - x1), (float)(y2 - y1)) + offset;
        if (dir < 0.0f)
            dir = 360.0f - fmod(-dir, 360.0f);
        else if (dir >= 360.0f)
            dir = fmod(dir, 360.0f);

        if (dir >= 22.5f && dir < 67.5f)
            return 7;
        if (dir >= 67.5f && dir < 112.5f)
            return 0;
        if (dir >= 112.5f && dir < 157.5f)
            return 1;
        if (dir >= 157.5f && dir < 202.5f)
            return 2;
        if (dir >= 202.5f && dir < 247.5f)
            return 3;
        if (dir >= 247.5f && dir < 292.5f)
            return 4;
        if (dir >= 292.5f && dir < 337.5f)
            return 5;
        return 6;
    }
}

bool CheckDist(ushort x1, ushort y1, ushort x2, ushort y2, uint dist)
{
    return DistGame(x1, y1, x2, y2) <= dist;
}

int ReverseDir(int dir)
{
    int dirs_count = DIRS_COUNT;
    return (dir + dirs_count / 2) % dirs_count;
}

void GetStepsXY(float& sx, float& sy, int x1, int y1, int x2, int y2)
{
    float dx = (float)abs(x2 - x1);
    float dy = (float)abs(y2 - y1);

    sx = 1.0f;
    sy = 1.0f;

    dx < dy ? sx = dx / dy : sy = dy / dx;

    if (x2 < x1)
        sx = -sx;
    if (y2 < y1)
        sy = -sy;
}

void ChangeStepsXY(float& sx, float& sy, float deq)
{
    float rad = deq * PI_FLOAT / 180.0f;
    sx = sx * cos(rad) - sy * sin(rad);
    sy = sx * sin(rad) + sy * cos(rad);
}

bool MoveHexByDir(ushort& hx, ushort& hy, uchar dir, ushort maxhx, ushort maxhy)
{
    int hx_ = hx;
    int hy_ = hy;
    MoveHexByDirUnsafe(hx_, hy_, dir);

    if (hx_ >= 0 && hx_ < maxhx && hy_ >= 0 && hy_ < maxhy)
    {
        hx = hx_;
        hy = hy_;
        return true;
    }
    return false;
}

void MoveHexByDirUnsafe(int& hx, int& hy, uchar dir)
{
    if (GameOpt.MapHexagonal)
    {
        switch (dir)
        {
        case 0:
            hx--;
            if (!(hx & 1))
                hy--;
            break;
        case 1:
            hx--;
            if (hx & 1)
                hy++;
            break;
        case 2:
            hy++;
            break;
        case 3:
            hx++;
            if (hx & 1)
                hy++;
            break;
        case 4:
            hx++;
            if (!(hx & 1))
                hy--;
            break;
        case 5:
            hy--;
            break;
        default:
            return;
        }
    }
    else
    {
        switch (dir)
        {
        case 0:
            hx--;
            break;
        case 1:
            hx--;
            hy++;
            break;
        case 2:
            hy++;
            break;
        case 3:
            hx++;
            hy++;
            break;
        case 4:
            hx++;
            break;
        case 5:
            hx++;
            hy--;
            break;
        case 6:
            hy--;
            break;
        case 7:
            hx--;
            hy--;
            break;
        default:
            return;
        }
    }
}

bool IntersectCircleLine(int cx, int cy, int radius, int x1, int y1, int x2, int y2)
{
    int x01 = x1 - cx;
    int y01 = y1 - cy;
    int x02 = x2 - cx;
    int y02 = y2 - cy;
    int dx = x02 - x01;
    int dy = y02 - y01;
    int a = dx * dx + dy * dy;
    int b = 2 * (x01 * dx + y01 * dy);
    int c = x01 * x01 + y01 * y01 - radius * radius;
    if (-b < 0)
        return c < 0;
    if (-b < 2 * a)
        return 4 * a * c - b * b < 0;
    return a + b + c < 0;
}

void ShowErrorMessage(const string& message, const string& traceback)
{
#ifndef FO_SERVER_NO_GUI
#if defined(FO_WEB) || defined(FO_ANDROID) || defined(FO_IOS)
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FOnline Error", message.c_str(), nullptr);

#else
    string verb_message = message;
#ifdef FO_WINDOWS
    string most_recent = "most recent call first";
#else
    string most_recent = "most recent call last";
#endif
    if (!traceback.empty())
        verb_message += _str("\n\nTraceback ({}):\n{}", most_recent, traceback);

    SDL_MessageBoxButtonData copy_button;
    SDL_zero(copy_button);
    copy_button.buttonid = 0;
    copy_button.text = "Copy";

    SDL_MessageBoxButtonData close_button;
    SDL_zero(close_button);
    close_button.buttonid = 1;
    close_button.text = "Close";

// Workaround for strange button focus behaviour
#ifdef FO_WINDOWS
    copy_button.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    copy_button.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
#else
    close_button.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    close_button.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
#endif

    const SDL_MessageBoxButtonData buttons[] = {close_button, copy_button};
    SDL_MessageBoxData data;
    SDL_zero(data);
    data.flags = SDL_MESSAGEBOX_ERROR;
    data.title = "FOnline Error";
    data.message = verb_message.c_str();
    data.numbuttons = 2;
    data.buttons = buttons;

    int buttonid;
    while (!SDL_ShowMessageBox(&data, &buttonid) && buttonid == 0)
        SDL_SetClipboardText(verb_message.c_str());
#endif
#endif
}

int ConvertParamValue(const string& str, bool& fail)
{
    if (str.empty())
    {
        WriteLog("Empty parameter value.\n");
        fail = true;
        return false;
    }

    if (str[0] == '@' && str[1])
        return _str(str.substr(1)).toHash();
    if (_str(str).isNumber())
        return _str(str).toInt();
    if (_str(str).compareIgnoreCase("true"))
        return 1;
    if (_str(str).compareIgnoreCase("false"))
        return 0;

    return Script::GetEnumValue(str, fail);
}

// Hex offset
#define HEX_OFFSET_SIZE ((MAX_HEX_OFFSET * MAX_HEX_OFFSET / 2 + MAX_HEX_OFFSET / 2) * DIRS_COUNT)
static int CurHexOffset = 0; // 0 - none, 1 - hexagonal, 2 - square
static short* SXEven = nullptr;
static short* SYEven = nullptr;
static short* SXOdd = nullptr;
static short* SYOdd = nullptr;

void InitializeHexOffsets()
{
    SAFEDELA(SXEven);
    SAFEDELA(SYEven);
    SAFEDELA(SXOdd);
    SAFEDELA(SYOdd);

    if (GameOpt.MapHexagonal)
    {
        CurHexOffset = 1;
        SXEven = new short[HEX_OFFSET_SIZE];
        SYEven = new short[HEX_OFFSET_SIZE];
        SXOdd = new short[HEX_OFFSET_SIZE];
        SYOdd = new short[HEX_OFFSET_SIZE];

        int pos = 0;
        int xe = 0, ye = 0, xo = 1, yo = 0;
        for (int i = 0; i < MAX_HEX_OFFSET; i++)
        {
            MoveHexByDirUnsafe(xe, ye, 0);
            MoveHexByDirUnsafe(xo, yo, 0);

            for (int j = 0; j < 6; j++)
            {
                int dir = (j + 2) % 6;
                for (int k = 0; k < i + 1; k++)
                {
                    SXEven[pos] = xe;
                    SYEven[pos] = ye;
                    SXOdd[pos] = xo - 1;
                    SYOdd[pos] = yo;
                    pos++;
                    MoveHexByDirUnsafe(xe, ye, dir);
                    MoveHexByDirUnsafe(xo, yo, dir);
                }
            }
        }
    }
    else
    {
        CurHexOffset = 2;
        SXEven = SXOdd = new short[HEX_OFFSET_SIZE];
        SYEven = SYOdd = new short[HEX_OFFSET_SIZE];

        int pos = 0;
        int hx = 0, hy = 0;
        for (int i = 0; i < MAX_HEX_OFFSET; i++)
        {
            MoveHexByDirUnsafe(hx, hy, 0);

            for (int j = 0; j < 5; j++)
            {
                int dir = 0, steps = 0;
                switch (j)
                {
                case 0:
                    dir = 2;
                    steps = i + 1;
                    break;
                case 1:
                    dir = 4;
                    steps = (i + 1) * 2;
                    break;
                case 2:
                    dir = 6;
                    steps = (i + 1) * 2;
                    break;
                case 3:
                    dir = 0;
                    steps = (i + 1) * 2;
                    break;
                case 4:
                    dir = 2;
                    steps = i + 1;
                    break;
                default:
                    break;
                }

                for (int k = 0; k < steps; k++)
                {
                    SXEven[pos] = hx;
                    SYEven[pos] = hy;
                    pos++;
                    MoveHexByDirUnsafe(hx, hy, dir);
                }
            }
        }
    }
}

void GetHexOffsets(bool odd, short*& sx, short*& sy)
{
    if (CurHexOffset != (GameOpt.MapHexagonal ? 1 : 2))
        InitializeHexOffsets();
    sx = (odd ? SXOdd : SXEven);
    sy = (odd ? SYOdd : SYEven);
}

void GetHexInterval(int from_hx, int from_hy, int to_hx, int to_hy, int& x, int& y)
{
    if (GameOpt.MapHexagonal)
    {
        int dx = to_hx - from_hx;
        int dy = to_hy - from_hy;
        x = dy * (GameOpt.MapHexWidth / 2) - dx * GameOpt.MapHexWidth;
        y = dy * GameOpt.MapHexLineHeight;
        if (from_hx & 1)
        {
            if (dx > 0)
                dx++;
        }
        else if (dx < 0)
            dx--;
        dx /= 2;
        x += (GameOpt.MapHexWidth / 2) * dx;
        y += GameOpt.MapHexLineHeight * dx;
    }
    else
    {
        int dx = to_hx - from_hx;
        int dy = to_hy - from_hy;
        x = (dy - dx) * GameOpt.MapHexWidth / 2;
        y = (dy + dx) * GameOpt.MapHexLineHeight;
    }
}

#if defined(FONLINE_CLIENT) || defined(FONLINE_EDITOR)
IntVec MainWindowKeyboardEvents;
StrVec MainWindowKeyboardEventsText;
IntVec MainWindowMouseEvents;

uint GetColorDay(int* day_time, uchar* colors, int game_time, int* light)
{
    uchar result[3];
    int color_r[4] = {colors[0], colors[1], colors[2], colors[3]};
    int color_g[4] = {colors[4], colors[5], colors[6], colors[7]};
    int color_b[4] = {colors[8], colors[9], colors[10], colors[11]};

    game_time %= 1440;
    int time, duration;
    if (game_time >= day_time[0] && game_time < day_time[1])
    {
        time = 0;
        game_time -= day_time[0];
        duration = day_time[1] - day_time[0];
    }
    else if (game_time >= day_time[1] && game_time < day_time[2])
    {
        time = 1;
        game_time -= day_time[1];
        duration = day_time[2] - day_time[1];
    }
    else if (game_time >= day_time[2] && game_time < day_time[3])
    {
        time = 2;
        game_time -= day_time[2];
        duration = day_time[3] - day_time[2];
    }
    else
    {
        time = 3;
        if (game_time >= day_time[3])
            game_time -= day_time[3];
        else
            game_time += 1440 - day_time[3];
        duration = (1440 - day_time[3]) + day_time[0];
    }

    if (!duration)
        duration = 1;
    result[0] = color_r[time] + (color_r[time < 3 ? time + 1 : 0] - color_r[time]) * game_time / duration;
    result[1] = color_g[time] + (color_g[time < 3 ? time + 1 : 0] - color_g[time]) * game_time / duration;
    result[2] = color_b[time] + (color_b[time < 3 ? time + 1 : 0] - color_b[time]) * game_time / duration;

    if (light)
    {
        int max_light = (MAX(MAX(MAX(color_r[0], color_r[1]), color_r[2]), color_r[3]) +
                            MAX(MAX(MAX(color_g[0], color_g[1]), color_g[2]), color_g[3]) +
                            MAX(MAX(MAX(color_b[0], color_b[1]), color_b[2]), color_b[3])) /
            3;
        int min_light = (MIN(MIN(MIN(color_r[0], color_r[1]), color_r[2]), color_r[3]) +
                            MIN(MIN(MIN(color_g[0], color_g[1]), color_g[2]), color_g[3]) +
                            MIN(MIN(MIN(color_b[0], color_b[1]), color_b[2]), color_b[3])) /
            3;
        int cur_light = (result[0] + result[1] + result[2]) / 3;
        *light = Procent(max_light - min_light, max_light - cur_light);
        *light = CLAMP(*light, 0, 100);
    }

    return (result[0] << 16) | (result[1] << 8) | (result[2]);
}
#endif

TwoBitMask::TwoBitMask()
{
    memset(this, 0, sizeof(TwoBitMask));
}

TwoBitMask::TwoBitMask(uint width_2bit, uint height_2bit, uchar* ptr)
{
    if (!width_2bit)
        width_2bit = 1;
    if (!height_2bit)
        height_2bit = 1;

    width = width_2bit;
    height = height_2bit;
    widthBytes = width / 4;

    if (width % 4)
        widthBytes++;

    if (ptr)
    {
        isAlloc = false;
        data = ptr;
    }
    else
    {
        isAlloc = true;
        data = new uchar[widthBytes * height];
        Fill(0);
    }
}

TwoBitMask::~TwoBitMask()
{
    if (isAlloc)
        delete[] data;
    data = nullptr;
}

void TwoBitMask::Set2Bit(uint x, uint y, int val)
{
    if (x >= width || y >= height)
        return;

    uchar& b = data[y * widthBytes + x / 4];
    int bit = (x % 4 * 2);

    UNSETFLAG(b, 3 << bit);
    SETFLAG(b, (val & 3) << bit);
}

int TwoBitMask::Get2Bit(uint x, uint y)
{
    if (x >= width || y >= height)
        return 0;

    return (data[y * widthBytes + x / 4] >> (x % 4 * 2)) & 3;
}

void TwoBitMask::Fill(int fill)
{
    memset(data, fill, widthBytes * height);
}

uchar* TwoBitMask::GetData()
{
    return data;
}

struct CmdDef
{
    char Name[20];
    uchar Id;
};

static const CmdDef CmdList[] = {
    {"exit", CMD_EXIT},
    {"myinfo", CMD_MYINFO},
    {"gameinfo", CMD_GAMEINFO},
    {"id", CMD_CRITID},
    {"move", CMD_MOVECRIT},
    {"disconnect", CMD_DISCONCRIT},
    {"toglobal", CMD_TOGLOBAL},
    {"prop", CMD_PROPERTY},
    {"getaccess", CMD_GETACCESS},
    {"additem", CMD_ADDITEM},
    {"additemself", CMD_ADDITEM_SELF},
    {"ais", CMD_ADDITEM_SELF},
    {"addnpc", CMD_ADDNPC},
    {"addloc", CMD_ADDLOCATION},
    {"reloadscripts", CMD_RELOADSCRIPTS},
    {"reloadclientscripts", CMD_RELOAD_CLIENT_SCRIPTS},
    {"rcs", CMD_RELOAD_CLIENT_SCRIPTS},
    {"runscript", CMD_RUNSCRIPT},
    {"run", CMD_RUNSCRIPT},
    {"reloadprotos", CMD_RELOAD_PROTOS},
    {"regenmap", CMD_REGENMAP},
    {"reloaddialogs", CMD_RELOADDIALOGS},
    {"loaddialog", CMD_LOADDIALOG},
    {"reloadtexts", CMD_RELOADTEXTS},
    {"settime", CMD_SETTIME},
    {"ban", CMD_BAN},
    {"deleteself", CMD_DELETE_ACCOUNT},
    {"changepassword", CMD_CHANGE_PASSWORD},
    {"changepass", CMD_CHANGE_PASSWORD},
    {"log", CMD_LOG},
    {"exec", CMD_DEV_EXEC},
    {"func", CMD_DEV_FUNC},
    {"gvar", CMD_DEV_GVAR},
};

bool PackNetCommand(const string& str, NetBuffer* pbuf, std::function<void(const string&)> logcb, const string& name)
{
    string args = _str(str).trim();
    string cmd_str = args;
    size_t space = cmd_str.find(' ');
    if (space != string::npos)
    {
        cmd_str = args.substr(0, space);
        args.erase(0, cmd_str.length());
    }
    istringstream args_str(args);

    uchar cmd = 0;
    for (uint cur_cmd = 0; cur_cmd < sizeof(CmdList) / sizeof(CmdDef); cur_cmd++)
        if (_str(cmd_str).compareIgnoreCase(CmdList[cur_cmd].Name))
            cmd = CmdList[cur_cmd].Id;
    if (!cmd)
        return false;

    uint msg = NETMSG_SEND_COMMAND;
    uint msg_len = sizeof(msg) + sizeof(msg_len) + sizeof(cmd);

    RUNTIME_ASSERT(pbuf);
    NetBuffer& buf = *pbuf;

    switch (cmd)
    {
    case CMD_EXIT: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_MYINFO: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_GAMEINFO: {
        int type;
        if (!(args_str >> type))
        {
            logcb("Invalid arguments. Example: gameinfo type.");
            break;
        }
        msg_len += sizeof(type);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << type;
    }
    break;
    case CMD_CRITID: {
        string name;
        if (!(args_str >> name))
        {
            logcb("Invalid arguments. Example: id name.");
            break;
        }
        msg_len += NetBuffer::StringLenSize;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << name;
    }
    break;
    case CMD_MOVECRIT: {
        uint crid;
        ushort hex_x;
        ushort hex_y;
        if (!(args_str >> crid >> hex_x >> hex_y))
        {
            logcb("Invalid arguments. Example: move crid hx hy.");
            break;
        }
        msg_len += sizeof(crid) + sizeof(hex_x) + sizeof(hex_y);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
        buf << hex_x;
        buf << hex_y;
    }
    break;
    case CMD_DISCONCRIT: {
        uint crid;
        if (!(args_str >> crid))
        {
            logcb("Invalid arguments. Example: disconnect crid.");
            break;
        }
        msg_len += sizeof(crid);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
    }
    break;
    case CMD_TOGLOBAL: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_PROPERTY: {
        uint crid;
        string property_name;
        int property_value;
        if (!(args_str >> crid >> property_name >> property_value))
        {
            logcb("Invalid arguments. Example: prop crid prop_name value.");
            break;
        }
        msg_len += sizeof(uint) + NetBuffer::StringLenSize + (uint)property_name.length() + sizeof(int);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
        buf << property_name;
        buf << property_value;
    }
    break;
    case CMD_GETACCESS: {
        string name_access;
        string pasw_access;
        if (!(args_str >> name_access >> pasw_access))
        {
            logcb("Invalid arguments. Example: getaccess name password.");
            break;
        }
        name_access = _str(name_access).replace('*', ' ');
        pasw_access = _str(pasw_access).replace('*', ' ');
        msg_len += NetBuffer::StringLenSize * 2 + (uint)(name_access.length() + pasw_access.length());
        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << name_access;
        buf << pasw_access;
    }
    break;
    case CMD_ADDITEM: {
        ushort hex_x;
        ushort hex_y;
        string proto_name;
        uint count;
        if (!(args_str >> hex_x >> hex_y >> proto_name >> count))
        {
            logcb("Invalid arguments. Example: additem hx hy name count.");
            break;
        }
        hash pid = _str(proto_name).toHash();
        msg_len += sizeof(hex_x) + sizeof(hex_y) + sizeof(pid) + sizeof(count);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << hex_x;
        buf << hex_y;
        buf << pid;
        buf << count;
    }
    break;
    case CMD_ADDITEM_SELF: {
        string proto_name;
        uint count;
        if (!(args_str >> proto_name >> count))
        {
            logcb("Invalid arguments. Example: additemself name count.");
            break;
        }
        hash pid = _str(proto_name).toHash();
        msg_len += sizeof(pid) + sizeof(count);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << pid;
        buf << count;
    }
    break;
    case CMD_ADDNPC: {
        ushort hex_x;
        ushort hex_y;
        uchar dir;
        string proto_name;
        if (!(args_str >> hex_x >> hex_y >> dir >> proto_name))
        {
            logcb("Invalid arguments. Example: addnpc hx hy dir name.");
            break;
        }
        hash pid = _str(proto_name).toHash();
        msg_len += sizeof(hex_x) + sizeof(hex_y) + sizeof(dir) + sizeof(pid);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << hex_x;
        buf << hex_y;
        buf << dir;
        buf << pid;
    }
    break;
    case CMD_ADDLOCATION: {
        ushort wx;
        ushort wy;
        string proto_name;
        if (!(args_str >> wx >> wy >> proto_name))
        {
            logcb("Invalid arguments. Example: addloc wx wy name.");
            break;
        }
        hash pid = _str(proto_name).toHash();
        msg_len += sizeof(wx) + sizeof(wy) + sizeof(pid);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << wx;
        buf << wy;
        buf << pid;
    }
    break;
    case CMD_RELOADSCRIPTS: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_RELOAD_CLIENT_SCRIPTS: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_RUNSCRIPT: {
        string func_name;
        uint param0, param1, param2;
        if (!(args_str >> func_name >> param0 >> param1 >> param2))
        {
            logcb("Invalid arguments. Example: runscript module::func param0 param1 param2.");
            break;
        }
        msg_len += NetBuffer::StringLenSize + (uint)func_name.length() + sizeof(uint) * 3;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << func_name;
        buf << param0;
        buf << param1;
        buf << param2;
    }
    break;
    case CMD_RELOAD_PROTOS: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_REGENMAP: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_RELOADDIALOGS: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_LOADDIALOG: {
        string dlg_name;
        if (!(args_str >> dlg_name))
        {
            logcb("Invalid arguments. Example: loaddialog name.");
            break;
        }
        msg_len += NetBuffer::StringLenSize + (uint)dlg_name.length();

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << dlg_name;
    }
    break;
    case CMD_RELOADTEXTS: {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_SETTIME: {
        int multiplier;
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        if (!(args_str >> multiplier >> year >> month >> day >> hour >> minute >> second))
        {
            logcb("Invalid arguments. Example: settime tmul year month day hour minute second.");
            break;
        }
        msg_len += sizeof(multiplier) + sizeof(year) + sizeof(month) + sizeof(day) + sizeof(hour) + sizeof(minute) +
            sizeof(second);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << multiplier;
        buf << year;
        buf << month;
        buf << day;
        buf << hour;
        buf << minute;
        buf << second;
    }
    break;
    case CMD_BAN: {
        string params;
        string name;
        uint ban_hours;
        string info;
        args_str >> params;
        if (!args_str.fail())
            args_str >> name;
        if (!args_str.fail())
            args_str >> ban_hours;
        if (!args_str.fail())
            info = args_str.str();
        if (!_str(params).compareIgnoreCase("add") && !_str(params).compareIgnoreCase("add+") &&
            !_str(params).compareIgnoreCase("delete") && !_str(params).compareIgnoreCase("list"))
        {
            logcb("Invalid arguments. Example: ban [add,add+,delete,list] [user] [hours] [comment].");
            break;
        }
        name = _str(name).replace('*', ' ').trim();
        info = _str(info).replace('$', '*').trim();
        msg_len +=
            NetBuffer::StringLenSize * 3 + (uint)(name.length() + params.length() + info.length()) + sizeof(ban_hours);

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << name;
        buf << params;
        buf << ban_hours;
        buf << info;
    }
    break;
    case CMD_DELETE_ACCOUNT: {
        if (name.empty())
        {
            logcb("Can't execute this command.");
            break;
        }

        string pass;
        if (!(args_str >> pass))
        {
            logcb("Invalid arguments. Example: deleteself user_password.");
            break;
        }
        pass = _str(pass).replace('*', ' ');
        string pass_hash = Hashing::ClientPassHash(name, pass);
        msg_len += PASS_HASH_SIZE;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push(pass_hash.c_str(), PASS_HASH_SIZE);
    }
    break;
    case CMD_CHANGE_PASSWORD: {
        if (name.empty())
        {
            logcb("Can't execute this command.");
            break;
        }

        string pass;
        string new_pass;
        if (!(args_str >> pass >> new_pass))
        {
            logcb("Invalid arguments. Example: changepassword current_password new_password.");
            break;
        }
        pass = _str(pass).replace('*', ' ');

        // Check the new password's validity
        uint pass_len = _str(new_pass).lengthUtf8();
        if (pass_len < MIN_NAME || pass_len < GameOpt.MinNameLength || pass_len > MAX_NAME ||
            pass_len > GameOpt.MaxNameLength)
        {
            logcb("Invalid new password.");
            break;
        }

        string pass_hash = Hashing::ClientPassHash(name, pass);
        new_pass = _str(new_pass).replace('*', ' ');
        string new_pass_hash = Hashing::ClientPassHash(name, new_pass);
        msg_len += PASS_HASH_SIZE * 2;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push(pass_hash.c_str(), PASS_HASH_SIZE);
        buf.Push(new_pass_hash.c_str(), PASS_HASH_SIZE);
    }
    break;
    case CMD_LOG: {
        string flags;
        if (!(args_str >> flags))
        {
            logcb("Invalid arguments. Example: log flag. Valid flags: '+' attach, '-' detach, '--' detach all.");
            break;
        }
        msg_len += NetBuffer::StringLenSize + (uint)flags.length();

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << flags;
    }
    break;
    case CMD_DEV_EXEC:
    case CMD_DEV_FUNC:
    case CMD_DEV_GVAR: {
        if (args.empty())
            break;

        msg_len += NetBuffer::StringLenSize + (uint)args.length();

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << args;
    }
    break;
    default:
        return false;
    }

    return true;
}

// Dummy symbols for web build to avoid linker errors
#ifdef FO_WEB
void* SDL_LoadObject(const char* sofile)
{
    UNREACHABLE_PLACE;
}

void* SDL_LoadFunction(void* handle, const char* name)
{
    UNREACHABLE_PLACE;
}

void SDL_UnloadObject(void* handle)
{
    UNREACHABLE_PLACE;
}
#endif

TEST_CASE()
{
    std::mt19937 rnd32;
    for (int i = 1; i < 10000; i++)
        (void)rnd32();
    RUNTIME_ASSERT(rnd32() == 4123659995);

    std::mt19937_64 rnd64;
    for (int i = 1; i < 10000; i++)
        (void)rnd64();
    RUNTIME_ASSERT(rnd64() == 9981545732273789042UL);
}
