#include "ScriptPragmas.h"
#include "AngelScript/angelscript.h"
#include <strstream>


#ifdef FONLINE_SCRIPT_COMPILER
	#include <Item.h>
	#include "..\ASCompiler\ASCompiler\ScriptEngine.h"

	namespace Script
	{
		asIScriptEngine* GetEngine()
		{
			return (asIScriptEngine*)GetScriptEngine();
		}

		HMODULE LoadDynamicLibrary(const char* dll_name)
		{
			// Load dynamic library
			HMODULE dll=LoadLibrary(dll_name);
			if(!dll) return NULL;

			// Register global function and vars
			static set<string> alreadyLoadedDll;
			if(!alreadyLoadedDll.count(dll_name))
			{
				// Register AS engine
				size_t* ptr=(size_t*)GetProcAddress(dll,"ASEngine");
				if(ptr) *ptr=(size_t)GetEngine();

				// Call init function
				typedef void(*DllMainEx)(bool);
				DllMainEx func=(DllMainEx)GetProcAddress(dll,"DllMainEx");
				if(func) (*func)(true);

				alreadyLoadedDll.insert(dll_name);
			}
			return dll;
		}
	}

	struct CScriptString
	{
		string Text;
		int RefCounter;
		CScriptString(const string& text):Text(text),RefCounter(0){}
	};

	#define WriteLog printf

#else

	#include "Script.h"

	#ifdef FONLINE_SERVER
		#include "Critter.h"
	#else
		#include "CritterCl.h"
	#endif
	#include "Item.h"

#endif


// #pragma globalvar "int __MyGlobalVar = 100"
class GlobalVarPragma
{
private:
	list<int> intArray;
	list<__int64> int64Array;
	list<CScriptString*> stringArray;
	list<float> floatArray;
	list<double> doubleArray;
	list<char> boolArray;

public:
	void Call(const string& text)
	{
		asIScriptEngine* engine=Script::GetEngine();
		string type,decl,value;
		char ch=0;
		istrstream str(text.c_str());
		str >> type >> decl >> ch >> value;

		if(decl=="")
		{
			WriteLog("Global var name not found, pragma<%s>.\n",text.c_str());
			return;
		}

		int int_value=(ch=='='?atoi(value.c_str()):0);
		double float_value=(ch=='='?atof(value.c_str()):0.0);
		string name=type+" "+decl;

		// Register
		if(type=="int8" || type=="int16" || type=="int32" || type=="int" || type=="uint8" || type=="uint16" || type=="uint32" || type=="uint")
		{
			list<int>::iterator it=intArray.insert(intArray.begin(),int_value);
			if(engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) WriteLog("Unable to register integer global var, pragma<%s>.\n",text.c_str());
		}
		else if(type=="int64" || type=="uint64")
		{
			list<__int64>::iterator it=int64Array.insert(int64Array.begin(),int_value);
			if(engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) WriteLog("Unable to register integer64 global var, pragma<%s>.\n",text.c_str());
		}
		else if(type=="string")
		{
			if(value!="") value=text.substr(text.find(value),string::npos);
			list<CScriptString*>::iterator it=stringArray.insert(stringArray.begin(),new CScriptString(value));
			if(engine->RegisterGlobalProperty(name.c_str(),(*it))<0) WriteLog("Unable to register string global var, pragma<%s>.\n",text.c_str());
		}
		else if(type=="float")
		{
			list<float>::iterator it=floatArray.insert(floatArray.begin(),(float)float_value);
			if(engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) WriteLog("Unable to register float global var, pragma<%s>.\n",text.c_str());
		}
		else if(type=="double")
		{
			list<double>::iterator it=doubleArray.insert(doubleArray.begin(),float_value);
			if(engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) WriteLog("Unable to register double global var, pragma<%s>.\n",text.c_str());
		}
		else if(type=="bool")
		{
			value=(ch=='='?value:"false");
			if(value!="true" && value!="false")
			{
				WriteLog("Invalid start value of boolean type, pragma<%s>.\n",text.c_str());
				return;
			}
			list<char>::iterator it=boolArray.insert(boolArray.begin(),value=="true"?true:false);
			if(engine->RegisterGlobalProperty(name.c_str(),&(*it))<0) WriteLog("Unable to register boolean global var, pragma<%s>.\n",text.c_str());
		}
		else
		{
			WriteLog("Global var not registered, unknown type, pragma<%s>.\n",text.c_str());
		}
	}
};

// #pragma crdata "Stat 0 199"
class CrDataPragma
{
private:
	int pragmaType;
	set<string> parametersAlready;
	DWORD parametersIndex;

public:
	CrDataPragma(int pragma_type):pragmaType(pragma_type),parametersIndex(1/*0 is ParamBase*/){}

	void Call(const string& text)
	{
		if(pragmaType==PRAGMA_SERVER)
			RegisterCrData(text.c_str());
		else if(pragmaType==PRAGMA_CLIENT)
			RegisterCrClData(text.c_str());
	}

