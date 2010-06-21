#include "stdafx.h"
/********************************************************************
	created:	end of 2006; updated: begin of 2007
	author:		Anton Tsvetinskiy

	excluded from project 11.11.2009 22.47
*********************************************************************/

#include "SQL.h"
#include <Log.h>
CSQL Sql;

CSQL::CSQL():
isInit(false),
mySQL_res(NULL)
{
}

CSQL::~CSQL()
{
}

bool CSQL::Init()
{
	if(isInit==true) return true;
	WriteLog("MySQL initialisation...");

	mySQL=mysql_init(NULL);
	if(!mySQL)
	{
		WriteLog("Init error.\n");
		return false;
	}

	my_bool recon=1;
	if(mysql_options(mySQL,MYSQL_OPT_RECONNECT,&recon))
	{
		WriteLog("Reconnect option set fail<%s>.\n",mysql_error(mySQL));
		return false;
	}

	GetPrivateProfileString("SQL","user"		,"root"		,&user[0]		,64,".\\foserv.cfg");
	GetPrivateProfileString("SQL","passwd"		,""			,&passwd[0]		,64,".\\foserv.cfg");
	GetPrivateProfileString("SQL","host"		,"localhost",&host[0]		,64,".\\foserv.cfg");
	GetPrivateProfileString("SQL","db"			,"fonline"	,&db[0]			,64,".\\foserv.cfg");
	GetPrivateProfileString("SQL","unix_socket" ,""			,&unix_socket[0],64,".\\foserv.cfg");

	port=GetPrivateProfileInt("SQL","port",3306,".\\foserv.cfg");
	clientflag=GetPrivateProfileInt("SQL","clientflag",0,".\\foserv.cfg");

	if(!mysql_real_connect(mySQL,host,user,passwd,NULL,port,unix_socket,clientflag))
	{
		WriteLog("Fail to connect<%s>.\n",mysql_error(mySQL));
		return false;
	}

	isInit=true;
	WriteLog("Success.\n");
	return true;
}

/*bool CSQL::Connect()
{
	WriteLog("Connect to MySQL server...");
	if(!mysql_real_connect(mySQL,host,user,passwd,db,port,unix_socket,clientflag))
	{
		WriteLog("Fail<%s>.\n",mysql_error(mySQL));
		return false;
	}
	WriteLog("Success.\n");
	return true;
}*/

void CSQL::Finish()
{
	WriteLog("SQL manager finish...");
	if(mySQL_res) mysql_free_result(mySQL_res);
	mysql_close(mySQL);
	mySQL=NULL;
	mySQL_res=NULL;
	isInit=false;
	WriteLog("Success.\n");
}

bool CSQL::Ping()
{
	if(mysql_ping(mySQL))
	{
		WriteLog(__FUNCTION__" - Ping error<%s>.\n",mysql_error(mySQL));
		return false;
	}
	return true;
}

bool CSQL::Query(const char* query, ...)
{
	char str[4096];

	va_list list;
	va_start(list,query);
	vsprintf(str,query,list);
	va_end(list);

	if(mysql_query(mySQL,str))
	{
		WriteLog(__FUNCTION__" - Query<%s> error<%s>.\n",str,mysql_error(mySQL));
		return false;
	}
    return true;
}

bool CSQL::RQuery(const char* query, DWORD len)
{
	if(mysql_real_query(mySQL,query,len))
	{
		WriteLog(__FUNCTION__" - Query<%s> error<%s>.\n",query,mysql_error(mySQL));
		return false;
	}
    return true;
}

int	CSQL::EscapeString(char * out_str, const char * str, int size)
{
	if (size <= 0) size = strlen(str);
	return mysql_real_escape_string(mySQL, out_str, str, size);
}

SQL_ROW CSQL::FetchRow(void)
{
	mySQL_row=mysql_fetch_row(mySQL_res);
	return mySQL_row;
}

