#ifndef _CFILEMGR_H_
#define _CFILEMGR_H_

#include "common.h"

/********************************************************************
author:	Oleg Mareskin
edit: Anton Tsvetinsky aka Cvet
*********************************************************************/

//!Cvet ++++

//разкомментировать, если требуется работа с DAT файлами
//#define FM_USE_DAT_MANAGER

//!Cvet ----

#ifdef FM_USE_DAT_MANAGER //!Cvet
#include "datfile.h"
#endif

#define OUT_BUF_START_SIZE	32 //!Cvet стартовый размер исходящего буфера

#define PT_ART_CRITTERS 0
#define PT_ART_INTRFACE 1
#define PT_ART_INVEN    2
#define PT_ART_ITEMS    3
#define PT_ART_MISC     4
#define PT_ART_SCENERY  5
#define PT_ART_SKILLDEX 6
#define PT_ART_SPLASH   7
#define PT_ART_TILES    8
#define PT_ART_WALLS    9

#define PT_MAPS         10

#define PT_PRO_ITEMS    11
#define PT_PRO_MISC     12
#define PT_PRO_SCENERY  13
#define PT_PRO_TILES    14
#define PT_PRO_WALLS    15

#define PT_SND_MUSIC    16
#define PT_SND_SFX      17
	
#define PT_TXT_GAME     18

#define PT_SRV_MAPS		19 //!Cvet server path
#define PT_SRV_MASKS	20 //!Cvet server path
#define PT_SRV_OBJECTS	21 //!Cvet server path

extern char pathlst[][50];

class CFileMngr
{
public:
	int Init(char* data_path);
	void Clear();
	void UnloadFile();
	int LoadFile(char* fname, int PathType);

	void SetCurPos(DWORD pos);
	void GoForward(DWORD offs);

	int GetStr(char* str,DWORD len);

	//!Cvet заодно переименовал функции добавив в имя BE и LE. т.е. Big-endian и Little-endian

	BYTE GetByte(); //!Cvet считывание байта
	WORD GetBEWord();
	WORD GetLEWord(); //!Cvet в little endian
	DWORD GetBEDWord();
	DWORD GetLEDWord(); //!Cvet в little endian

	//!Cvet +++++++++++
//для работы с выходным буфером. для записи инфы в файл.
	void ClearOutBuf();
	void ResizeOutBuf();

	void SetPosOutBuf(UINT pos);

	BOOL SaveOutBufToFile(char* fname, int PathType);

	void SetByte(BYTE data);
	void SetBEWord(WORD data);
	void SetLEWord(WORD data);
	void SetBEDWord(DWORD data);
	void SetLEDWord(DWORD data);

//проверка существования файла и возврат полного путя к нему
	BOOL GetFullPath(char* fname, int PathType, char* get_path);
//полный путь к файлу
	void GetFullPath(int PathType, char* get_path);
	//!Cvet -----------

	int CopyMem(void* ptr, size_t size);

	BYTE* GetBuf(){ return buf; }; //!Cvet
	DWORD GetFsize(){ return fsize; }; //!Cvet


	CFileMngr(): crtd(0),hFile(NULL),

#ifdef FM_USE_DAT_MANAGER //!Cvet
		lpDAT(NULL),lpDATcr(NULL),
#endif

		obuf(NULL),pos_obuf(0),end_obuf(0),len_obuf(0), //!Cvet
		fsize(0),cur_pos(0),buf(NULL){};
private:
	bool crtd;

	HANDLE hFile;
	DWORD fsize;
	DWORD cur_pos;
	BYTE* buf;

	BYTE* obuf; //!Cvet
	UINT pos_obuf; //!Cvet
	UINT end_obuf; //!Cvet
	UINT len_obuf; //!Cvet

#ifdef FM_USE_DAT_MANAGER //!Cvet
	TDatFile *lpDAT,*lpDATcr;

	char master_dat[1024];
	char crit_dat[1024];
#endif

	char fo_dat[1024];

	void LoadSimple(HANDLE hFile);
};

#endif