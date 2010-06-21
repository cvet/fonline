#ifndef __SQL_H__
#define __SQL_H__

#include "common.h"

/********************************************************************
	created:	end of 2006; updated: begin of 2007

	author:		Anton Tsvetinsky
	
	purpose:	
*********************************************************************/

//Переменные НПЦ
const BYTE NPC_VAR_LOCAL	=0;
const BYTE NPC_VAR_GLOBAL	=1;

//Выборка
const char CHOOSE_PLAYER		=0;
const char CHOOSE_ALL			=1;
const char CHOOSE_ALL_NOT_PLAYER=2;

typedef set<char> true_char_set;

typedef MYSQL_ROW SQL_RESULT;

class CSQL  
{
public:

	CSQL();
	~CSQL();

	MYSQL* mySQL;
	MYSQL_RES* mySQL_res;
	MYSQL_ROW mySQL_row;

	BOOL Init();
	void Finish();

//	BOOL Query(char* query, ...);
	BOOL Query(char* query);
	BOOL RQuery(char* query, DWORD len);

	BOOL StoreResult();
	SQL_RESULT GetNextResult();
	DWORD GetResultCount(){return mysql_num_rows(mySQL_res);};

	int GetInt(char* table, char* find_row, char* row, char* row_vol);
	int GetInt(char* table, char* find_row, char* row, int row_vol);
	BOOL GetChar(char* table, char* find_row, char* row, char* row_vol, char* result);
	BOOL GetChar(char* table, char* find_row, char* row, int row_vol, char* result);

	int CountRows(char* table, char* column, char* count_vol);
	int CountRows(char* table, char* column, int count_vol);

	int CountTable(char* table, char* row);

private:

	bool Active;

	char user[64];
	char passwd[64];
	char host[64];
	char db[64];
	UINT port;
	char unix_socket[64];
	ULONG clientflag;
};

#endif //__SQL_H__