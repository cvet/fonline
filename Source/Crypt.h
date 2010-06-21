#ifndef __CRYPT__
#define __CRYPT__

#include "Log.h"

class CryptManager
{
private:
	// Crc32 table
	DWORD crcTable[0x100];

	// Tempolary argument
	DWORD tmpValue;

public:
	// Init Crypt Manager
	CryptManager();

	// Returns Crc32 of data
	DWORD Crc32(BYTE* data, DWORD len);

	// Continue calculate of Crc32
	void Crc32(BYTE* data, DWORD len, DWORD& crc);

	// Returns CheckSum of data
	DWORD CheckSum(BYTE* data, DWORD len);

	// Xor the data
	void XOR(char* data, DWORD len, char* xor, DWORD xor_len);

	// Xor the text
	void TextXOR(char* data, DWORD len, char* xor, DWORD xor_len);

	// Compress zlib
	BYTE* Compress(const BYTE* data, DWORD& data_len);

	// Uncompress zlib
	BYTE* Uncompress(const BYTE* data, DWORD& data_len, DWORD mul_approx);

	// Crypt text
//	void CryptText(char* text);

	// Uncrypt text
//	void UncryptText(char* text);

	bool CreateCacheTable(const char* chache_fname);
	//bool ChangeCacheTable(const char* chache_fname);
	bool SetCacheTable(const char* chache_fname);
	void SetCache(const char* data_name, const BYTE* data, DWORD data_len);
	BYTE* GetCache(const char* data_name, DWORD& data_len);
};

extern CryptManager Crypt;

#endif // __CRYPT__