MYSQL_STMT* CSQL::GetStatementSave(const char* query, MYSQL_BIND* bind)
{
	if(!query)
	{
		WriteLog(__FUNCTION__" - query null ptr.\n");
		return NULL;
	}

	MYSQL_STMT* stmt=mysql_stmt_init(mySQL);
	if(!stmt)
	{
		WriteLog(__FUNCTION__" - mysql_stmt_init(), out of memory.\n");
		return NULL;
	}

	if(mysql_stmt_prepare(stmt,query,strlen(query)))
	{
		WriteLog(__FUNCTION__" - mysql_stmt_prepare(), error<%s>.\n",mysql_stmt_error(stmt));
		return NULL;
	}

	if(mysql_stmt_bind_param(stmt,bind))
	{
		WriteLog(__FUNCTION__" - mysql_stmt_bind_param(), error<%s>.\n",mysql_stmt_error(stmt));
		return NULL;
	}

	return stmt;
}

bool CSQL::RunStatementSave(MYSQL_STMT* stmt)
{
	if(mysql_stmt_execute(stmt))
	{
		WriteLog(__FUNCTION__" - mysql_stmt_execute(), error<%u,%s>.\n",mysql_stmt_errno(stmt),mysql_stmt_error(stmt));
		return false;
	}
	return true;
}

/*bool CSQL::RunStatementLoad(MYSQL_STMT* stmt)
{
	return mysql_stmt_fetch(stmt)==0;
}*/

void CSQL::CloseStatement(MYSQL_STMT* stmt)
{
	if(!stmt) return;
	if(mysql_stmt_close(stmt)) WriteLog(__FUNCTION__" - mysql_stmt_close(), error<%u,%s>.\n",mysql_stmt_errno(stmt),mysql_stmt_error(stmt));
}

int CSQL::GetInt(char* table, char* find_row, char* row, char* row_vol)
{
	char stradd[2048];
	sprintf(stradd,"SELECT `%s` FROM `%s` WHERE `%s`='%s'", find_row, table, row, row_vol);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog(__FUNCTION__" - Query error<%s>.\n", mysql_error(mySQL));
		return -1;
	}

	if(StoreResult()<=0) return -1;
	mySQL_row=mysql_fetch_row(mySQL_res);
	return atoi(mySQL_row[0]);
}

int CSQL::GetInt(char* table, char* find_row, char* row, int row_vol)
{
	char stradd[2048];
	sprintf(stradd,"SELECT `%s` FROM `%s` WHERE `%s`='%u'", find_row, table, row, row_vol);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog(__FUNCTION__" - Query error<%s>.\n", mysql_error(mySQL));
		return -1;
	}

	if(StoreResult()<=0) return -1;
	mySQL_row=mysql_fetch_row(mySQL_res);
	return atoi(mySQL_row[0]);
}

bool CSQL::GetChar(char* table, char* find_row, char* row, char* row_vol, char* result)
{
	char stradd[2048];
	sprintf(stradd,"SELECT `%s` FROM `%s` WHERE `%s`='%s'", find_row, table, row, row_vol);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog(__FUNCTION__" - Query error<%s>.\n", mysql_error(mySQL));
		return false;
	}

	if(StoreResult()<=0) return false;
	mySQL_row=mysql_fetch_row(mySQL_res);
	strcpy(result,mySQL_row[0]);
	return true;
}

bool CSQL::GetChar(char* table, char* find_row, char* row, int row_vol, char* result)
{
	char stradd[2048];
	sprintf(stradd,"SELECT `%s` FROM `%s` WHERE `%s`='%u'", find_row, table, row, row_vol);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog(__FUNCTION__" - Query error<%s>.\n", mysql_error(mySQL));
		return false;
	}

	if(StoreResult()<=0) return false;
	mySQL_row=mysql_fetch_row(mySQL_res);
	strcpy(result,mySQL_row[0]);
	return true;
}

/*bool CSQL::IsUserExist(const char* name)
{
	char stradd[2048];
	sprintf(stradd,"SELECT `id` FROM `users` WHERE `name`='%s'", name);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog(__FUNCTION__" - Query error<%s>.\n", mysql_error(mySQL));
		return true;
	}

	return StoreResult()!=0;
}

int CSQL::CheckUserPass(const char* name, const char* pass)
{
	char stradd[2048];
	sprintf(stradd,"SELECT `id` FROM `users` WHERE `name`='%s' AND `pass`='%s'", name, pass);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog(__FUNCTION__" - Query error<%s>.\n", mysql_error(mySQL));
		return 0;
	}

	if(StoreResult()<=0) return 0;
	mySQL_row=mysql_fetch_row(mySQL_res);
	return atoi(mySQL_row[0]);
}*/

