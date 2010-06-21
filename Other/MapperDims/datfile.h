#if !defined(_DATFILE_H)
#define _DATFILE_H

#include <windows.h>
#include <Classes.hpp>

#include <stdio.h>
#include <dir.h>
#include <alloc.h>
#include <stdlib.h>

// Added by ABel: Seamless reading from both Fallouts' DAT
#include "cfile/cfile.h"

class TDatFile
{
public:
   char FileName[256];
   String FullName;             // Имя файла.
   BYTE  FileType;              //если там 1, то файл считается компрессированым(не всегда).
   DWORD RealSize;              //Размер файла без декомпрессии
   DWORD PackedSize;            //Размер сжатого файла
   DWORD Offset;                //Адрес файла в виде смещения от начала DAT-файла.

   ULONG FileIndex;             //Index of file in DAT
   ULONG FileNameSize;          // Размер имени файла

   LONG FilePointer;            // Указатель в DAT файле

   bool lError;
   bool Fallout1;               //Added for Fallout1
   UINT ErrorType;

   HANDLE h_in, h_out, h_imp;   // Handles: (DAT, output, import) files

   BYTE *m_pInBuf;

   String DatFileName;
   ULONG FileSizeFromDat;
   ULONG TreeSize;              // Размер дерева файлов
   ULONG tmpTreeSize;
   ULONG FilesTotal;

   bool CancelOp;

   TStringList *lstPaths;       // Cache for
   BYTE **pPtrs;                // Fallout directories

   char *buff, *ptr, *filebuff, *gzipbuff, *ptrCache, *ptr_end;
   //in buff - DATtree, ptr - pointer

   CFile* reader; // reader for current file in DAT-archive

   int ReadTree(void);
   void IndexingDAT(void);
   bool FindFile(String Mask);
   HANDLE DATOpenFile(String FileName);
   bool DATSetFilePointer(LONG lDistanceToMove, DWORD dwMoveMethod);
   bool DATReadFile(LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                                                   LPDWORD lpNumberOfBytesRead);
   DWORD DATGetFileSize(void);

   String ExtractFiles(TStringList *MaskList, String DestinationFolder);
   void ExtractFile(int Temp);
   bool ImportFile(HANDLE h_outdat, HANDLE h_outtree,
                   String DestinationFolder, String impfilename);
   void ImportFiles(TStrings *FilesList, String SourceFolder,
                                                      String DestinationFolder);
   UINT CalculateFiles(String Mask);
   String GetPathName(void);
   String GetFileName(bool WithExtension);
   String GetDirOrFile(String Mask);
   void RevDw(DWORD *addr);
   void ResetPtr(void);
   void ShowError(void);

   TDatFile(String filename);
   virtual ~TDatFile();

protected:
   void FindInDirectory(String PathAndMask);

};


#endif
