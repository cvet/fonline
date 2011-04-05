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
#define PT_SND_MUSIC           (16)
#define PT_SND_SFX             (17)
#define PT_SCRIPTS             (18)
#define PT_VIDEO               (19)
#define PT_TEXTS               (20)
#define PT_SAVE                (21)
#define PT_FONTS               (22)
#define PT_CACHE               (23)

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

#define PATH_LIST_COUNT        (50)
extern const char* PathLst[PATH_LIST_COUNT];

class FileManager
{
public:
	static void SetDataPath(const char* path);
	static void SetCacheName(const char* name);
	static void InitDataFiles(const char* path);
	static bool LoadDataFile(const char* path);
	static void EndOfWork();

	bool LoadFile(const char* fname, int path_type);
	bool LoadStream(const uchar* stream, uint length);
	void UnloadFile();
	uchar* ReleaseBuffer();

	void SetCurPos(uint pos);
	void GoForward(uint offs);
	void GoBack(uint offs);
	bool FindFragment(const uchar* fragment, uint fragment_len, uint begin_offs);

	bool GetLine(char* str, uint len);
	bool CopyMem(void* ptr, uint size);
	void GetStr(char* str);
	uchar GetUChar();
	ushort GetBEUShort();
	ushort GetLEUShort();
	uint GetBEUInt();
	uint GetLEUInt();
	uint GetLE3UChar();
	float GetBEFloat();
	float GetLEFloat();
	int GetNum();

	void SwitchToRead();
	void SwitchToWrite();
	void ClearOutBuf();
	bool ResizeOutBuf();
	void SetPosOutBuf(uint pos);
	bool SaveOutBufToFile(const char* fname, int path_type);
	uchar* GetOutBuf(){return dataOutBuf;}
	uint GetOutBufLen(){return endOutBuf;}

	void SetData(void* data, uint len);
	void SetStr(const char* fmt, ...);
	void SetUChar(uchar data);
	void SetBEUShort(ushort data);
	void SetLEUShort(ushort data);
	void SetBEUInt(uint data);
	void SetLEUInt(uint data);

	static const char* GetFullPath(const char* fname, int path_type);
	static void GetFullPath(const char* fname, int path_type, char* get_path);
	static const char* GetPath(int path_type);
	static const char* GetDataPath(int path_type);
	static void FormatPath(char* path);
	static void ExtractPath(const char* fname, char* path);
	static void ExtractFileName(const char* fname, char* name);
	static void MakeFilePath(const char* name, const char* path, char* result);
	static const char* GetExtension(const char* fname);
	static void EraseExtension(char* fname);

	bool IsLoaded(){return fileBuf!=NULL;}
	uchar* GetBuf(){return fileBuf;}
	uchar* GetCurBuf(){return fileBuf+curPos;}
	uint GetCurPos(){return curPos;}
	uint GetFsize(){return fileSize;}
	bool IsEOF(){return curPos>=fileSize;}
	void GetTime(uint64* create, uint64* access, uint64* write);
	int ParseLinesInt(const char* fname, int path_type, IntVec& lines);

	static DataFileVec& GetDataFiles(){return dataFiles;}
	static void GetFolderFileNames(const char* path, bool include_subdirs, const char* ext, StrVec& result);
	static void GetDatsFileNames(const char* path, bool include_subdirs, const char* ext, StrVec& result);

	FileManager():dataOutBuf(NULL),posOutBuf(0),endOutBuf(0),lenOutBuf(0),fileSize(0),curPos(0),fileBuf(NULL){};
	~FileManager(){UnloadFile(); ClearOutBuf();}

private:
	static char dataPath[MAX_FOPATH];
	static DataFileVec dataFiles;

	uint fileSize;
	uchar* fileBuf;
	uint curPos;

	uchar* dataOutBuf;
	uint posOutBuf;
	uint endOutBuf;
	uint lenOutBuf;

	uint64 timeCreate,timeAccess,timeWrite;

	static void RecursiveDirLook(const char* init_dir, bool include_subdirs, const char* ext, StrVec& result);
};

#endif // __FILE_MANAGER__