int CSQL::CountRows(const char* query)
{
	if(mysql_query(mySQL,query))
	{
		WriteLog(__FUNCTION__" - Query error<%s>.\n",mysql_error(mySQL));
		return -1;
	}

	return StoreResult();
}

int CSQL::CountRows(char* table, char* column, char* count_vol)
{
	char stradd[2048];
	sprintf(stradd,"SELECT * FROM `%s` WHERE `%s`='%s'",table,column,count_vol);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog(__FUNCTION__" - Query error<%s>.\n",mysql_error(mySQL));
		return 0;
	}

	return StoreResult();
}

int CSQL::CountRows(char* table, char* column, int count_vol)
{
	char stradd[2048];
	sprintf(stradd,"SELECT * FROM `%s` WHERE `%s`='%u'",table,column,count_vol);

	if(mysql_query(mySQL,stradd))
	{
		WriteLog(__FUNCTION__" - Query error<%s>.\n",mysql_error(mySQL));
		return 0;
	}

	return StoreResult();
}

void CSQL::AddCheat(DWORD user_id, char* text_cheat)
{
	Query("INSERT INTO `users_cheat` (user_id,text_cheat) VALUES ('%u','%s');",user_id,text_cheat);
}

int CSQL::CountRow(char* table, char* row, char* value)
{
	if(!Query("SELECT * FROM `%s` WHERE `%s`='%s'",table,row,value)) return -1;
	return StoreResult();
}

int CSQL::CountTable(char* table, char* row)
{
	if(!Query("SELECT `%s` FROM `%s`", row, table)) return 0;
	return StoreResult();
}

void CSQL::PrintTableInLog(char* table, char* rows)
{
	WriteLog("Print table<%s>.\n", table);
	if(!Query("SELECT %s FROM %s",rows,table)) return;
	if(StoreResult()<0) return;

	WriteLog("Count<%u>.\n",mysql_num_rows(mySQL_res));
	while((mySQL_row=mysql_fetch_row(mySQL_res)))
	{
		for(int pt=0; pt<mysql_num_fields(mySQL_res); pt++)
			WriteLog("<%s> - ",mySQL_row[pt]); 
		WriteLog(" | \n ");
	}
}

void CSQL::BindPrepare(const char* query)
{
	bindCur=0;
	bindNeed=0;
	for(const char* str=query;*str;str++) if(*str=='?') bindNeed++;
	strcpy(bindQuery,query);
	memset(bindBind,0,sizeof(bindBind));
}

void CSQL::BindPush(enum_field_types type, void* data, bool is_unsigned)
{
	MYSQL_BIND& b=bindBind[bindCur];
	b.buffer_type=type;
	b.buffer=data;
	b.is_unsigned=(is_unsigned?1:0);
	bindCur++;
}

void CSQL::BindPush(enum_field_types type, void* data, DWORD length)
{
	MYSQL_BIND& b=bindBind[bindCur];
	b.buffer_type=type;
	b.buffer=data;
	b.buffer_length=length;
	bindCur++;
}

bool CSQL::BindExecute()
{
	if(bindCur!=bindNeed)
	{
		WriteLog(__FUNCTION__" - Invalid input parameters to bind, need<%d>, cur<%d>.\n",bindNeed,bindCur);
		return false;
	}

	MYSQL_STMT* stmt=Sql.GetStatementSave(bindQuery,bindBind);
	if(!stmt)
	{
		WriteLog(__FUNCTION__" - Unable to create sql statement.\n");
		return false;
	}

	if(!Sql.RunStatementSave(stmt))
	{
		WriteLog(__FUNCTION__" - Unable to execute sql statement.\n");
		Sql.CloseStatement(stmt);
		return false;
	}
	Sql.CloseStatement(stmt);
	return true;
}