#pragma once

#include "Common.h"

#include "GeometryHelper.h"
#include "Settings.h"

class LineTracer
{
public:
    LineTracer(
        GeometrySettings& sett, ushort hx, ushort hy, ushort tx, ushort ty, ushort maxhx, ushort maxhy, float angle);
    uchar GetNextHex(ushort& cx, ushort& cy);
    void GetNextSquare(ushort& cx, ushort& cy);

private:
    void NormalizeDir();

    GeometrySettings& settings;
    GeometryHelper geomHelper;
    ushort maxHx {};
    ushort maxHy {};
    float x1 {};
    float y1 {};
    float x2 {};
    float y2 {};
    float dir {};
    uchar dir1 {};
    uchar dir2 {};
    float dx {};
    float dy {};
};
