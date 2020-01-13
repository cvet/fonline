#include "Crypt.h"
#include "FileSystem.h"
#include "Log.h"
#include "StringUtils.h"
#include "Testing.h"
#ifdef FO_WEB
#include "DiskFileSystem.h"
#endif

#include "sha2.h"
#include "zlib.h"
#ifndef FO_WEB
#include "unqlite.h"
#endif

CryptManager Crypt;

uint CryptManager::MurmurHash2(const uchar* data, uint len)
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

uint64 CryptManager::MurmurHash2_64(const uchar* data, uint len)
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

void CryptManager::XOR(uchar* data, uint len, const uchar* xor_key, uint xor_len)
{
    for (uint i = 0; i < len; i++)
        data[i] ^= xor_key[i % xor_len];
}

string CryptManager::ClientPassHash(const string& name, const string& pass)
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

uchar* CryptManager::Compress(const uchar* data, uint& data_len)
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

bool CryptManager::Compress(UCharVec& data)
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

uchar* CryptManager::Uncompress(const uchar* data, uint& data_len, uint mul_approx)
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

bool CryptManager::Uncompress(UCharVec& data, uint mul_approx)
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

#ifndef FO_WEB
static unqlite* CacheDb;

bool CryptManager::InitCache()
{
    RUNTIME_ASSERT(!CacheDb);

    string path = File::GetWritePath("/Cache.bin");
    File::CreateDirectoryTree(path);

    if (unqlite_open(&CacheDb, path.c_str(), UNQLITE_OPEN_CREATE | UNQLITE_OPEN_OMIT_JOURNALING) != UNQLITE_OK)
    {
        CacheDb = nullptr;
        return false;
    }

    uint ping = 42;
    if (unqlite_kv_store(CacheDb, &ping, sizeof(ping), &ping, sizeof(ping)) != UNQLITE_OK)
    {
        unqlite_close(CacheDb);
        CacheDb = nullptr;
        return false;
    }

    if (unqlite_commit(CacheDb) != UNQLITE_OK)
    {
        unqlite_close(CacheDb);
        CacheDb = nullptr;
        return false;
    }

    return true;
}

bool CryptManager::IsCache(const string& data_name)
{
    RUNTIME_ASSERT(CacheDb);

    int r = unqlite_kv_fetch_callback(
        CacheDb, data_name.c_str(), (int)data_name.length(),
        [](const void*, unsigned int, void*) { return UNQLITE_OK; }, nullptr);
    RUNTIME_ASSERT((r == UNQLITE_OK || r == UNQLITE_NOTFOUND));

    return r == UNQLITE_OK;
}

void CryptManager::EraseCache(const string& data_name)
{
    RUNTIME_ASSERT(CacheDb);

    int r = unqlite_kv_delete(CacheDb, data_name.c_str(), (int)data_name.length());
    RUNTIME_ASSERT((r == UNQLITE_OK || r == UNQLITE_NOTFOUND));

    if (r == UNQLITE_OK)
    {
        r = unqlite_commit(CacheDb);
        RUNTIME_ASSERT(r == UNQLITE_OK);
    }
}

void CryptManager::SetCache(const string& data_name, const uchar* data, uint data_len)
{
    RUNTIME_ASSERT(CacheDb);

    int r = unqlite_kv_store(CacheDb, data_name.c_str(), (int)data_name.length(), data, data_len);
    RUNTIME_ASSERT(r == UNQLITE_OK);

    r = unqlite_commit(CacheDb);
    RUNTIME_ASSERT(r == UNQLITE_OK);
}

void CryptManager::SetCache(const string& data_name, const string& str)
{
    RUNTIME_ASSERT(CacheDb);

    SetCache(data_name, !str.empty() ? (uchar*)str.c_str() : (uchar*)"", (uint)str.length());
}

void CryptManager::SetCache(const string& data_name, UCharVec& data)
{
    RUNTIME_ASSERT(CacheDb);

    SetCache(data_name, !data.empty() ? (uchar*)&data[0] : (uchar*)"", (uint)data.size());
}

