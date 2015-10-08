#ifndef __FILE_SYSTEM__
#define __FILE_SYSTEM__

#include "Common.h"

#ifndef SEEK_SET
# define SEEK_SET    ( 0 )      // Seek from beginning of file
# define SEEK_CUR    ( 1 )      // Seek from current position
# define SEEK_END    ( 2 )      // Set file pointer to EOF plus "offset"
#endif

void*  FileOpen( const char* fname, bool write, bool write_through = false );
void*  FileOpenForAppend( const char* fname, bool write_through = false );
void*  FileOpenForReadWrite( const char* fname, bool write_through = false );
void   FileClose( void* file );
bool   FileRead( void* file, void* buf, uint len, uint* rb = NULL );
bool   FileWrite( void* file, const void* buf, uint len );
bool   FileSetPointer( void* file, int offset, int origin );
uint   FileGetPointer( void* file );
uint64 FileGetWriteTime( void* file );
uint   FileGetSize( void* file );
bool   FileDelete( const char* fname );
bool   FileExist( const char* fname );
bool   FileRename( const char* fname, const char* new_fname );
void   CreateDirectoryTree( const char* path );

struct FindData
{
    char   FileName[ MAX_FOPATH ];
    uint   FileSize;
    uint64 WriteTime;
    bool   IsDirectory;
};
typedef vector< FindData > FindDataVec;
void* FileFindFirst( const char* path, const char* extension, FindData& fd );
bool  FileFindNext( void* descriptor, FindData& fd );
void  FileFindClose( void* descriptor );

bool  MakeDirectory( const char* path );
char* NormalizePathSlashes( char* path );
bool  ResolvePath( char* path );

#endif // __FILE_SYSTEM__
