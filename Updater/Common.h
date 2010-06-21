//---------------------------------------------------------------------------

#ifndef CommonH
#define CommonH
#include <Classes.hpp>

void InitCrc32();
unsigned int Crc32(unsigned char* data, unsigned int len);
bool Compress(TMemoryStream* stream);
bool Uncompress(TMemoryStream* stream);
//---------------------------------------------------------------------------
#endif
