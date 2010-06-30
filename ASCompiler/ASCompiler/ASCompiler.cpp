// ASCompiler.cpp : Defines the entry point for the console application.
//

#include "StdAfx.h"
#include <Defines.h>
#include <list>
#include <set>
#include <strstream>
using namespace std;

typedef int (__cdecl *BindFunc)(asIScriptEngine*);
typedef asIScriptEngine* (__cdecl *ASCreate)();
typedef bool(*PragmaCallbackFunc)(const char*);

asIScriptEngine* Engine;

void CallBack(const asSMessageInfo *msg, void *param)
{
	const char* type="ERR ";
	if(msg->type==asMSGTYPE_WARNING) type="WARN";
	else if(msg->type==asMSGTYPE_INFORMATION) type="INFO";

	printf("%s (%d, %d) : %s : %s.\n",msg->section,msg->row,msg->col,type,msg->message);
}

class GvarPragmaCallback : public Preprocessor::PragmaCallback
{
private:
	static set<string> addedVars;
	static list<int> intArray;
	static list<__int64> int64Array;
	static list<string> stringArray;
	static list<float> floatArray;
	static list<double> doubleArray;
	static list<char> boolArray;

public:
	void pragma(const Preprocessor::PragmaInstance& instance)
	{
		string type,decl,value;
		char ch=0;
		istrstream str(instance.text.c_str());
		str >> type;
		str >> decl;
		str >> ch;
		str >> value;

		if(decl=="")
		{
			printf("Global var name not found, pragma<%s>.\n",instance.text.c_str());
			return;
		}

		int int_value=(ch=='='?atoi(value.c_str()):0);
		double float_value=(ch=='='?atof(value.c_str()):0.0);
		string name=type+" "+decl;

		// Check for exists
		if(addedVars.count(name)) return;

		// Register
		if(type=="int8" || type=="int16" || type=="int32" || type=="int" || type=="uint8" || type=="uint16" || type=="uint32" || type=="uint")
		{
			list<int>::iterator it=intArray.insert(intArray.begin(),int_value);
			if(Engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) printf("Unable to register integer global var, pragma<%s>.\n",instance.text.c_str());
		}
		else if(type=="int64" || type=="uint64")
		{
			list<__int64>::iterator it=int64Array.insert(int64Array.begin(),int_value);
			if(Engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) printf("Unable to register integer64 global var, pragma<%s>.\n",instance.text.c_str());
		}
		else if(type=="string")
		{
			value=instance.text.substr(instance.text.find(value),string::npos);
			list<string>::iterator it=stringArray.insert(stringArray.begin(),value);
			if(Engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) printf("Unable to register string global var, pragma<%s>.\n",instance.text.c_str());
		}
		else if(type=="float")
		{
			list<float>::iterator it=floatArray.insert(floatArray.begin(),(float)float_value);
			if(Engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) printf("Unable to register float global var, pragma<%s>.\n",instance.text.c_str());
		}
		else if(type=="double")
		{
			list<double>::iterator it=doubleArray.insert(doubleArray.begin(),float_value);
			if(Engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) printf("Unable to register double global var, pragma<%s>.\n",instance.text.c_str());
		}
		else if(type=="bool")
		{
			value=(ch=='='?value:"false");
			if(value!="true" && value!="false")
			{
				printf("Invalid start value of boolean type, pragma<%s>.\n",instance.text.c_str());
				return;
			}
			list<char>::iterator it=boolArray.insert(boolArray.begin(),value=="true"?true:false);
			if(Engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) printf("Unable to register boolean global var, pragma<%s>.\n",instance.text.c_str());
		}
		else
		{
			printf("Global var not registered, unknown type, pragma<%s>.\n",instance.text.c_str());
		}
		addedVars.insert(name);
	}
};
set<string> GvarPragmaCallback::addedVars;
list<int> GvarPragmaCallback::intArray;
list<__int64> GvarPragmaCallback::int64Array;
list<string> GvarPragmaCallback::stringArray;
list<float> GvarPragmaCallback::floatArray;
list<double> GvarPragmaCallback::doubleArray;
list<char> GvarPragmaCallback::boolArray;

class CrDataPragmaCallback : public Preprocessor::PragmaCallback
{
public:
	static PragmaCallbackFunc CallFunc;

	void pragma(const Preprocessor::PragmaInstance& instance)
	{
		if(!(*CallFunc)(instance.text.c_str())) printf("Unable parse critter data pragma<%s>.\n",instance.text.c_str());
	}
};
bool PragmaCallbackCrData(const char* text);
PragmaCallbackFunc CrDataPragmaCallback::CallFunc=PragmaCallbackCrData;

