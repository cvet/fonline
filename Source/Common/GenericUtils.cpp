#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"
#include "Testing.h"

#include "SDL.h"
#include "sha2.h"
#include "zlib.h"

uint Hashing::MurmurHash2(const uchar* data, uint len)
{
    const uint seed = 0;
    const uint m = 0x5BD1E995;
    const int r = 24;
    uint h = seed ^ len;

    while (len >= 4)
    {
        uint k;

        k = data[0];
        k |= data[1] << 8;
        k |= data[2] << 16;
        k |= data[3] << 24;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    switch (len)
    {
    case 3:
        h ^= data[2] << 16;
    case 2:
        h ^= data[1] << 8;
    case 1:
        h ^= data[0];
        h *= m;
    }

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

uint64 Hashing::MurmurHash2_64(const uchar* data, uint len)
{
    const uint seed = 0;
    const uint64 m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;
    uint64 h = seed ^ (len * m);

    const uint64* data2 = (const uint64*)data;
    const uint64* end = data2 + (len / 8);

    while (data2 != end)
    {
        uint64 k = *data2++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const uchar* data3 = (const uchar*)data2;

    switch (len & 7)
    {
    case 7:
        h ^= uint64(data3[6]) << 48;
    case 6:
        h ^= uint64(data3[5]) << 40;
    case 5:
        h ^= uint64(data3[4]) << 32;
    case 4:
        h ^= uint64(data3[3]) << 24;
    case 3:
        h ^= uint64(data3[2]) << 16;
    case 2:
        h ^= uint64(data3[1]) << 8;
    case 1:
        h ^= uint64(data3[0]);
        h *= m;
        break;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}

void Hashing::XOR(uchar* data, uint len, const uchar* xor_key, uint xor_len)
{
    for (uint i = 0; i < len; i++)
        data[i] ^= xor_key[i % xor_len];
}

string Hashing::ClientPassHash(const string& name, const string& pass)
{
    char* bld = new char[MAX_NAME + 1];
    memzero(bld, MAX_NAME + 1);
    size_t pass_len = pass.length();
    size_t name_len = name.length();
    if (pass_len > MAX_NAME)
        pass_len = MAX_NAME;
    memcpy(bld, pass.c_str(), pass_len);
    if (pass_len < MAX_NAME)
        bld[pass_len++] = '*';
    for (; name_len && pass_len < MAX_NAME; pass_len++)
        bld[pass_len] = name[pass_len % name_len];

    char* pass_hash = new char[MAX_NAME + 1];
    memzero(pass_hash, MAX_NAME + 1);
    sha256((const uchar*)bld, MAX_NAME, (uchar*)pass_hash);
    string result = pass_hash;
    delete[] bld;
    delete[] pass_hash;
    return result;
}

uchar* Compressor::Compress(const uchar* data, uint& data_len)
{
    uLongf buf_len = data_len * 110 / 100 + 12;
    uchar* buf = new uchar[buf_len];

    if (compress2(buf, &buf_len, data, data_len, Z_BEST_SPEED) != Z_OK)
    {
        SAFEDELA(buf);
        return nullptr;
    }

    data_len = (uint)buf_len;
    return buf;
}

bool Compressor::Compress(UCharVec& data)
{
    uint result_len = (uint)data.size();
    uchar* result = Compress(&data[0], result_len);
    if (!result)
        return false;

    data.resize(result_len);
    UCharVec(data).swap(data);
    memcpy(&data[0], result, result_len);
    SAFEDELA(result);
    return true;
}

uchar* Compressor::Uncompress(const uchar* data, uint& data_len, uint mul_approx)
{
    uLongf buf_len = data_len * mul_approx;
    if (buf_len > 100000000) // 100mb
    {
        WriteLog("Unpack buffer length is too large, data length {}, multiplier {}.\n", data_len, mul_approx);
        return nullptr;
    }

    uchar* buf = new uchar[buf_len];
    while (true)
    {
        int result = uncompress(buf, &buf_len, data, data_len);
        if (result == Z_BUF_ERROR)
        {
            buf_len *= 2;
            SAFEDELA(buf);
            buf = new uchar[buf_len];
        }
        else if (result != Z_OK)
        {
            SAFEDELA(buf);
            WriteLog("Unpack error {}.\n", result);
            return nullptr;
        }
        else
        {
            break;
        }
    }

    data_len = (uint)buf_len;
    return buf;
}

bool Compressor::Uncompress(UCharVec& data, uint mul_approx)
{
    uint result_len = (uint)data.size();
    uchar* result = Uncompress(&data[0], result_len, mul_approx);
    if (!result)
        return false;

    data.resize(result_len);
    memcpy(&data[0], result, result_len);
    SAFEDELA(result);
    return true;
}

// Default randomizer
#ifndef FO_TESTING
static std::mt19937 RandomGenerator(std::random_device {}());
#else
static std::mt19937 RandomGenerator(42);
#endif
int GenericUtils::Random(int minimum, int maximum)
{
    return std::uniform_int_distribution {minimum, maximum}(RandomGenerator);
}

// Math stuff
int GenericUtils::Procent(int full, int peace)
{
    if (!full)
        return 0;
    int procent = peace * 100 / full;
    return CLAMP(procent, 0, 100);
}

uint GenericUtils::NumericalNumber(uint num)
{
    if (num & 1)
        return num * (num / 2 + 1);
    else
        return num * num / 2 + num / 2;
}

bool GenericUtils::IntersectCircleLine(int cx, int cy, int radius, int x1, int y1, int x2, int y2)
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

void GenericUtils::ShowErrorMessage(const string& message, const string& traceback)
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

int GenericUtils::ConvertParamValue(const string& str, bool& fail)
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

    // Todo: script
    // return Script::GetEnumValue(str, fail);
    UNREACHABLE_PLACE;
}

uint GenericUtils::GetColorDay(int* day_time, uchar* colors, int game_time, int* light)
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

uint GenericUtils::DistSqrt(int x1, int y1, int x2, int y2)
{
    int dx = x1 - x2;
    int dy = y1 - y2;
    return (uint)sqrt(double(dx * dx + dy * dy));
}

void GenericUtils::GetStepsXY(float& sx, float& sy, int x1, int y1, int x2, int y2)
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

void GenericUtils::ChangeStepsXY(float& sx, float& sy, float deq)
{
    float rad = deq * PI_FLOAT / 180.0f;
    sx = sx * cos(rad) - sy * sin(rad);
    sy = sx * sin(rad) + sy * cos(rad);
}

TEST_CASE()
{
    RUNTIME_ASSERT(Hashing::MurmurHash2((uchar*)"abcdef", 6) == 1271458169);
    RUNTIME_ASSERT(Hashing::MurmurHash2_64((uchar*)"abcdef", 6) == 13226566493390071673ULL);
}

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
