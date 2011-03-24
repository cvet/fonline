#ifndef __CRYPT__
#define __CRYPT__

#include "Log.h"

class CryptManager
{
private:
	// Crc32 table
	uint crcTable[0x100];

public:
	// Init Crypt Manager
	CryptManager();

	// Returns Crc32 of data
	uint Crc32(uchar* data, uint len);

	// Continue calculate of Crc32
	void Crc32(uchar* data, uint len, uint& crc);

	// Returns CheckSum of data
	uint CheckSum(uchar* data, uint len);

	// Xor the data
	void XOR(char* data, uint len, char* xor_key, uint xor_len);

	// Password encrypt
	void EncryptPassword(char* data, uint len, uint key);
	void DecryptPassword(char* data, uint len, uint key);

	// Xor the text
	void TextXOR(char* data, uint len, char* xor_key, uint xor_len);

	// Compress zlib
	uchar* Compress(const uchar* data, uint& data_len);

	// Uncompress zlib
	uchar* Uncompress(const uchar* data, uint& data_len, uint mul_approx);

	// Crypt text
//	void CryptText(char* text);

	// Uncrypt text
//	void UncryptText(char* text);

	bool IsCacheTable(const char* cache_fname);
	bool CreateCacheTable(const char* cache_fname);
	bool SetCacheTable(const char* cache_fname);
	void SetCache(const char* data_name, const uchar* data, uint data_len);
	uchar* GetCache(const char* data_name, uint& data_len);
};

extern CryptManager Crypt;

#endif // __CRYPT__
