#ifndef __FILE_SYSTEM__
#define __FILE_SYSTEM__

#include "Common.h"

#ifndef SEEK_SET
# define SEEK_SET    ( 0 )      // Seek from beginning of file
# define SEEK_CUR    ( 1 )      // Seek from current position
# define SEEK_END    ( 2 )      // Set file pointer to EOF plus "offset"
#endif

void*  FileOpen( const string& fname, bool write, bool write_through = false );
void*  FileOpenForAppend( const string& fname, bool write_through = false );
void*  FileOpenForReadWrite( const string& fname, bool write_through = false );
void   FileClose( void* file );
bool   FileRead( void* file, void* buf, uint len, uint* rb = nullptr );
bool   FileWrite( void* file, const void* buf, uint len );
bool   FileSetPointer( void* file, int offset, int origin );
uint   FileGetPointer( void* file );
uint64 FileGetWriteTime( void* file );
uint   FileGetSize( void* file );
bool   FileDelete( const string& fname );
bool   FileExist( const string& fname );
bool   FileCopy( const string& fname, const string& copy_fname );
bool   FileRename( const string& fname, const string& new_fname );

void* FileFindFirst( const string& path, const string& extension, string* fname, uint* fsize, uint64* wtime, bool* is_dir );
bool  FileFindNext( void* descriptor, string* fname, uint* fsize, uint64* wtime, bool* is_dir );
void  FileFindClose( void* descriptor );

void NormalizePathSlashesInplace( string& path );
void ResolvePathInplace( string& path );
void MakeDirectory( const string& path );
void MakeDirectoryTree( const string& path );

#endif // __FILE_SYSTEM__
