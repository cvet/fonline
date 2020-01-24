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

#include "LineTracer.h"

LineTracer::LineTracer(
    GeometrySettings& sett, ushort hx, ushort hy, ushort tx, ushort ty, ushort maxhx, ushort maxhy, float angle) :
    settings {sett}, geomHelper(settings)
{
    maxHx = maxhx;
    maxHy = maxhy;

    if (!settings.MapHexagonal)
    {
        dir = atan2((float)(ty - hy), (float)(tx - hx)) + angle;
        dx = cos(dir);
        dy = sin(dir);
        if (fabs(dx) > fabs(dy))
        {
            dy /= fabs(dx);
            dx = (dx > 0 ? 1.0f : -1.0f);
        }
        else
        {
            dx /= fabs(dy);
            dy = (dy > 0 ? 1.0f : -1.0f);
        }
        x1 = (float)hx + 0.5f;
        y1 = (float)hy + 0.5f;
    }
    else
    {
        float nx = 3.0f * (float(tx) - float(hx));
        float ny = (float(ty) - float(hy)) * SQRT3T2_FLOAT - (float(tx & 1) - float(hx & 1)) * SQRT3_FLOAT;
        this->dir = 180.0f + RAD2DEG * atan2f(ny, nx);
        if (angle != 0.0f)
        {
            this->dir += angle;
            NormalizeDir();
        }

        if (dir >= 30.0f && dir < 90.0f)
        {
            dir1 = 5;
            dir2 = 0;
        }
        else if (dir >= 90.0f && dir < 150.0f)
        {
            dir1 = 4;
            dir2 = 5;
        }
        else if (dir >= 150.0f && dir < 210.0f)
        {
            dir1 = 3;
            dir2 = 4;
        }
        else if (dir >= 210.0f && dir < 270.0f)
        {
            dir1 = 2;
            dir2 = 3;
        }
        else if (dir >= 270.0f && dir < 330.0f)
        {
            dir1 = 1;
            dir2 = 2;
        }
        else
        {
            dir1 = 0;
            dir2 = 1;
        }

        x1 = 3.0f * float(hx) + BIAS_FLOAT;
        y1 = SQRT3T2_FLOAT * float(hy) - SQRT3_FLOAT * (float(hx & 1)) + BIAS_FLOAT;
        x2 = 3.0f * float(tx) + BIAS_FLOAT + BIAS_FLOAT;
        y2 = SQRT3T2_FLOAT * float(ty) - SQRT3_FLOAT * (float(tx & 1)) + BIAS_FLOAT;
        if (angle != 0.0f)
        {
            x2 -= x1;
            y2 -= y1;
            float xp = cos(angle / RAD2DEG) * x2 - sin(angle / RAD2DEG) * y2;
            float yp = sin(angle / RAD2DEG) * x2 + cos(angle / RAD2DEG) * y2;
            x2 = x1 + xp;
            y2 = y1 + yp;
        }
        dx = x2 - x1;
        dy = y2 - y1;
    }
}

uchar LineTracer::GetNextHex(ushort& cx, ushort& cy)
{
    ushort t1x = cx;
    ushort t2x = cx;
    ushort t1y = cy;
    ushort t2y = cy;
    geomHelper.MoveHexByDir(t1x, t1y, dir1, maxHx, maxHy);
    geomHelper.MoveHexByDir(t2x, t2y, dir2, maxHx, maxHy);
    float dist1 =
        dx * (y1 - (SQRT3T2_FLOAT * float(t1y) - (float(t1x & 1)) * SQRT3_FLOAT)) - dy * (x1 - 3 * float(t1x));
    float dist2 =
        dx * (y1 - (SQRT3T2_FLOAT * float(t2y) - (float(t2x & 1)) * SQRT3_FLOAT)) - dy * (x1 - 3 * float(t2x));
    dist1 = (dist1 > 0 ? dist1 : -dist1);
    dist2 = (dist2 > 0 ? dist2 : -dist2);
    if (dist1 <= dist2) // Left hand biased
    {
        cx = t1x;
        cy = t1y;
        return dir1;
    }
    else
    {
        cx = t2x;
        cy = t2y;
        return dir2;
    }
}

void LineTracer::GetNextSquare(ushort& cx, ushort& cy)
{
    x1 += dx;
    y1 += dy;
    cx = (ushort)floor(x1);
    cy = (ushort)floor(y1);
    if (cx >= maxHx)
        cx = maxHx - 1;
    if (cy >= maxHy)
        cy = maxHy - 1;
}

void LineTracer::NormalizeDir()
{
    if (dir <= 0.0f)
        dir = 360.0f - fmod(-dir, 360.0f);
    else if (dir >= 0.0f)
        dir = fmod(dir, 360.0f);
}
