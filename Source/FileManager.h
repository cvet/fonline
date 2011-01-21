#ifndef __FILE_MANAGER__
#define __FILE_MANAGER__

#include "Defines.h"
#include "Log.h"
#include "DataFile.h"

// Client and mapper paths
#define PT_ROOT                (0)
#define PT_DATA                (1)
#define PT_ART                 (2)
#define PT_ART_CRITTERS        (3)
#define PT_ART_INTRFACE        (4)
#define PT_ART_INVEN           (5)
#define PT_ART_ITEMS           (6)
#define PT_ART_MISC            (7)
#define PT_ART_SCENERY         (8)
#define PT_ART_SKILLDEX        (9)
#define PT_ART_SPLASH          (10)
#define PT_ART_TILES           (11)
#define PT_ART_WALLS           (12)
#define PT_TEXTURES            (13)
#define PT_EFFECTS             (14)
#define PT_MAPS                (15)
#define PT_TERRAIN             (16)
#define PT_SND_MUSIC           (17)
#define PT_SND_SFX             (18)
#define PT_SCRIPTS             (19)
#define PT_VIDEO               (20)
#define PT_TEXTS               (21)
#define PT_SAVE                (22)
#define PT_FONTS               (23)
#define PT_CACHE               (24)

// Server paths
#define PT_SERVER_ROOT         (30)
#define PT_SERVER_DATA         (31)
#define PT_SERVER_TEXTS        (32)
#define PT_SERVER_DIALOGS      (33)
#define PT_SERVER_MAPS         (34)
#define PT_SERVER_PRO_ITEMS    (35)
#define PT_SERVER_PRO_CRITTERS (36)
#define PT_SERVER_SCRIPTS      (37)
#define PT_SERVER_SAVE         (38)
#define PT_SERVER_CLIENTS      (39)
#define PT_SERVER_BANS         (40)

// Other
#define PT_MAPPER_DATA         (45)

extern char PathLst[][50];
#define PATH_LIST_COUNT     (50)

class FileManager
{
public:
	static void SetDataPath(const char* path);
	static void InitDataFiles(const char* path);
	static bool LoadDataFile(const char* path);
	static void EndOfWork();

	bool LoadFile(const char* fname, int path_type);
	bool LoadStream(BYTE* stream, DWORD length);
	void UnloadFile();
	BYTE* ReleaseBuffer();

	void SetCurPos(DWORD pos);
	void GoForward(DWORD offs);
	void GoBack(DWORD offs);
	bool FindFragment(const BYTE* fragment, DWORD fragment_len, DWORD begin_offs);

	bool GetLine(char* str, DWORD len);
	bool CopyMem(void* ptr, size_t size);
	void GetStr(char* str);
	BYTE GetByte();
	WORD GetBEWord();
	WORD GetLEWord();
	DWORD GetBEDWord();
	DWORD GetLEDWord();
	DWORD GetLE3Bytes();
	float GetBEFloat();
	float GetLEFloat();
	int GetNum();

	void SwitchToRead();
	void SwitchToWrite();
	void ClearOutBuf();
	bool ResizeOutBuf();
	void SetPosOutBuf(DWORD pos);
	bool SaveOutBufToFile(const char* fname, int path_type);
	BYTE* GetOutBuf(){return dataOutBuf;}
	DWORD GetOutBufLen(){return endOutBuf;}

	void SetData(void* data, DWORD len);
	void SetStr(const char* fmt, ...);
	void SetByte(BYTE data);
	void SetBEWord(WORD data);
	void SetLEWord(WORD data);
	void SetBEDWord(DWORD data);
	void SetLEDWord(DWORD data);

	static const char* GetFullPath(const char* fname, int path_type);
	static void GetFullPath(const char* fname, int path_type, char* get_path);
	static const char* GetPath(int path_type);
	static const char* GetDataPath(int path_type);
	static void FormatPath(char* path);
	static void ExtractPath(const char* fname, char* path);
	static void ExtractFileName(const char* fname, char* name);
	static const char* GetExtension(const char* fname);

	bool IsLoaded(){return fileBuf!=NULL;}
	BYTE* GetBuf(){return fileBuf;}
	BYTE* GetCurBuf(){return fileBuf+curPos;}
	DWORD GetCurPos(){return curPos;}
	DWORD GetFsize(){return fileSize;}
	bool IsEOF(){return curPos>=fileSize;}
	void GetTime(FILETIME* create, FILETIME* access, FILETIME* write);
	int ParseLinesInt(const char* fname, int path_type, IntVec& lines);

	static DataFileVec& GetDataFiles(){return dataFiles;}
	static void GetFolderFileNames(int path_type, const char* ext, StrVec& result); // Note: include subdirs
	static void GetDatsFileNames(int path_type, const char* ext, StrVec& result); // Note: include subdirs

	FileManager():dataOutBuf(NULL),posOutBuf(0),endOutBuf(0),lenOutBuf(0),fileSize(0),curPos(0),fileBuf(NULL){};
	~FileManager(){UnloadFile(); ClearOutBuf();}

private:
	static char dataPath[MAX_FOPATH];
	static DataFileVec dataFiles;

	DWORD fileSize;
	BYTE* fileBuf;
	DWORD curPos;

	BYTE* dataOutBuf;
	DWORD posOutBuf;
	DWORD endOutBuf;
	DWORD lenOutBuf;

	FILETIME timeCreate,timeAccess,timeWrite;

	static void RecursiveDirLook(const char* init_dir, const char* ext, StrVec& result);
};

#endif // __FILE_MANAGER__