	bool RegisterCrData(const char* text)
	{
		asIScriptEngine* engine=Script::GetEngine();
		string name;
		DWORD min,max;
		istrstream str(text);
		str >> name >> min >> max;
		if(str.fail()) return false;
		if(min>max || max>=MAX_PARAMS) return false;
		if(parametersAlready.count(name)) return true;
		if(parametersIndex>=MAX_PARAMETERS_ARRAYS) return false;

		char decl_val[128];
		sprintf_s(decl_val,"DataVal %s",name.c_str());
		char decl_ref[128];
		sprintf_s(decl_ref,"DataRef %sBase",name.c_str());

#ifdef FONLINE_SERVER
		// Real registration
		if(engine->RegisterObjectProperty("Critter",decl_val,offsetof(Critter,ThisPtr[parametersIndex]))<0) return false;
		if(engine->RegisterObjectProperty("Critter",decl_ref,offsetof(Critter,ThisPtr[parametersIndex]))<0) return false;
		Critter::ParametersMin[parametersIndex]=min;
		Critter::ParametersMax[parametersIndex]=max;
		Critter::ParametersOffset[parametersIndex]=(strstr(text,"+")!=NULL);
#else
		// Fake registration
		if(engine->RegisterObjectProperty("Critter",decl_val,10000+parametersIndex*4)<0) return false;
		if(engine->RegisterObjectProperty("Critter",decl_ref,10000+parametersIndex*4)<0) return false;
#endif

		parametersIndex++;
		parametersAlready.insert(name);
		return true;
	}

	bool RegisterCrClData(const char* text)
	{
		asIScriptEngine* engine=Script::GetEngine();
		string name;
		DWORD min,max;
		istrstream str(text);
		str >> name >> min >> max;
		if(str.fail()) return false;
		if(min>max || max>=MAX_PARAMS) return false;
		if(parametersAlready.count(name)) return true;
		if(parametersIndex>=MAX_PARAMETERS_ARRAYS) return false;

		char decl_val[128];
		sprintf_s(decl_val,"DataVal %s",name.c_str());
		char decl_ref[128];
		sprintf_s(decl_ref,"DataRef %sBase",name.c_str());

#ifdef FONLINE_CLIENT
		// Real registration
		if(engine->RegisterObjectProperty("CritterCl",decl_val,offsetof(CritterCl,ThisPtr[parametersIndex]))<0) return false;
		if(engine->RegisterObjectProperty("CritterCl",decl_ref,offsetof(CritterCl,ThisPtr[parametersIndex]))<0) return false;
		CritterCl::ParametersMin[parametersIndex]=min;
		CritterCl::ParametersMax[parametersIndex]=max;
		CritterCl::ParametersOffset[parametersIndex]=(strstr(text,"+")!=NULL);
#else
		// Fake registration
		if(engine->RegisterObjectProperty("CritterCl",decl_val,10000+parametersIndex*4)<0) return false;
		if(engine->RegisterObjectProperty("CritterCl",decl_ref,10000+parametersIndex*4)<0) return false;
#endif

		parametersIndex++;
		parametersAlready.insert(name);
		return true;
	}
};

// #pragma bindfunc "int MyObject::MyMethod(int, uint) -> my.dll MyDllFunc"
class BindFuncPragma
{
public:
	void Call(const string& text)
	{
		asIScriptEngine* engine=Script::GetEngine();
		string func_name,dll_name,func_dll_name;
		istrstream str(text.c_str());
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
			WriteLog("Error in 'bindfunc' pragma<%s>, parse fail.\n",text.c_str());
			return;
		}

		HMODULE dll=Script::LoadDynamicLibrary(dll_name.c_str());
		if(!dll)
		{
			WriteLog("Error in 'bindfunc' pragma<%s>, dll not found, error<%u>.\n",text.c_str(),GetLastError());
			return;
		}

		// Find function
		FARPROC func=GetProcAddress(dll,func_dll_name.c_str());
		if(!func)
		{
			WriteLog("Error in 'bindfunc' pragma<%s>, function not found, error<%u>.\n",text.c_str(),GetLastError());
			return;
		}

		int result=0;
		if(func_name.find("::")==string::npos)
		{
			// Register global function
			result=engine->RegisterGlobalFunction(func_name.c_str(),asFUNCTION(func),asCALL_CDECL);
		}
		else
		{
			// Register class method
			string::size_type i=func_name.find(" ");
			string::size_type j=func_name.find("::");
			if(i==string::npos || i+1>=j)
			{
				WriteLog("Error in 'bindfunc' pragma<%s>, parse class name fail.\n",text.c_str());
				return;
			}
			i++;
			string class_name;
			class_name.assign(func_name,i,j-i);
			func_name.erase(i,j-i+2);
			result=engine->RegisterObjectMethod(class_name.c_str(),func_name.c_str(),asFUNCTION(func),asCALL_CDECL_OBJFIRST);
		}
		if(result<0) WriteLog("Error in 'bindfunc' pragma<%s>, script registration fail, error<%d>.\n",text.c_str(),result);
	}
};