StringSet ParametersAlready;
DWORD ParametersIndex=1;
bool PragmaCallbackCrData(const char* text)
{
	string name;
	DWORD min,max;
	istrstream str(text);
	str >> name;
	str >> min;
	str >> max;
	if(str.fail()) return false;
	if(min>max || max>=MAX_PARAMS) return false;
	if(ParametersAlready.count(name)) return true;
	if(ParametersIndex>=MAX_PARAMETERS_ARRAYS) return false;

	bool is_client=Preprocessor::IsDefined("__CLIENT");
	asIScriptEngine* engine=Engine;
	char decl[128];
	sprintf_s(decl,"DataVal %s",name.c_str());
	if(engine->RegisterObjectProperty(is_client?"CritterCl":"Critter",decl,0)<0) return false;
	sprintf_s(decl,"DataRef %sBase",name.c_str());
	if(engine->RegisterObjectProperty(is_client?"CritterCl":"Critter",decl,0)<0) return false;
	ParametersIndex++;
	ParametersAlready.insert(name);
	return true;
}

HMODULE LoadDynamicLibrary(const char* dll_name)
{
	// Load dynamic library
	HMODULE dll=LoadLibrary(dll_name);
	if(!dll) return NULL;

	// Register global function and vars
	static StringSet alreadyLoadedDll;
	if(!alreadyLoadedDll.count(dll_name))
	{
		// Register AS engine
		size_t* ptr=(size_t*)GetProcAddress(dll,"ASEngine");
		if(ptr) *ptr=(size_t)Engine;

		// Call init function
		typedef void(*DllMainEx)(bool);
		DllMainEx func=(DllMainEx)GetProcAddress(dll,"DllMainEx");
		if(func) (*func)(true);

		alreadyLoadedDll.insert(dll_name);
	}
	return dll;
}

class BindFuncPragmaCallback : public Preprocessor::PragmaCallback
{
private:
	static set<string> alreadyProcessed;

public:
	void pragma(const Preprocessor::PragmaInstance& instance)
	{
		if(alreadyProcessed.count(instance.text)) return;
		alreadyProcessed.insert(instance.text);

		string func_name,dll_name,func_dll_name;
		istrstream str(instance.text.c_str());
		while(true)
		{
			string s;
			str >> s;
			if(str.fail() || s=="->") break;
			if(func_name!="") func_name+=" ";
			func_name+=s;
		}
		str >> dll_name;
		str >> func_dll_name;

		if(str.fail())
		{
			printf("Error in bindfunc pragma<%s>, parse fail.\n",instance.text.c_str());
			return;
		}

		HMODULE dll=LoadDynamicLibrary(dll_name.c_str());
		if(!dll)
		{
			printf("Error in bindfunc pragma<%s>, dll not found, error<%u>.\n",instance.text.c_str(),GetLastError());
			return;
		}

		FARPROC func=GetProcAddress(dll,func_dll_name.c_str());
		if(!func)
		{
			printf("Error in bindfunc pragma<%s>, function not found, error<%u>.\n",instance.text.c_str(),GetLastError());
			return;
		}

		int result=0;
		if(func_name.find("::")==string::npos)
		{
			// Register global function
			result=Engine->RegisterGlobalFunction(func_name.c_str(),asFUNCTION(func),asCALL_CDECL);
		}
		else
		{
			// Register class method
			string::size_type i=func_name.find(" ");
			string::size_type j=func_name.find("::");
			if(i==string::npos || i+1>=j)
			{
				printf("Error in bindfunc pragma<%s>, parse class name fail.\n",instance.text.c_str());
				return;
			}
			i++;
			string class_name;
			class_name.assign(func_name,i,j-i);
			func_name.erase(i,j-i+2);
			result=Engine->RegisterObjectMethod(class_name.c_str(),func_name.c_str(),asFUNCTION(func),asCALL_CDECL_OBJFIRST);
		}
		if(result<0) printf("Error in bindfunc pragma<%s>, script registration fail, error<%d>.\n",instance.text.c_str(),result);
	}
};
set<string> BindFuncPragmaCallback::alreadyProcessed;

