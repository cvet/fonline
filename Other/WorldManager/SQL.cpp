/********************************************************************
	created:	end of 2006; updated: begin of 2007

	author:		Anton Tsvetinsky
	
	purpose:	
*********************************************************************/

#include "stdafx.h"
#include "SQL.h"

CSQL::CSQL()
{
	Active=false;
}

CSQL::~CSQL()
{

}

BOOL CSQL::Init()
{
	if(Active==true) return TRUE;

	WriteLog("mySQL init begin\n");

	mySQL=mysql_init(NULL);
	if(!mySQL)
	{
		WriteLog("ошибка инициализации\n");
		return FALSE;
	}

	GetPrivateProfileString("SQL","user"		,"root"		,&user[0]		,64,".\\foserv.cfg");
	GetPrivateProfileString("SQL","passwd"		,""			,&passwd[0]		,64,".\\foserv.cfg");
	GetPrivateProfileString("SQL","host"		,"localhost",&host[0]		,64,".\\foserv.cfg");
	GetPrivateProfileString("SQL","db"			,"fonline"	,&db[0]			,64,".\\foserv.cfg");
	GetPrivateProfileString("SQL","unix_socket" ,""			,&unix_socket[0],64,".\\foserv.cfg");

	port=GetPrivateProfileInt("SQL","port",3306,".\\foserv.cfg");
	clientflag=GetPrivateProfileInt("SQL","clientflag",0,".\\foserv.cfg");

	if(!mysql_real_connect(mySQL,&host[0],&user[0],&passwd[0],&db[0],port,&unix_socket[0],clientflag))
	{
		WriteLog("ошибка подключения (%s)\n",mysql_error(mySQL));
		return 0;
	}

	Active=true;

	WriteLog("mySQL init OK\n");

	return TRUE;
}

void CSQL::Finish()
{
	mysql_close(mySQL);
	mySQL=NULL;

	Active=false;
}
/*
BOOL CSQL::Query(char* query, ...)
{
	char str[2048];

	va_list list;
	va_start(list,query);
	wvsprintf(str,query,list);
	va_end(list);

	if(mysql_query(mySQL,str))
	{
		WriteLog("mySQL Query |%s| error: %s\n",str,mysql_error(mySQL));

		return FALSE;
	}

    return TRUE;
}*/

BOOL CSQL::Query(char* query)
{
	if(mysql_query(mySQL,query))
	{
		WriteLog("mySQL Query |%s| error: %s\n",query,mysql_error(mySQL));

		return FALSE;
	}

    return TRUE;
}

BOOL CSQL::RQuery(char* query, DWORD len)
{
	if(mysql_real_query(mySQL,query,len))
	{
		WriteLog("mySQL Query |%s| error: %s\n",query,mysql_error(mySQL));
		
		return FALSE;
	}
	
    return TRUE;
}

BOOL CSQL::StoreResult()
{
	if(!(mySQL_res=mysql_store_result(mySQL))) return FALSE;

	return TRUE;
}

SQL_RESULT CSQL::GetNextResult()
{
	mySQL_row=mysql_fetch_row(mySQL_res);

	return mySQL_row?mySQL_row:NULL;
}

int CSQL::GetInt(char* table, char* find_row, char* row, char* row_vol)
{
	char stradd[2048];
	sprintf(stradd,"SELECT `%s` FROM `%s` WHERE `%s`='%s'", find_row, table, row, row_vol);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog("mySQL GetInt1 error: %s\n", mysql_error(mySQL));
		return 0xFFFFFFFF;
	}

	if(!(mySQL_res=mysql_store_result(mySQL))) return 0xFFFFFFFF;

	if(!mysql_num_rows(mySQL_res)) return 0xFFFFFFFF;

	mySQL_row=mysql_fetch_row(mySQL_res);

	return atoi(mySQL_row[0]);
}

int CSQL::GetInt(char* table, char* find_row, char* row, int row_vol)
{
	char stradd[2048];
	sprintf(stradd,"SELECT `%s` FROM `%s` WHERE `%s`='%d'", find_row, table, row, row_vol);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog("mySQL GetInt2 error: %s\n", mysql_error(mySQL));
		return 0xFFFFFFFF;
	}

	if(!(mySQL_res=mysql_store_result(mySQL))) return 0xFFFFFFFF;

	if(!mysql_num_rows(mySQL_res)) return 0xFFFFFFFF;

	mySQL_row=mysql_fetch_row(mySQL_res);

	return atoi(mySQL_row[0]);
}

BOOL CSQL::GetChar(char* table, char* find_row, char* row, char* row_vol, char* result)
{
	char stradd[2048];
	sprintf(stradd,"SELECT `%s` FROM `%s` WHERE `%s`='%s'", find_row, table, row, row_vol);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog("mySQL GetChar1 error: %s\n", mysql_error(mySQL));
		return FALSE;
	}

	if(!(mySQL_res=mysql_store_result(mySQL))) return FALSE;

	if(!mysql_num_rows(mySQL_res)) return FALSE;

	mySQL_row=mysql_fetch_row(mySQL_res);

	strcpy(result,mySQL_row[0]);

	return TRUE;
}

BOOL CSQL::GetChar(char* table, char* find_row, char* row, int row_vol, char* result)
{
	char stradd[2048];
	sprintf(stradd,"SELECT `%s` FROM `%s` WHERE `%s`='%d'", find_row, table, row, row_vol);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog("mySQL GetChar2 error: %s\n", mysql_error(mySQL));
		return FALSE;
	}

	if(!(mySQL_res=mysql_store_result(mySQL))) return FALSE;

	if(!mysql_num_rows(mySQL_res)) return FALSE;

	mySQL_row=mysql_fetch_row(mySQL_res);

	strcpy(result,mySQL_row[0]);

	return TRUE;
}

int CSQL::CountRows(char* table, char* column, char* count_vol)
{
	char stradd[2048];
	sprintf(stradd,"SELECT * FROM `%s` WHERE `%s`='%s'", table, column, count_vol);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog("mySQL CountRows error: %s\n", mysql_error(mySQL));
		return 0;
	}

	if(!(mySQL_res=mysql_store_result(mySQL))) return 0;

	return mysql_num_rows(mySQL_res);
}

int CSQL::CountRows(char* table, char* column, int count_vol)
{
	char stradd[2048];
	sprintf(stradd,"SELECT * FROM `%s` WHERE `%s`='%d'", table, column, count_vol);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog("mySQL CountRows error: %s\n", mysql_error(mySQL));
		return 0;
	}

	if(!(mySQL_res=mysql_store_result(mySQL))) return 0;

	return mysql_num_rows(mySQL_res);
}