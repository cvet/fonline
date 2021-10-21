#pragma once

#include <tchar.h>
#include <iostream>
#include <string.h>
namespace Md5
{
	typedef struct
	{
		char          hash[ 33 ]; // 32 chars for MD5, +1 for terminating zero
		unsigned char digest[ 16 ];
	} TMD5;

	typedef struct
	{
		unsigned long i[ 2 ];
		unsigned long buf[ 4 ];
		unsigned char in[ 64 ];
		unsigned char digest[ 16 ];
	} MD5_CTX;

	typedef void( __stdcall * PMD5Init )( MD5_CTX* context );
	typedef void( __stdcall * PMD5Update )( MD5_CTX* context, const unsigned char* input, unsigned int inlen );
	typedef void( __stdcall * PMD5Final )( MD5_CTX* context );

	extern void* MapFile_ReadOnly( const wchar_t * lpFileName, unsigned long& dwSize );
	extern bool InitMD5( );
	extern TMD5 GetMD5( unsigned char* Buffer, unsigned long dwSize );
	extern std::string GetMD5File( std::string& fileName );
}