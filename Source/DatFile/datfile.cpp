#include "datfile.h"

#define VER_FALLOUT1             (0x00000013)
#define VER_FALLOUT2             (0x10000014)

#define ZLIB_BUFF_SIZE           (0x10000)
#define GZIP_MODE1               (0x0178)
#define GZIP_MODE2               (0xDA78)

char *error_types[] = {
	"Cannot open file.",                       // ERR_CANNOT_OPEN_FILE
	"File invalid or truncated.",              // ERR_FILE_TRUNCATED
	"This file not supported.",                // ERR_FILE_NOT_SUPPORTED
	"Not enough memory to allocate buffer.",   // ERR_ALLOC_MEMORY
	"is_fallout1 dat files are not supported.",   // ERR_FILE_NOT_SUPPORTED2
};


TDatFile::TDatFile(const char* filename)
{
	IsError = true;
	Tree = NULL;
	Reader = NULL; // Initially empty reader. We don't know its type at this point
	Datname = filename;

	DatHandle = CreateFile(filename,  // В DatHandle находится HANDLE на DAT файл
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if(DatHandle == INVALID_HANDLE_VALUE)
	{
		ErrorType = ERR_CANNOT_OPEN_FILE;
		return;
	}

	if( (ErrorType = ReadTree()) != RES_OK)
	{
		return;
	}

	IsError = false;
}

TDatFile::~TDatFile()
{
	if(DatHandle!=INVALID_HANDLE_VALUE)
	{
		CloseHandle(DatHandle);
		DatHandle=INVALID_HANDLE_VALUE;
	}
	if(Tree)
	{
		delete[] Tree;
		Tree=NULL;
	}
	if(Reader)
	{
		delete Reader;
		Reader=NULL;
	}
}

int TDatFile::ReadTree()
{
	DWORD i,f1_dir_count;
	bool is_fallout1=false;

	// Проверка на то, что файл не менее 8 байт
	i = SetFilePointer(DatHandle, -8, NULL, FILE_END);
	if(i == 0xFFFFFFFF) return ERR_FILE_TRUNCATED;

	// Чтение информации из DAT файла
	ReadFile(DatHandle, &TreeSize, 4, &i, NULL);
	ReadFile(DatHandle, &FileSizeFromDat, 4, &i, NULL);

	i = SetFilePointer(DatHandle, 0, NULL, FILE_BEGIN); // Added for is_fallout1
	ReadFile(DatHandle, &f1_dir_count, 4, &i, NULL); // Added for is_fallout1
	RevDw(&f1_dir_count); // Added for is_fallout1
	if(f1_dir_count == 0x01 || f1_dir_count == 0x33) is_fallout1 = true; // Added for is_fallout1
	if(GetFileSize(DatHandle, NULL) != FileSizeFromDat && is_fallout1 == false) return ERR_FILE_NOT_SUPPORTED;
	if(is_fallout1) return ERR_FILE_NOT_SUPPORTED2; // Fallout 1

	i = SetFilePointer (DatHandle, -((LONG)TreeSize + 8), NULL, FILE_END);
	ReadFile(DatHandle, &FilesTotal, 4, &i, NULL);
	TreeSize-=4;

	if((Tree = new BYTE[TreeSize]) == NULL) return ERR_ALLOC_MEMORY;
	ZeroMemory(Tree, TreeSize);

	ReadFile(DatHandle, Tree, TreeSize, &i, NULL);
	IndexingDAT();
	return RES_OK;
}

void TDatFile::RevDw(DWORD* addr)
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

const char* TDatFile::GetError(int err)
{
	return *(error_types + err);
}

void TDatFile::IndexingDAT()
{
	char name[1024];
	BYTE* ptr=Tree;
	BYTE* end_ptr=Tree+TreeSize;
	while(true)
	{
		DWORD fnsz=*(DWORD*)ptr;

		if(fnsz && fnsz<1024)
		{
			memcpy(name,ptr+4,fnsz);
			name[fnsz]=0;
			_strlwr_s(name);
			Index.insert(IndexMapVal(name,ptr+4+fnsz));
		}

		if(ptr+fnsz+17>=end_ptr) break;
		ptr+=fnsz+17;
	}
}

HANDLE TDatFile::DATOpenFile(const char* fname)
{
	if(DatHandle != INVALID_HANDLE_VALUE)
	{
		if(FindFile(fname))
		{
			if(Reader) // If we still have old non-closed reader - we kill it
			{
				delete Reader;
				Reader=NULL;
			}

#pragma message("Bug!")
//		ntdll.dll!77182071() 	
//  	[Frames below may be incorrect and/or missing, no symbols loaded for ntdll.dll]	
// >	FOnline.exe!malloc(unsigned int size=9520)  Line 163 + 0x5f bytes	C
//  	FOnline.exe!_zcalloc()  + 0xf bytes	C
//  	FOnline.exe!_inflateInit2_()  + 0x5e bytes	C
//  	FOnline.exe!_inflateInit_()  + 0x16 bytes	C
//  	FOnline.exe!C_Z_PackedFile::C_Z_PackedFile()  + 0x5f bytes	C++
//  	FOnline.exe!TDatFile::DATOpenFile()  + 0x8b bytes	C++
//  	FOnline.exe!FileManager::LoadFile()  + 0x1c1 bytes	C++
//  	FOnline.exe!ResourceMngr::LoadList()  + 0x31 bytes	C++
//  	FOnline.exe!ResourceMngr::Init()  + 0x3b bytes	C++
//  	FOnline.exe!CFEngine::Init()  + 0xd9b bytes	C++
//  	FOnline.exe!_WinMain@16()  + 0x20d bytes	C++
//  	FOnline.exe!__tmainCRTStartup()  Line 263 + 0x1b bytes	C

// 		ntdll.dll!77966739() 	
//  	[Frames below may be incorrect and/or missing, no symbols loaded for ntdll.dll]	
//  	ntdll.dll!779666ea() 	
// >	FOnline.exe!free(void * pBlock=0x04065af0)  Line 110	C
//  	FOnline.exe!CPackedFile::~CPackedFile()  + 0x53 bytes	C++
//  	FOnline.exe!C_Z_PackedFile::`scalar deleting destructor'()  + 0x1c bytes	C++
//  	FOnline.exe!TDatFile::DATOpenFile()  + 0x2e bytes	C++
//  	FOnline.exe!CFileMngr::LoadFile()  + 0x1c1 bytes	C++
//  	FOnline.exe!CSpriteManager::LoadFont()  + 0xbc bytes	C++
//  	FOnline.exe!CFEngine::Init()  + 0x853 bytes	C++
//  	FOnline.exe!_WinMain@16()  + 0x205 bytes	C++
//  	FOnline.exe!__tmainCRTStartup()  Line 263 + 0x1b bytes	C

// 		ntdll.dll!77285ba4() 	
//  	[Frames below may be incorrect and/or missing, no symbols loaded for ntdll.dll]	
// >	FOnline.exe!malloc(unsigned int size=262144)  Line 163 + 0x5f bytes	C
//  	FOnline.exe!operator new(unsigned int size=262144)  Line 59 + 0x8 bytes	C++
//  	FOnline.exe!C_Z_PackedFile::read()  + 0x3f bytes	C++
//  	FOnline.exe!TDatFile::DATReadFile()  + 0x2e bytes	C++
//  	FOnline.exe!CFileMngr::LoadFile()  + 0x212 bytes	C++
//  	FOnline.exe!CSpriteManager::LoadFontAAF()  + 0x63 bytes	C++
//  	FOnline.exe!CFEngine::Init()  + 0x913 bytes	C++
//  	FOnline.exe!_WinMain@16()  + 0x205 bytes	C++
//  	FOnline.exe!__tmainCRTStartup()  Line 263 + 0x1b bytes	C

//  	ntdll.dll!77413c4f() 	
//  	[Frames below may be incorrect and/or missing, no symbols loaded for ntdll.dll]	
// >	FOnline.exe!malloc(unsigned int size=262144)  Line 163 + 0x5f bytes	C
//  	FOnline.exe!operator new(unsigned int size=262144)  Line 59 + 0x8 bytes	C++
//  	FOnline.exe!C_Z_PackedFile::read()  + 0x3f bytes	C++
//  	FOnline.exe!TDatFile::DATReadFile()  + 0x2e bytes	C++
//  	FOnline.exe!CFileMngr::LoadFile()  + 0x212 bytes	C++
//  	FOnline.exe!CSpriteManager::LoadFont()  + 0xbc bytes	C++
//  	FOnline.exe!CFEngine::Init()  + 0x89b bytes	C++
//  	FOnline.exe!_WinMain@16()  + 0x205 bytes	C++
//  	FOnline.exe!__tmainCRTStartup()  Line 263 + 0x1b bytes	C

// >	FOnline.exe!malloc(unsigned int size=262144)  Line 163 + 0x5f bytes	C
//  	FOnline.exe!operator new(unsigned int size=262144)  Line 59 + 0x8 bytes	C++
//  	FOnline.exe!C_Z_PackedFile::read()  + 0x3f bytes	C++
//  	FOnline.exe!TDatFile::DATReadFile()  + 0x2e bytes	C++
//  	FOnline.exe!CFileMngr::LoadFile()  + 0x212 bytes	C++
//  	FOnline.exe!CSpriteManager::LoadFontAAF()  + 0x63 bytes	C++
//  	FOnline.exe!CFEngine::Init()  + 0x913 bytes	C++
//  	FOnline.exe!_WinMain@16()  + 0x205 bytes	C++
//  	FOnline.exe!__tmainCRTStartup()  Line 263 + 0x1b bytes	C

//  	ntdll.dll!777d3c4f() 	
//  	[Frames below may be incorrect and/or missing, no symbols loaded for ntdll.dll]	
// >	FOnline.exe!malloc(unsigned int size=262144)  Line 163 + 0x5f bytes	C
//  	FOnline.exe!operator new(unsigned int size=262144)  Line 59 + 0x8 bytes	C++
//  	FOnline.exe!C_Z_PackedFile::read()  + 0x3f bytes	C++
//  	FOnline.exe!TDatFile::DATReadFile()  + 0x2e bytes	C++
//  	FOnline.exe!CFileMngr::LoadFile()  + 0x212 bytes	C++
//  	FOnline.exe!CSpriteManager::LoadFont()  + 0xbc bytes	C++
//  	FOnline.exe!CFEngine::Init()  + 0x853 bytes	C++
//  	FOnline.exe!_WinMain@16()  + 0x205 bytes	C++
//  	FOnline.exe!__tmainCRTStartup()  Line 263 + 0x1b bytes	C

//  	ntdll.dll!771859c3() 	
//  	[Frames below may be incorrect and/or missing, no symbols loaded for ntdll.dll]	
// >	FOnline.exe!__crtLCMapStringA(localeinfo_struct * plocinfo=0x024e0000, unsigned long Locale=0, unsigned long dwMapFlags=66673392, const char * lpSrcStr=0x1ac27b62, int cchSrc=43517560, char * lpDestStr=0x029cfe00, int cchDest=42866612, int code_page=42866612, int bError=1241744)  Line 376 + 0x20 bytes	C++
//  	FOnline.exe!free(void * pBlock=0x03f95af0)  Line 110	C
//  	FOnline.exe!CPackedFile::~CPackedFile()  + 0x53 bytes	C++
//  	FOnline.exe!C_Z_PackedFile::`scalar deleting destructor'()  + 0x1c bytes	C++
//  	FOnline.exe!TDatFile::DATOpenFile()  + 0x2e bytes	C++
//  	FOnline.exe!CFileMngr::LoadFile()  + 0x1c1 bytes	C++
//  	FOnline.exe!CSpriteManager::LoadFont()  + 0xbc bytes	C++
//  	FOnline.exe!CFEngine::Init()  + 0x853 bytes	C++
//  	FOnline.exe!_WinMain@16()  + 0x205 bytes	C++
//  	FOnline.exe!__tmainCRTStartup()  Line 263 + 0x1b bytes	C

			if(!FileType) Reader = new CPlainFile (DatHandle, Offset, RealSize);
			else Reader = new C_Z_PackedFile (DatHandle, Offset, RealSize, PackedSize);
			return DatHandle;
		}
	}

	return INVALID_HANDLE_VALUE;
}

bool TDatFile::FindFile(const char* fname)
{
	IndexMapIt it=Index.find(fname);
	if(it==Index.end()) return false;

	BYTE* ptr=(*it).second;
	FileType=*ptr;
	RealSize=*(DWORD*)(ptr+1);
	PackedSize=*(DWORD*)(ptr+5);
	Offset=*(DWORD*)(ptr+9);
	return true;
}

bool TDatFile::DATSetFilePointer(LONG lDistanceToMove, DWORD dwMoveMethod)
{
	if(DatHandle == INVALID_HANDLE_VALUE) return false;
	Reader->seek (lDistanceToMove, dwMoveMethod);
	return true;
}

DWORD TDatFile::DATGetFileSize(void)
{
	if(DatHandle == INVALID_HANDLE_VALUE) return 0;
	return RealSize;
}

bool TDatFile::DATReadFile(LPVOID lpBuffer, DWORD nNumberOfBytesToRead)
{
	if(DatHandle == INVALID_HANDLE_VALUE) return false;
	if(!lpBuffer) return false;
	if(!nNumberOfBytesToRead) return true;

	DWORD br;
	Reader->read (lpBuffer, nNumberOfBytesToRead, (long*)&br);
	return nNumberOfBytesToRead == br;
}

void TDatFile::GetFileNames(const char* path, const char* ext, std::vector<std::string>& result)
{
	size_t path_len=strlen(path);
	for(IndexMapIt it=Index.begin(),end=Index.end();it!=end;++it)
	{
		const std::string& fname=(*it).first;
		if(!fname.compare(0,path_len,path))
		{
			if(ext && *ext)
			{
				size_t pos=fname.find_last_of('.');
				if(pos!=std::string::npos && !_stricmp(&fname.c_str()[pos+1],ext)) result.push_back(fname);
			}
			else
			{
				result.push_back(fname);
			}
		}
	}
}






