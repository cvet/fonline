#include "md5.h"
#include <windows.h>

using namespace std;

namespace Md5
{
	PMD5Init   MD5Init = NULL;
	PMD5Update MD5Update = NULL;
	PMD5Final  MD5Final = NULL;

	LPVOID MapFile_ReadOnly( LPCTSTR lpFileName, DWORD& dwSize )
	{
		dwSize = 0;
		HANDLE hFile = CreateFile( lpFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
		if( hFile == INVALID_HANDLE_VALUE )
			return NULL;
		if( dwSize == 0 )
			dwSize = GetFileSize( hFile, NULL );
		if( dwSize == 0 )
		{
			CloseHandle( hFile );
			return NULL;
		}
		HANDLE hFileMap = CreateFileMapping( hFile, NULL, PAGE_READONLY, 0, dwSize, NULL );
		CloseHandle( hFile );
		if( hFileMap == NULL )
			return NULL;
		LPVOID lpMap = MapViewOfFile( hFileMap, FILE_MAP_READ, 0, 0, dwSize );
		CloseHandle( hFileMap );
		return lpMap;
	}

	bool InitMD5( )
	{
		HMODULE hLib = LoadLibrary( TEXT( "Cryptdll.dll" ) );

		MD5Init = PMD5Init( GetProcAddress( hLib, "MD5Init" ) );
		MD5Update = PMD5Update( GetProcAddress( hLib, "MD5Update" ) );
		MD5Final = PMD5Final( GetProcAddress( hLib, "MD5Final" ) );
		return true;
	}

	TMD5 GetMD5( PUCHAR Buffer, DWORD dwSize )
	{
		const char HEX_CHAR_MAP[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
		MD5_CTX    context;
		TMD5       Result;
		MD5Init( &context );

		for( ; dwSize >= 64; Buffer += 64, dwSize -= 64 )
			MD5Update( &context, Buffer, 64 );
		MD5Update( &context, Buffer, dwSize );

		MD5Final( &context );

		for( int i = 0; i < 16; i++ )
		{
			Result.hash[ i * 2 ] = HEX_CHAR_MAP[ context.digest[ i ] >> 4 ];
			Result.hash[ i * 2 + 1 ] = HEX_CHAR_MAP[ context.digest[ i ] & 15 ];
		}
		Result.hash[ 32 ] = NULL;
		memcpy( Result.digest, context.digest, 16 );
		return Result;
	}

	string GetMD5File( string& fileName )
	{
		TMD5   MD5;
		DWORD  dwSize;
		LPVOID pFile = MapFile_ReadOnly( TEXT( fileName.c_str( ) ), dwSize );
		MD5 = GetMD5( PUCHAR( pFile ), dwSize );
		UnmapViewOfFile( pFile );
		return MD5.hash;
	}
}