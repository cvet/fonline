#pragma once

#include "Common.h"

class LineTracer
{
public:
    LineTracer(ushort hx, ushort hy, ushort tx, ushort ty, ushort maxhx, ushort maxhy, float angle, bool is_square);
    uchar GetNextHex(ushort& cx, ushort& cy);
    void GetNextSquare(ushort& cx, ushort& cy);

private:
    ushort maxHx;
    ushort maxHy;
    float x1;
    float y1;
    float x2;
    float y2;
    float dir;
    uchar dir1;
    uchar dir2;
    float dx;
    float dy;

    void NormalizeDir();
};