uchar* CryptManager::GetCache(const string& data_name, uint& data_len)
{
    RUNTIME_ASSERT(CacheDb);

    unqlite_int64 size;
    int r = unqlite_kv_fetch(CacheDb, data_name.c_str(), (int)data_name.length(), nullptr, &size);
    RUNTIME_ASSERT((r == UNQLITE_OK || r == UNQLITE_NOTFOUND));

    if (r == UNQLITE_NOTFOUND)
        return nullptr;

    uchar* data = new uchar[(uint)size];
    r = unqlite_kv_fetch(CacheDb, data_name.c_str(), (int)data_name.length(), data, &size);
    RUNTIME_ASSERT(r == UNQLITE_OK);

    data_len = (uint)size;
    return data;
}

string CryptManager::GetCache(const string& data_name)
{
    RUNTIME_ASSERT(CacheDb);

    string str;
    uint result_len;
    uchar* result = GetCache(data_name, result_len);
    if (!result)
        return str;

    if (result_len)
    {
        str.resize(result_len);
        memcpy(&str[0], result, result_len);
    }
    SAFEDELA(result);
    return str;
}

bool CryptManager::GetCache(const string& data_name, UCharVec& data)
{
    RUNTIME_ASSERT(CacheDb);

    data.clear();
    uint result_len;
    uchar* result = GetCache(data_name, result_len);
    if (!result)
        return false;

    data.resize(result_len);
    if (result_len)
        memcpy(&data[0], result, result_len);
    SAFEDELA(result);
    return true;
}

#else

static string MakeCachePath(const string& data_name)
{
    return File::GetWritePath("Cache/" + _str(data_name).replace('/', '_').replace('\\', '_'));
}

bool CryptManager::InitCache()
{
    return true;
}

bool CryptManager::IsCache(const string& data_name)
{
    string path = MakeCachePath(data_name);
    auto f = DiskFileSystem::OpenFile(path, false);
    return f != nullptr;
}

void CryptManager::EraseCache(const string& data_name)
{
    string path = MakeCachePath(data_name);
    DiskFileSystem::DeleteFile(path);
}

void CryptManager::SetCache(const string& data_name, const uchar* data, uint data_len)
{
    string path = MakeCachePath(data_name);
    auto f = DiskFileSystem::OpenFile(path, true);
    if (!f)
    {
        WriteLog("Can't open write cache at '{}'.\n", path);
        return;
    }

    if (!DiskFileSystem::WriteFile(f, data, data_len))
    {
        WriteLog("Can't write cache to '{}'.\n", path);
        f = nullptr;
        DiskFileSystem::DeleteFile(path);
        return;
    }
}

void CryptManager::SetCache(const string& data_name, const string& str)
{
    SetCache(data_name, !str.empty() ? (uchar*)str.c_str() : (uchar*)"", (uint)str.length());
}

void CryptManager::SetCache(const string& data_name, UCharVec& data)
{
    SetCache(data_name, !data.empty() ? (uchar*)&data[0] : (uchar*)"", (uint)data.size());
}

uchar* CryptManager::GetCache(const string& data_name, uint& data_len)
{
    string path = MakeCachePath(data_name);
    auto f = DiskFileSystem::OpenFile(path, false);
    if (!f)
        return nullptr;

    data_len = DiskFileSystem::GetFileSize(f);
    uchar* data = new uchar[data_len];

    if (!DiskFileSystem::ReadFile(f, data, data_len))
    {
        WriteLog("Can't read cache from '{}'.\n", path);
        SAFEDELA(data);
        data_len = 0;
        return nullptr;
    }

    return data;
}

string CryptManager::GetCache(const string& data_name)
{
    string str;
    uint result_len;
    uchar* result = GetCache(data_name, result_len);
    if (!result)
        return str;

    if (result_len)
    {
        str.resize(result_len);
        memcpy(&str[0], result, result_len);
    }
    SAFEDELA(result);
    return str;
}

bool CryptManager::GetCache(const string& data_name, UCharVec& data)
{
    data.clear();
    uint result_len;
    uchar* result = GetCache(data_name, result_len);
    if (!result)
        return false;

    data.resize(result_len);
    if (result_len)
        memcpy(&data[0], result, result_len);
    SAFEDELA(result);
    return true;
}
#endif

TEST_CASE()
{
    RUNTIME_ASSERT(Crypt.MurmurHash2((uchar*)"abcdef", 6) == 1271458169);
    RUNTIME_ASSERT(Crypt.MurmurHash2_64((uchar*)"abcdef", 6) == 13226566493390071673ULL);
}
