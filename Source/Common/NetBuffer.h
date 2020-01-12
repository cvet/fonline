#pragma once

#include "Common.h"
#include "NetProtocol_Include.h"
#include "Testing.h"

class NetBuffer
{
public:
    static const uint DefaultBufSize = 4096;
    static const int CryptKeysCount = 50;
    static const int StringLenSize = sizeof(ushort);

private:
    bool isError;
    uchar* bufData;
    uint bufLen;
    uint bufEndPos;
    uint bufReadPos;
    bool encryptActive;
    int encryptKeyPos;
    uchar encryptKeys[CryptKeysCount];

    uchar EncryptKey(int move);
    void CopyBuf(const void* from, void* to, uchar crypt_key, uint len);

public:
    NetBuffer();
    ~NetBuffer();

    void SetEncryptKey(uint seed);
    void Refresh();
    void Reset();
    void Push(const void* buf, uint len, bool no_crypt = false);
    void Pop(void* buf, uint len);
    void Cut(uint len);
    void GrowBuf(uint len);
    uchar* GetData() { return bufData; }
    uchar* GetCurData() { return bufData + bufReadPos; }
    uint GetLen() { return bufLen; }
    uint GetCurPos() { return bufReadPos; }
    void SetEndPos(uint pos) { bufEndPos = pos; }
    uint GetEndPos() { return bufEndPos; }
    void MoveReadPos(int val);
    bool IsError() { return isError; }
    void SetError(bool value) { isError = value; }
    bool IsEmpty() { return bufReadPos >= bufEndPos; }
    bool IsHaveSize(uint size) { return bufReadPos + size <= bufEndPos; }
    bool NeedProcess();
    void SkipMsg(uint msg);

    // Generic specification
    template<typename T>
    NetBuffer& operator<<(const T& i)
    {
        Push(&i, sizeof(T));
        return *this;
    }

    template<typename T>
    NetBuffer& operator>>(T& i)
    {
        Pop(&i, sizeof(T));
        return *this;
    }

    // String specification
    NetBuffer& operator<<(const string& i)
    {
        RUNTIME_ASSERT(i.length() <= 65535);
        ushort len = (ushort)i.length();
        Push(&len, sizeof(len));
        Push(i.c_str(), len);
        return *this;
    }

    NetBuffer& operator>>(string& i)
    {
        ushort len = 0;
        Pop(&len, sizeof(len));
        i.resize(len);
        Pop(&i[0], len);
        return *this;
    }

    // Disable copying
    NetBuffer(const NetBuffer& other) = delete;
    NetBuffer& operator=(const NetBuffer& other) = delete;

    // Disable transferring of some types
    NetBuffer& operator<<(const uint64& i) = delete;
    NetBuffer& operator>>(uint64& i) = delete;
    NetBuffer& operator<<(const float& i) = delete;
    NetBuffer& operator>>(float& i) = delete;
    NetBuffer& operator<<(const double& i) = delete;
    NetBuffer& operator>>(double& i) = delete;
};
