//////////////////////////////////////////////////////////////////////
// TDatFile Class
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include <FileCtrl.hpp>
#include "macros.h"
#include "error.h"
#include "zlib\zlib.h"
#include "cfile/cfile.h"

#include "datfile.h"

char *error_types[] = {
   "Cannot open file.",                       // ERR_CANNOT_OPEN_FILE
   "File invalid or truncated.",              // ERR_FILE_TRUNCATED
   "This file not supported.",                // ERR_FILE_NOT_SUPPORTED
   "Not enough memory to allocate buffer.",   // ERR_ALLOC_MEMORY
   "Cannot open DAT file.",                   // ERR_CANNOT_OPEN_DAT_FILE
   "Cannot create output file.",              // ERR_CANNOT_CREATE_OUT_FILE
   "Cannot write to output file. Disk full?", // ERR_CANNOT_WRITE_TO_FILE
   "Fatal error."                             // ERR_UNKNOWN
};
//------------------------------------------------------------------------------
TDatFile::TDatFile(String filename)
{
   lError = true;
   buff = NULL;
   m_pInBuf = NULL;
   Fallout1 = false;
   unsigned long i;
   lstPaths = NULL;
   pPtrs = NULL;

   reader = NULL; // Initially empty reader. We don't know its type at this point

   lstPaths = new TStringList();
   pPtrs = new BYTE* [1024];

   h_in = CreateFile(filename.c_str(),  //В h_in находится HANDLE на DAT файл
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

   if (h_in == INVALID_HANDLE_VALUE)
   {
      ErrorType = ERR_CANNOT_OPEN_FILE;
      return;
   }
   if (ReadTree() != RES_OK)
   {
      return;
   }
   m_pInBuf = (BYTE *) malloc(ZLIB_BUFF_SIZE);
   if (m_pInBuf == NULL)
   {
      ErrorType = ERR_ALLOC_MEMORY;
      return;
   }
   lError = false;
   DatFileName = filename;
}
//------------------------------------------------------------------------------
int TDatFile::ReadTree()
{
   unsigned long i, pof, n, m, F1DirCount, F1FilesTotal, F1StrLenDW,
                  F1compressed, F1offset, F1filesize, F1unknown;
   BYTE F1StrLen;
   char F1StrBuf[255];
   TStringList *dirs = new TStringList;
//             Проверка на то, что файл не менее 8 байт
   i = SetFilePointer(h_in, -8, NULL, FILE_END);
   if (i == 0xFFFFFFFF)
       return ERR_FILE_TRUNCATED;
//             Чтение информации из DAT файла
   ReadFile(h_in, &TreeSize, 4, &i, NULL);
   ReadFile(h_in, &FileSizeFromDat, 4, &i, NULL);

   i = SetFilePointer(h_in, 0, NULL, FILE_BEGIN); //Added for Fallout1
   ReadFile(h_in, &F1DirCount, 4, &i, NULL); //Added for Fallout1
   RevDw(&F1DirCount); //Added for Fallout1
   if (F1DirCount == 0x01 || F1DirCount == 0x33) Fallout1 = true; //Added for Fallout1
   if (GetFileSize(h_in, NULL) != FileSizeFromDat && Fallout1 == false)
      return ERR_FILE_NOT_SUPPORTED;
   if (!Fallout1)
   {
      i = SetFilePointer (h_in, -(TreeSize + 8), NULL, FILE_END);
      ReadFile(h_in, &FilesTotal, 4, &i, NULL);
   }
   else //FALLOUT 1 !!!
   {
      // Получаем длинну DAT файла
      FileSizeFromDat = GetFileSize(h_in, NULL);
      // Пропускаем первые 4 байта
      i=SetFilePointer (h_in, 16, NULL, FILE_BEGIN);
      // Некоторые начальные установки
      TreeSize = 0;
      FilesTotal = 0;
      pof = 16;
      // Заполняем массив директориями
      for (n = 1; n <= F1DirCount; n++)
      {
         ReadFile(h_in, &F1StrLen, 1, &i, NULL);
         ReadFile(h_in, &F1StrBuf, F1StrLen, &i, NULL);
         pof += F1StrLen + 1;
         F1StrBuf[F1StrLen++] = 0x5C;
         if (F1StrLen == 2) F1StrLen = 0;
         F1StrBuf[F1StrLen] = 0x00;
         dirs->Add(String(F1StrBuf));
      }
      // Подсчитываем длинну дерева необходимую для конвертации
      for (n = 1; n <= F1DirCount; n++)
      {
         ReadFile(h_in, &F1FilesTotal, 4, &i, NULL);
         RevDw(&F1FilesTotal);
         i=SetFilePointer(h_in, 12, NULL, FILE_CURRENT);
         for (m = 1; m <= F1FilesTotal; m++)
         {
            FilesTotal++;
            ReadFile(h_in, &F1StrLen, 1, &i, NULL);
            TreeSize += dirs->Strings[n - 1].Length() + F1StrLen + 17; //17=4+1+12
            i = SetFilePointer(h_in, F1StrLen + 16, NULL, FILE_CURRENT);
         }
      }
      // В TreeSize теперь длинна дерева необходимая для конвертации
   }

   if (buff != NULL)
      free(buff);
   if ((buff = (char *)malloc(TreeSize)) == NULL)
      return ERR_ALLOC_MEMORY;
   ZeroMemory(buff, TreeSize);

   if (!Fallout1)
      ReadFile(h_in, buff, TreeSize - 4, &i, NULL);
   else
   {
      ptr = buff;
      i=SetFilePointer(h_in, pof, NULL, FILE_BEGIN); //Restore old file pointer
      TreeSize += 4; // Учитывается 4 байта под FilesTotal
      for (n = 1; n <= F1DirCount; n++)
      {
         ReadFile(h_in, &F1FilesTotal, 4, &i, NULL);
         RevDw(&F1FilesTotal);
         i = SetFilePointer(h_in, 12, NULL, FILE_CURRENT);
         for (m = 1; m <= F1FilesTotal; m++)
         {
            ReadFile(h_in, &F1StrLen, 1, &i, NULL);
            F1StrLenDW = dirs->Strings[n - 1].Length() + F1StrLen;
            memcpy(ptr, &F1StrLenDW, 4);
            ptr += 4;
            memcpy(ptr, dirs->Strings[n - 1].c_str(), dirs->Strings[n - 1].Length());
            ptr += dirs->Strings[n - 1].Length();
            ReadFile(h_in, ptr, F1StrLen, &i, NULL);
            ptr += F1StrLen;
            ReadFile(h_in, &F1compressed, 4, &i, NULL);
            RevDw(&F1compressed);
            ReadFile(h_in, &F1offset, 4, &i, NULL);
            RevDw(&F1offset);
            ReadFile(h_in, &F1filesize, 4, &i, NULL);
            RevDw(&F1filesize);
            ReadFile(h_in, &F1unknown, 4, &i, NULL);
            RevDw(&F1unknown);
            if (F1compressed == 0x40) //compressed
               *ptr = 0x01;
            else
               *ptr = 0x00;
            ptr ++;
            memcpy(ptr, &F1filesize, 4); // RealSize
            ptr += 4;
            memcpy(ptr, &F1unknown, 4); // PackedSize
            ptr += 4;
            memcpy(ptr, &F1offset, 4); // Offset
            ptr += 4;
         }
      }
      delete dirs;
      TreeSize -= 4;
   }
   ptr_end = buff + TreeSize;
//   ResetPtr();
   IndexingDAT();
   return RES_OK;
}
//------------------------------------------------------------------------------
void TDatFile::IndexingDAT(void)
{
   String CurrentPath, LastAddedPath;
   ptr = buff;
   while (true)
   {
      FileNameSize = *(ULONG *)ptr;
      memcpy(&FileName, ptr + 4, FileNameSize);
      FileName[FileNameSize] = 0;
      CurrentPath = UpperCase(ExtractFilePath((AnsiString)FileName));
      if (CurrentPath != LastAddedPath && !CurrentPath.IsEmpty())
      {
         lstPaths->Add(CurrentPath);
         LastAddedPath = CurrentPath;
         pPtrs[lstPaths->Count - 1] = ptr;
      }
      if ((ptr + FileNameSize + 17) >= ptr_end)
         break;
      else
         ptr += FileNameSize + 17;
   }
   ptr = buff;
}
//------------------------------------------------------------------------------
void TDatFile::ResetPtr(void)
{
   ptr = buff;
//   FileIndex = 0;
}
//------------------------------------------------------------------------------
bool TDatFile::FindFile(String Mask)
{
   while (true)
   {
//      FileIndex ++;
      FileNameSize = *(ULONG *)ptr;
      ptr += 4;
      memcpy(&FileName, ptr, FileNameSize);
      FileName[FileNameSize] = 0;
      FullName = (AnsiString)FileName;
      FileType = *(ptr + FileNameSize);
      RealSize = *(DWORD *)(ptr + FileNameSize + 1);
      PackedSize = *(DWORD *)(ptr + FileNameSize + 5);
      Offset = *(DWORD *)(ptr + FileNameSize + 9);
      if (FullName.UpperCase() == Mask.UpperCase())
         return true;
      if ((ptr + FileNameSize + 13) >= ptr_end)
         break;
      else
         ptr += FileNameSize + 13;
   }
   return false;
}
//------------------------------------------------------------------------------
HANDLE TDatFile::DATOpenFile(String FileName)
{
   // If we still have old non-closed reader - we kill it
   if (reader) {
      delete reader;
      reader = NULL;
   }

   if (h_in != INVALID_HANDLE_VALUE)
   {
      String OpPath = UpperCase(ExtractFilePath(FileName));
      int Position = lstPaths->IndexOf(OpPath);
      if (Position != -1)
         ptr = pPtrs[Position];
      else
         ResetPtr();
      if (FindFile(FileName.TrimRight()))
      {
         if (!FileType) {
            reader = new CPlainFile (h_in, Offset, RealSize);
         } else {
            if (Fallout1) {
               reader = new C_LZ_BlockFile (h_in, Offset, RealSize, PackedSize);
            } else {
               reader = new C_Z_PackedFile (h_in, Offset, RealSize, PackedSize);
            }
         }
         return h_in;
      }
   }
   return INVALID_HANDLE_VALUE;
}
//------------------------------------------------------------------------------
bool TDatFile::DATSetFilePointer(LONG lDistanceToMove, DWORD dwMoveMethod)
{
   if (h_in == INVALID_HANDLE_VALUE) return false;
   reader->seek (lDistanceToMove, dwMoveMethod);
   return true;
}
//------------------------------------------------------------------------------
DWORD TDatFile::DATGetFileSize(void)
{
   if (h_in == INVALID_HANDLE_VALUE) return 0;
   return RealSize;
}
//------------------------------------------------------------------------------
bool TDatFile::DATReadFile(LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                                                    LPDWORD lpNumberOfBytesRead)
{
   if (h_in == INVALID_HANDLE_VALUE) return false;
   if (!lpBuffer) return false;
   if (!nNumberOfBytesToRead) {
      lpNumberOfBytesRead = 0;
      return true;
   }
   reader->read (lpBuffer, nNumberOfBytesToRead, (long*)lpNumberOfBytesRead);
   return true;
}
//------------------------------------------------------------------------------
void TDatFile::ExtractFile(int Temp)
{
   filebuff = NULL;
   gzipbuff = NULL;
   h_out = NULL;
   unsigned long i = 0;

   if (FileType)
   {
       if (PackedSize || !Fallout1)
       {
          gzipbuff = (char *) malloc(RealSize);
          if (gzipbuff == NULL)
          {
             ErrorType = ERR_ALLOC_MEMORY;
             lError = true;
             goto err;
          }
       }
   }
   else
   {
       if (PackedSize || Fallout1)
       {
          filebuff = (char *) malloc(RealSize);
          if (filebuff == NULL)
          {
             ErrorType = ERR_ALLOC_MEMORY;
             lError = true;
             goto err;
          }
      }
   }

   i = SetFilePointer (h_in, Offset, NULL,FILE_BEGIN);

   if (FileType)     //if compressed
   {
	z_stream stream;
 	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	stream.next_in = Z_NULL;
	stream.avail_in = 0;
	if (::inflateInit (&stream)!=Z_OK)
        {
           ErrorType = ERR_UNKNOWN;
           lError = true;
           goto err;
	}

	stream.next_out = gzipbuff;
	stream.avail_out = RealSize; // ZLIB_BUFF_SIZE <-----!!! Fixing bug !!!-

	int res = Z_OK;

 	while ( res == Z_OK )
        {
	   if (stream.avail_in == 0) // stream.avail_in == 0
	   {
              stream.next_in = m_pInBuf;
              if(::ReadFile(h_in, m_pInBuf, ZLIB_BUFF_SIZE, (unsigned long*)&stream.avail_in, NULL)==0)
              {
                 ErrorType = ERR_UNKNOWN;
                 lError = true;
                 goto err;
              }
           }

	   // INFLATING BUFFER
	   res = ::inflate (&stream, Z_PARTIAL_FLUSH); //Z_NO_FLUSH

           if (stream.avail_out == 0 || res == Z_STREAM_END)
           {
	      stream.avail_out = ZLIB_BUFF_SIZE;
	      stream.next_out = gzipbuff + ZLIB_BUFF_SIZE;
           }
           if (stream.total_out == RealSize)
              res = Z_STREAM_END;
        }
	::inflateEnd (&stream);
   }
   else //if not compressed
   {
      if (::ReadFile(h_in, filebuff, RealSize, &i, NULL)==0)
      {
         ErrorType = ERR_UNKNOWN;
         lError = true;
         goto err;
      }
   }

   if (Temp)
     h_out = CreateFile(FullName.c_str(),
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_TEMPORARY,
		NULL);
   else
     h_out = CreateFile(FullName.c_str(),
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_ARCHIVE,
		NULL);

     if (h_out == INVALID_HANDLE_VALUE)
     {
         ErrorType = ERR_CANNOT_CREATE_OUT_FILE;
         lError = true;
         goto err;
     }

     if (FileType)
     {
        if (WriteFile(h_out, gzipbuff, RealSize, &i, NULL) == 0) //compressed
        {
            ErrorType = ERR_CANNOT_WRITE_TO_FILE;
            lError = true;
            goto err;
        }
     }
     else
     {
        if (WriteFile(h_out, filebuff, RealSize, &i, NULL) == 0) //not compressed
        {
            ErrorType = ERR_CANNOT_WRITE_TO_FILE;
            lError = true;
            goto err;
        }
      }
err: //----------------LABEL-------------------
   if (h_out != INVALID_HANDLE_VALUE)
      CloseHandle(h_out);
   if (filebuff != NULL)
      free(filebuff);
   if (gzipbuff != NULL)
      free(gzipbuff);
}
//------------------------------------------------------------------------------
void TDatFile::ImportFiles(TStrings *FilesList,
                             String SourceFolder, String DestinationFolder)
{
   String TempFolder = "";
   unsigned long i;  //WARNING !!! Это процедура очень опасна для DAT
   CancelOp = false;
   lError = false;

   i = SetFilePointer (h_in, -(TreeSize+8), NULL, FILE_END);
   if (!SetEndOfFile(h_in))
   {
       ErrorType = ERR_UNKNOWN;
       ShowError();
       return;
   }  // Хвост обрезан ! (Осталась одна дата)

   HANDLE h_tmptree = CreateFile("c:\\temptree.dat",
		GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,
		NULL);

   if (h_tmptree == INVALID_HANDLE_VALUE)
   {
       ErrorType = ERR_CANNOT_CREATE_OUT_FILE;
       ShowError();
       return;
   } // Создаём и открываем временный файл для хранения импортированного дерева.

   Offset = FileSizeFromDat - (TreeSize + 8); //Смещение для нового имп. файла
   tmpTreeSize = 0;                         //Размер нового дерева

   for (int I = 0; I < FilesList->Count; I ++)
   {
      if (SourceFolder != 0)
      {
         FullName = FilesList->Strings[I].SubString(SourceFolder.Length() + 2, 0xFFFF);
         TempFolder = GetPathName();
      }
      if (!ImportFile(h_in, h_tmptree, DestinationFolder + TempFolder,
                                        FilesList->Strings[I]) || CancelOp)
         break;
      else
      {
         Application->ProcessMessages();
      }
   }
   WriteFile(h_in, &FilesTotal, 4, &i, NULL);
   i = SetFilePointer (h_tmptree, 0, NULL, FILE_BEGIN);
   BYTE *pTmpBuff = (BYTE *)malloc(tmpTreeSize);
   ReadFile(h_tmptree, pTmpBuff, tmpTreeSize, &i, NULL);
   WriteFile(h_in, buff, TreeSize - 4, &i, NULL); //TreeSize-4 - без FilesTotal
   WriteFile(h_in, pTmpBuff, tmpTreeSize, &i, NULL);
   free(pTmpBuff);
   TreeSize += tmpTreeSize;
   WriteFile(h_in, &TreeSize, 4, &i, NULL);
   WriteFile(h_in, &FileSizeFromDat, 4, &i, NULL);

   CloseHandle(h_tmptree);

   ReadTree();
   if (lError)
      ShowError();
}
//------------------------------------------------------------------------------
bool TDatFile::ImportFile(HANDLE h_tmpdat, HANDLE h_tmptree, 
                          String DestinationFolder, String impfilename)
{
   unsigned long i;
   unsigned __int64 percent;
   filebuff = NULL;
   gzipbuff = NULL;

   h_imp = CreateFile(impfilename.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
   if (h_imp == INVALID_HANDLE_VALUE)
   {
       ErrorType = ERR_CANNOT_OPEN_FILE;
       lError = true;
       return false;
   }

   i = SetFilePointer (h_imp, 0, NULL, FILE_BEGIN);
   RealSize = GetFileSize(h_imp, NULL);
   if (RealSize == 0)
   {
      ErrorType = ERR_FILE_TRUNCATED;
      lError = true;
      CloseHandle(h_imp);
      return false;
   }

   gzipbuff = (char *) malloc(RealSize);
   if (gzipbuff == NULL)
   {
      ErrorType = ERR_ALLOC_MEMORY;
      lError = true;
      CloseHandle(h_imp);
      return false;
   }

   z_stream stream;
   stream.zalloc = Z_NULL;
   stream.zfree = Z_NULL;
   stream.opaque = Z_NULL;
   stream.next_in = Z_NULL;
   stream.avail_in = 0;
   if (::deflateInit (&stream, Z_BEST_SPEED) != Z_OK)// Z_DEFAULT_COMPRESSION 
   {
      ErrorType = ERR_UNKNOWN;
      lError = true;
      CloseHandle(h_imp);
      return false;
   }

   stream.next_out = gzipbuff;   //Containing compressed Data
   stream.avail_out = RealSize;// RealSize

   int res = Z_OK;

   while ( res == Z_OK )
   {
      if (stream.avail_in == 0) // stream.avail_in == 0
      {
          stream.next_in = m_pInBuf;
          if(::ReadFile(h_imp, m_pInBuf, ZLIB_BUFF_SIZE, (unsigned long*)&stream.avail_in, NULL)==0)
          {
             ErrorType = ERR_UNKNOWN;
             lError = true;
             return false;
          }
      }

      // DEFLATING BUFFER
      res = ::deflate (&stream, Z_PARTIAL_FLUSH); //Z_PARTIAL_FLUSH, Z_FINISH, Z_FULL_FLUSH, Z_NO_FLUSH

      if (stream.avail_out == 0 && stream.total_in < RealSize)//stream.avail_out == 0 || res == Z_STREAM_END
      {
	   stream.avail_out = ZLIB_BUFF_SIZE; //ZLIB_BUFF_SIZE
	   stream.next_out = gzipbuff + ZLIB_BUFF_SIZE;
      }
      if (stream.total_in >= RealSize) //
         res = ::deflate (&stream, Z_FINISH); //Z_PARTIAL_FLUSH, Z_FINISH
   }
   ::deflateEnd (&stream);

   PackedSize = stream.total_out;

   FileType = 1; // File Packed
   if (PackedSize >= RealSize)
   {
      // Если нет смысла сжимать файл, то он переписывается в оригинале
      i = SetFilePointer (h_imp, 0, NULL, FILE_BEGIN);
      if(::ReadFile(h_imp, gzipbuff, RealSize, &i, NULL) == 0)
      {
         ErrorType = ERR_UNKNOWN;
         lError = true;
         return false;
      }
      PackedSize = RealSize;
      FileType = 0;//File Normal
   }
   if (WriteFile(h_tmpdat, gzipbuff, PackedSize, &i, NULL) == 0)
   {
       ErrorType = ERR_CANNOT_WRITE_TO_FILE;
       lError = true;
       return false;
   }

   FullName = impfilename;
   FullName = DestinationFolder + GetFileName(true);
   char *TempString = FullName.c_str();
   FileNameSize = FullName.Length();
   ULONG NeedBytes = 17 + FileNameSize;
   BYTE *pTmpTreeBuf = (BYTE *)malloc(NeedBytes);
   memcpy(pTmpTreeBuf, &FileNameSize, 4);
   memcpy(pTmpTreeBuf + 4, TempString, FullName.Length());
   memcpy(pTmpTreeBuf + 4 + FileNameSize, &FileType, 1);
   memcpy(pTmpTreeBuf + 5 + FileNameSize, &RealSize, 4);
   memcpy(pTmpTreeBuf + 9 + FileNameSize, &PackedSize, 4);
   memcpy(pTmpTreeBuf + 13 + FileNameSize, &Offset, 4);
   if (WriteFile(h_tmptree, pTmpTreeBuf, NeedBytes, &i, NULL) == 0)
   {
       ErrorType = ERR_CANNOT_WRITE_TO_FILE;
       lError = true;
       return false;
   }
   free(pTmpTreeBuf);

   Offset += PackedSize;
   tmpTreeSize += NeedBytes;
   FilesTotal ++;
   FileSizeFromDat += PackedSize + NeedBytes;

   CloseHandle(h_imp);
   if (filebuff != NULL)
      free(filebuff);
   if (gzipbuff != NULL)
      free(gzipbuff);
   return true;
}
//------------------------------------------------------------------------------
UINT TDatFile::CalculateFiles(String Mask)
{
   ULONG FilesTotal = 0;
   ResetPtr();
   while (FindFile(Mask))
      FilesTotal ++;
   return FilesTotal;
}
//------------------------------------------------------------------------------
String TDatFile::GetPathName(void)
{
   return FullName.SubString(1, FullName.LastDelimiter("\\"));
}
//------------------------------------------------------------------------------
String TDatFile::GetFileName(bool WithExtension)
{
   int nPosition = FullName.LastDelimiter("\\");
   if (WithExtension)
      return FullName.SubString(nPosition + 1, 255);
   return FullName.SubString(nPosition + 1,
                    FullName.LastDelimiter(".") - nPosition - 1);

}
//------------------------------------------------------------------------------
String TDatFile::GetDirOrFile(String Mask)
{
   int n;
   String LastName;
   LastName = FullName.SubString(Mask.Length() + 1, FileNameSize);
   if (n = LastName.Pos("\\"))
      return LastName.SubString(0, n);
   return LastName;
}
//------------------------------------------------------------------------------
void TDatFile::RevDw(DWORD *addr)
{
   BYTE *b, tmp;
   b = (BYTE*)addr;
   tmp = *(b + 3);
   *(b + 3) = *b;
   *b = tmp;
   tmp = *(b + 2);
   *(b + 2) = *(b + 1);
   *(b + 1) = tmp;
}
//------------------------------------------------------------------------------
void TDatFile::ShowError(void)
{
   if (lError)
      MessageBox(NULL, *(error_types + ErrorType), "Error", MB_OK);
   lError = false;
}
//------------------------------------------------------------------------------
TDatFile::~TDatFile()
{
   if (h_in != INVALID_HANDLE_VALUE)
      CloseHandle(h_in);
   if (m_pInBuf != NULL)
      free(m_pInBuf);
   if (buff != NULL)
      free(buff);
   if (lstPaths)
      delete lstPaths;  // delete cache
   if (pPtrs)
      delete pPtrs;  // delete cache
   if (reader)
      delete reader;
}
