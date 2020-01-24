//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "GeometryHelper.h"

GeometryHelper::GeometryHelper(GeometrySettings& sett) : settings {sett}
{
}

GeometryHelper::~GeometryHelper()
{
    if (sxEven)
    {
        if (settings.MapHexagonal)
        {
            delete[] sxEven;
            delete[] syEven;
            delete[] sxOdd;
            delete[] syOdd;
        }
        else
        {
            delete[] sxEven;
            delete[] syEven;
        }
    }
}

void GeometryHelper::InitializeHexOffsets()
{
    int size = (MAX_HEX_OFFSET * MAX_HEX_OFFSET / 2 + MAX_HEX_OFFSET / 2) * settings.MapDirCount;
    if (settings.MapHexagonal)
    {
        sxEven = new short[size];
        syEven = new short[size];
        sxOdd = new short[size];
        syOdd = new short[size];

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
                    sxEven[pos] = xe;
                    syEven[pos] = ye;
                    sxOdd[pos] = xo - 1;
                    syOdd[pos] = yo;
                    pos++;
                    MoveHexByDirUnsafe(xe, ye, dir);
                    MoveHexByDirUnsafe(xo, yo, dir);
                }
            }
        }
    }
    else
    {
        sxEven = sxOdd = new short[size];
        syEven = syOdd = new short[size];

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
                    sxEven[pos] = hx;
                    syEven[pos] = hy;
                    pos++;
                    MoveHexByDirUnsafe(hx, hy, dir);
                }
            }
        }
    }
}

uint GeometryHelper::DistGame(int x1, int y1, int x2, int y2)
{
    if (settings.MapHexagonal)
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

int GeometryHelper::GetNearDir(int x1, int y1, int x2, int y2)
{
    int dir = 0;

    if (settings.MapHexagonal)
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

int GeometryHelper::GetFarDir(int x1, int y1, int x2, int y2)
{
    if (settings.MapHexagonal)
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

int GeometryHelper::GetFarDir(int x1, int y1, int x2, int y2, float offset)
{
    if (settings.MapHexagonal)
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

bool GeometryHelper::CheckDist(ushort x1, ushort y1, ushort x2, ushort y2, uint dist)
{
    return DistGame(x1, y1, x2, y2) <= dist;
}

int GeometryHelper::ReverseDir(int dir)
{
    return (dir + settings.MapDirCount / 2) % settings.MapDirCount;
}

bool GeometryHelper::MoveHexByDir(ushort& hx, ushort& hy, uchar dir, ushort maxhx, ushort maxhy)
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

void GeometryHelper::MoveHexByDirUnsafe(int& hx, int& hy, uchar dir)
{
    if (settings.MapHexagonal)
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

void GeometryHelper::GetHexOffsets(bool odd, short*& sx, short*& sy)
{
    if (!sxEven)
        InitializeHexOffsets();

    sx = (odd ? sxOdd : sxEven);
    sy = (odd ? syOdd : syEven);
}

void GeometryHelper::GetHexInterval(int from_hx, int from_hy, int to_hx, int to_hy, int& x, int& y)
{
    if (settings.MapHexagonal)
    {
        int dx = to_hx - from_hx;
        int dy = to_hy - from_hy;
        x = dy * (settings.MapHexWidth / 2) - dx * settings.MapHexWidth;
        y = dy * settings.MapHexLineHeight;
        if (from_hx & 1)
        {
            if (dx > 0)
                dx++;
        }
        else if (dx < 0)
            dx--;
        dx /= 2;
        x += (settings.MapHexWidth / 2) * dx;
        y += settings.MapHexLineHeight * dx;
    }
    else
    {
        int dx = to_hx - from_hx;
        int dy = to_hy - from_hy;
        x = (dy - dx) * settings.MapHexWidth / 2;
        y = (dy + dx) * settings.MapHexLineHeight;
    }
}
