#include "GenericUtils.h"
#include "Log.h"
#include "Testing.h"

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

TEST_CASE()
{
    RUNTIME_ASSERT(Hashing::MurmurHash2((uchar*)"abcdef", 6) == 1271458169);
    RUNTIME_ASSERT(Hashing::MurmurHash2_64((uchar*)"abcdef", 6) == 13226566493390071673ULL);
}
