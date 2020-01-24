#pragma once

#include "Common.h"

#include "Settings.h"

#define MAX_HEX_OFFSET (50) // Must be not odd

class GeometryHelper : public NonCopyable
{
public:
    GeometryHelper(GeometrySettings& sett);
    ~GeometryHelper();
    uint DistGame(int x1, int y1, int x2, int y2);
    int GetNearDir(int x1, int y1, int x2, int y2);
    int GetFarDir(int x1, int y1, int x2, int y2);
    int GetFarDir(int x1, int y1, int x2, int y2, float offset);
    bool CheckDist(ushort x1, ushort y1, ushort x2, ushort y2, uint dist);
    int ReverseDir(int dir);
    bool MoveHexByDir(ushort& hx, ushort& hy, uchar dir, ushort maxhx, ushort maxhy);
    void MoveHexByDirUnsafe(int& hx, int& hy, uchar dir);
    void GetHexOffsets(bool odd, short*& sx, short*& sy);
    void GetHexInterval(int from_hx, int from_hy, int to_hx, int to_hy, int& x, int& y);

private:
    void InitializeHexOffsets();

    GeometrySettings& settings;
    short* sxEven {};
    short* syEven {};
    short* sxOdd {};
    short* syOdd {};
};
