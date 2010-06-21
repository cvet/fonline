#ifndef __SQL_H__
#define __SQL_H__

/********************************************************************
	created:	end of 2006; updated: begin of 2007
	author:		Anton Tsvetinskiy
	
	excluded from project 11.11.2009 22.47
*********************************************************************/

#include <mysql5inc\mysql.h>
#pragma comment(lib,"libmySQL.lib")
#include <NetProto.h>
#include <stack>
#include <FOdefines.h>

//********************************************************************
//********************************************************************
//********************************************************************

#define SQL_MAX_TABLE_NAME	64
typedef MYSQL_ROW SQL_RESULT;
typedef MYSQL_RES * SQL_RES;
typedef char ** SQL_ROW;

class CSQL
{
public:
	CSQL();
	~CSQL();

	bool Init();
	//bool Connect();
	void Finish();
	bool Ping();
	MYSQL* GetMySql(){return mySQL;}
	const char* GetDataBaseName(){return db;}

	bool IsInit(){return isInit;}

//	bool Query(const char* query);
	bool Query(const char* query, ...);
	bool RQuery(const char* query, DWORD len);

	//Для организации вложеннных запросов.
	SQL_ROW FetchRow(void);
	int	EscapeString(char * out_str, const char * str, int size);

	DWORD GetInsertId(){return (DWORD)mysql_insert_id(mySQL);}
	//int StoreResult(){return (mySQL_res=mysql_store_result(mySQL))?mysql_num_rows(mySQL_res):-1;} //Optimization error
	int StoreResult(){if(mySQL_res) mysql_free_result(mySQL_res); mySQL_res=mysql_store_result(mySQL); if(!mySQL_res) return -1; else return (int)mysql_num_rows(mySQL_res);}
	SQL_RESULT GetNextResult(){mySQL_row=mysql_fetch_row(mySQL_res); return mySQL_row;}
	DWORD* GetLengths(){return mysql_fetch_lengths(mySQL_res);}
	MYSQL_STMT* GetStatementSave(const char* query, MYSQL_BIND* bind);
	bool RunStatementSave(MYSQL_STMT* stmt);
	//bool RunStatementLoad(MYSQL_STMT* stmt);
	void CloseStatement(MYSQL_STMT* stmt);

	int GetInt(char* table, char* find_row, char* row, char* row_vol);
	int GetInt(char* table, char* find_row, char* row, int row_vol);
	bool GetChar(char* table, char* find_row, char* row, char* row_vol, char* result);
	bool GetChar(char* table, char* find_row, char* row, int row_vol, char* result);

	int CountRows(const char* query);
	int CountRows(char* table, char* column, char* count_vol);
	int CountRows(char* table, char* column, int count_vol);

	void AddCheat(DWORD user_id, char* text_cheat);
	int CountRow(char* table, char* row, char* value);
	int CountTable(char* table, char* row);
	void PrintTableInLog(char* table, char* rows);

	void BindPrepare(const char* query);
	void BindPush(enum_field_types type, void* data, bool is_unsigned);
	void BindPush(enum_field_types type, void* data, DWORD length);
	bool BindExecute();

private:
	bool isInit;

	MYSQL* mySQL;
	MYSQL_RES* mySQL_res;
	MYSQL_ROW mySQL_row;

	char user[64];
	char passwd[64];
	char host[64];
	char db[64];
	UINT port;
	char unix_socket[64];
	ULONG clientflag;

	char bindQuery[512];
	MYSQL_BIND bindBind[10];
	int bindCur,bindNeed;
};

extern CSQL Sql;

#endif //__SQL_H__