// #pragma bindfield "[const] int MyObject::Field -> offset"
class BindFieldPragma
{
private:
	string className;
	BoolVec busyBytes;
	int baseOffset;
	int dataSize;

public:
	BindFieldPragma(const string& class_name, int base_offset, int data_size):
		className(class_name),baseOffset(base_offset),dataSize(data_size){}

	void Call(const string& text)
	{
		asIScriptEngine* engine=Script::GetEngine();
		string s,type_name,field_name,class_name;
		int offset=0;
		istrstream str(text.c_str());

		str >> s;
		type_name+=s+" ";
		if(s=="const")
		{
			str >> s;
			type_name+=s+" ";
		}

		str >> s;
		size_t ns=s.find("::");
		if(ns==string::npos)
		{
			WriteLog("Error in 'bindfield' pragma<%s>, '::' not found.\n",text.c_str());
			return;
		}
		field_name.assign(s,ns+2,s.size()-ns-2);
		class_name.assign(s,0,ns);

		str >> s >> offset;
		if(s!="->" || str.fail())
		{
			WriteLog("Error in 'bindfield' pragma<%s>, offset parse fail.\n",text.c_str());
			return;
		}

		if(class_name!=className.c_str())
		{
			WriteLog("Error in 'bindfield' pragma<%s>, unknown class name<%s>.\n",text.c_str(),class_name.c_str());
			return;
		}

		int size=engine->GetSizeOfPrimitiveType(engine->GetTypeIdByDecl(type_name.c_str()));
		if(size<=0)
		{
			WriteLog("Error in 'bindfield' pragma<%s>, wrong type<%s>.\n",text.c_str(),type_name.c_str());
			return;
		}

		// Check offset
		int base_offset=baseOffset;
		int data_size=dataSize;
		if(offset<0 || offset+size>=data_size)
		{
			WriteLog("Error in 'bindfield' pragma<%s>, wrong offset<%d> data.\n",text.c_str(),offset);
			return;
		}

		// Check for already binded on this position
		vector<bool>& busy_bytes=busyBytes;
		if((int)busy_bytes.size()<offset+size) busy_bytes.resize(offset+size);
		bool busy=false;
		for(int i=offset;i<offset+size;i++)
		{
			if(busy_bytes[i])
			{
				busy=true;
				break;
			}
		}
		if(busy)
		{
			WriteLog("Error in 'bindfield' pragma<%s>, data bytes<%d..%d> already in use.\n",text.c_str(),offset,offset+size-1);
			return;
		}

		// Check name for already used
		for(int i=0,ii=engine->GetObjectTypeCount();i<ii;i++)
		{
			asIObjectType* ot=engine->GetObjectTypeByIndex(i);
			if(!strcmp(ot->GetName(),className.c_str()))
			{
				for(int j=0,jj=ot->GetPropertyCount();j<jj;j++)
				{
					const char* name;
					ot->GetProperty(j,&name);
					if(!strcmp(name,field_name.c_str()))
					{
						WriteLog("Error in 'bindfield' pragma<%s>, property<%s> already available.\n",text.c_str(),name);
						return;
					}
				}
			}
		}

		// Bind
		int result=engine->RegisterObjectProperty(class_name.c_str(),(type_name+field_name).c_str(),base_offset+offset);
		if(result<0)
		{
			WriteLog("Error in 'bindfield' pragma<%s>, register object property fail, error<%d>.\n",text.c_str(),result);
			return;
		}
		for(int i=offset;i<offset+size;i++) busy_bytes[i]=true;
	}
};


ScriptPragmaCallback::ScriptPragmaCallback(int pragma_type)
{
	pragmaType=pragma_type;
	if(pragmaType!=PRAGMA_SERVER && pragmaType!=PRAGMA_CLIENT && pragmaType!=PRAGMA_MAPPER) pragmaType=PRAGMA_UNKNOWN;

	if(pragmaType!=PRAGMA_UNKNOWN)
	{
		globalVarPragma=new GlobalVarPragma();
		crDataPragma=new CrDataPragma(pragmaType);
		bindFuncPragma=new BindFuncPragma();
		bindFieldPragma=new BindFieldPragma("ProtoItem",offsetof(ProtoItem,UserData),PROTO_ITEM_USER_DATA_SIZE);
	}
}

void ScriptPragmaCallback::CallPragma(const string& name, const Preprocessor::PragmaInstance& instance)
{
	if(alreadyProcessed.count(name+instance.text)) return;
	alreadyProcessed.insert(name+instance.text);

	if(name=="globalvar" && globalVarPragma) globalVarPragma->Call(instance.text);
	else if(name=="crdata" && crDataPragma) crDataPragma->Call(instance.text);
	else if(name=="bindfunc" && bindFuncPragma) bindFuncPragma->Call(instance.text);
	else if(name=="bindfield" && bindFieldPragma) bindFieldPragma->Call(instance.text);
	else WriteLog("Unknown pragma instance, name<%s> text<%s>.\n",name.c_str(),instance.text.c_str());
}