int _tmain(int argc, _TCHAR* argv[])
{
	/************************************************************************/
	/* Parameters                                                           */
	/************************************************************************/
	setlocale(LC_ALL,"Russian");

	if(argc<4)
	{
		printf("Not enough parameters. Example:\nASCompiler script_name.fos as.dll fo.dll [preprocessor_output.txt]\n");
		return 0;
	}

	char* str_fname=argv[1];
	char* str_comp_dll=argv[2];
	char* str_script_dll=argv[3];
	char* str_prep=NULL;
	vector<char*> defines;

	for(int i=4;i<argc;i++)
	{
		if(strstr(argv[i],"-p") && i+1<argc) str_prep=argv[i+1];
		else if(strstr(argv[i],"-d") && i+1<argc) defines.push_back(argv[i+1]);
	}
	/************************************************************************/
	/* Dll                                                                  */
	/************************************************************************/
	// Engine
	HINSTANCE comp_dll;
	if((comp_dll=LoadLibrary(str_comp_dll))==NULL)
	{
		printf("AngelScript library<%s> load fail.\n",str_comp_dll);
		return 0;
	}

	ASCreate create_func;
	if(!(create_func=(ASCreate)GetProcAddress(comp_dll,"Register")))
	{
		printf("Function Register not found, error<%u>.\n",GetLastError());
		return 0;
	}

	Engine=(*create_func)();
	if(!Engine)
	{
		printf("Register fail.\n");
		return 0;
	}
	Engine->SetMessageCallback(asFUNCTION(CallBack),NULL,asCALL_CDECL);

	// Bind
	HINSTANCE script_dll;
	if((script_dll=LoadLibrary(str_script_dll))==NULL)
	{
		printf("Script library<%s> load fail.\n",str_script_dll);
		return 0;
	}

	BindFunc bind_func;
	if(!(bind_func=(BindFunc)GetProcAddress(script_dll,"Bind")))
	{
		printf("Function Bind not found, error<%u>.\n",GetLastError());
		return 0;
	}
	int result=(*bind_func)(Engine);
	if(result) printf("Warning, Bind result: %d.\n",result);
	FreeLibrary(script_dll);

	/************************************************************************/
	/* Compile                                                              */
	/************************************************************************/

	printf("Compiling...\n");

	// Preprocessor
	Preprocessor::VectorOutStream vos;
	Preprocessor::VectorOutStream vos_err;
	Preprocessor::FileSource fsrc;
	fsrc.CurrentDir="";
	fsrc.Stream=NULL;

	Preprocessor::RegisterPragma("globalvar",new GvarPragmaCallback());
	Preprocessor::RegisterPragma("crdata",new CrDataPragmaCallback());
	Preprocessor::RegisterPragma("bindfunc",new BindFuncPragmaCallback());
	for(size_t i=0;i<defines.size();i++) Preprocessor::Define(string(defines[i]));
	int res=Preprocessor::Preprocess(str_fname,fsrc,vos,true,vos_err);
	if(res)
	{
		vos_err.PushNull();
		printf("Unable to preprocess. Errors:\n%s\n",vos_err.GetData());
		return 0;
	}

	vos.Format();
	if(str_prep)
	{
		char* buf=new char[vos.GetSize()+1];
		memcpy(buf,vos.GetData(),vos.GetSize());
		buf[vos.GetSize()]='\0';

		FILE* f=NULL;
		if(!fopen_s(&f,str_prep,"wt"))
		{
			fwrite(buf,sizeof(char),strlen(buf),f);
			fclose(f);
		}
		else
		{
			printf("Unable to create preprocessed file<%s>.\n",str_prep);
		}
		delete buf;
	}

	// Compiler
	__int64 freq,fp,fp2;
	QueryPerformanceFrequency((PLARGE_INTEGER) &freq);
	QueryPerformanceCounter((PLARGE_INTEGER)&fp);

	asIScriptModule* mod=Engine->GetModule(0,asGM_ALWAYS_CREATE);
	if(!mod)
	{
		printf("Can't create module.\n");
		return 0;
	}

	if(mod->AddScriptSection(NULL,vos.GetData(),vos.GetSize(),0)<0)
	{
		printf("Unable to add section.\n");
		return 0;
	}

	if(mod->Build()<0)
	{
		printf("Unable to build.\nRow and col see in preprocessed file.\n");
		return 0;
	}

	QueryPerformanceCounter((PLARGE_INTEGER)&fp2);
	printf("Success.\nTime: %.02f ms.\n",double(((double)fp2-(double)fp)/(double)freq*1000)/*,t2-t1*/);
	Engine->Release();
	FreeLibrary(comp_dll);
	/************************************************************************/
	/*                                                                      */
	/************************************************************************/
	return 0;
}

