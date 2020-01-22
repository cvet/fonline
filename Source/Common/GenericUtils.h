#pragma once

#include "Common.h"

class Hashing : public StaticClass
{
public:
    static hash MurmurHash2(const uchar* data, uint len);
    static uint64 MurmurHash2_64(const uchar* data, uint len);
    static void XOR(uchar* data, uint len, const uchar* xor_key, uint xor_len);
    static string ClientPassHash(const string& name, const string& pass);
};

class Compressor : public StaticClass
{
public:
    static uchar* Compress(const uchar* data, uint& data_len);
    static bool Compress(UCharVec& data);
    static uchar* Uncompress(const uchar* data, uint& data_len, uint mul_approx);
    static bool Uncompress(UCharVec& data, uint mul_approx);
};

class GenericUtils : public StaticClass
{
public:
    static int Random(int minimum, int maximum);
    static int Procent(int full, int peace);
    static uint NumericalNumber(uint num);
    static bool IntersectCircleLine(int cx, int cy, int radius, int x1, int y1, int x2, int y2);
    static void ShowErrorMessage(const string& message, const string& traceback);
    static int ConvertParamValue(const string& str, bool& fail);
    static uint GetColorDay(int* day_time, uchar* colors, int game_time, int* light);
    static uint DistSqrt(int x1, int y1, int x2, int y2);
    static void GetStepsXY(float& sx, float& sy, int x1, int y1, int x2, int y2);
    static void ChangeStepsXY(float& sx, float& sy, float deq);
};
