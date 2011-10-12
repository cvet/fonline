#ifndef __FILE_SYSTEM__
#define __FILE_SYSTEM__

#include "Common.h"

#if defined ( FO_WINDOWS )
# define DIR_SLASH_C      '\\'
# define DIR_SLASH_S      "\\"
# define DIR_SLASH_SD     ".\\"
# define DIR_SLASH_SDD    "..\\"
#else
# define DIR_SLASH_C      '/'
# define DIR_SLASH_S      "/"
# define DIR_SLASH_SD     "./"
# define DIR_SLASH_SDD    "../"
#endif

#ifndef SEEK_SET
# define SEEK_SET         ( 0 ) // Seek from beginning of file
# define SEEK_CUR         ( 1 ) // Seek from current position
# define SEEK_END         ( 2 ) // Set file pointer to EOF plus "offset"
#endif

void* FileOpen( const char* fname, bool write );
void  FileClose( void* file );
bool  FileRead( void* file, void* buf, uint len, uint* rb = NULL );
bool  FileWrite( void* file, const void* buf, uint len );
bool  FileSetPointer( void* file, int offset, int origin );
void  FileGetTime( void* file, uint64& tc, uint64& ta, uint64& tw );
uint  FileGetSize( void* file );
bool  FileDelete( const char* fname );

struct FIND_DATA
{
    char FileName[ MAX_FOPATH ];
    bool IsDirectory;
};
void* FileFindFirst( const char* path, const char* extension, FIND_DATA& fd );
bool  FileFindNext( void* descriptor, FIND_DATA& fd );
void  FileFindClose( void* descriptor );

bool  MakeDirectory( const char* path );
char* FixPathSlashes( char* path );
bool  ResolvePath( char* path );

#endif // __FILE_SYSTEM__
