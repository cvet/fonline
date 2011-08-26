//---------------------------------------------------------------------------

#ifndef CommonH
#define CommonH
#include <Classes.hpp>
#include <process.h>
#include <vector>
#include <stdio.h>

#define DEFAULT_WEB_HOST   "www.fonline.ru/servers.html"

typedef std::vector<AnsiString> AnsiStringVec;
typedef std::vector<AnsiString>::iterator AnsiStringVecIt;

extern bool IsEnglish;
#define LANG(rus, eng) (IsEnglish ? (eng) : (rus))

#define PROXY_NONE     (0)
#define PROXY_SOCKS4   (1)
#define PROXY_SOCKS5   (2)
#define PROXY_HTTP     (3)

void InitCrc32();
int Crc32(unsigned char* data, unsigned int len);
int Crc32(TStream* stream);
bool Compress(TMemoryStream* stream);
bool Uncompress(TMemoryStream* stream);

void SetConfigName(AnsiString cfgName, AnsiString appName);
AnsiString GetString(AnsiString key, AnsiString defVal);
int GetInt(AnsiString key, int defVal);
void SetString(AnsiString key, AnsiString val);
void SetInt(AnsiString key, int val);

void ShowMessageOK(AnsiString msg);
void RestoreMainDirectory();
void CheckDirectory(AnsiString fileName);
AnsiString FormatSize(int size);
char* EraseFrontBackSpecificChars(char* str);
bool SpecialCompare(const AnsiString& astrL, const AnsiString& astrR);

#endif
