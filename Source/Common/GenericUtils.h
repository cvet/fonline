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
