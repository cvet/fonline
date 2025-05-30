#include "datetime.h"
#include <string.h>
#include <assert.h>
#include <new>
#include "../autowrapper/aswrappedcall.h"

using namespace std;
using namespace std::chrono;

BEGIN_AS_NAMESPACE

// TODO: Allow setting the timezone to use

CDateTime::CDateTime() : tp(std::chrono::system_clock::now()) 
{
}

CDateTime::CDateTime(const CDateTime &o) : tp(o.tp) 
{
}

CDateTime &CDateTime::operator=(const CDateTime &o)
{
	tp = o.tp;
	return *this;
}

int CDateTime::getYear() const
{
	time_t t = system_clock::to_time_t(tp);
	tm local = *localtime(&t);

	return local.tm_year + 1900;
}

int CDateTime::getMonth() const
{
	time_t t = system_clock::to_time_t(tp);
	tm local = *localtime(&t);

	return local.tm_mon + 1;
}

int CDateTime::getDay() const
{
	time_t t = system_clock::to_time_t(tp);
	tm local = *localtime(&t);

	return local.tm_mday;
}

int CDateTime::getHour() const
{
	time_t t = system_clock::to_time_t(tp);
	tm local = *localtime(&t);

	return local.tm_hour;
}

int CDateTime::getMinute() const
{
	time_t t = system_clock::to_time_t(tp);
	tm local = *localtime(&t);

	return local.tm_min;
}

int CDateTime::getSecond() const
{
	time_t t = system_clock::to_time_t(tp);
	tm local = *localtime(&t);

	return local.tm_sec;
}

static void Construct(CDateTime *mem)
{
	new(mem) CDateTime();
}

static void ConstructCopy(CDateTime *mem, const CDateTime &o)
{
	new(mem) CDateTime(o);
}

void RegisterScriptDateTime(asIScriptEngine *engine)
{
	// TODO: Add support for generic calling convention
	int r = engine->RegisterObjectType("datetime", sizeof(CDateTime), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<CDateTime>()); assert(r >= 0);

	// (FOnline Patch)
	if(strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") == 0)
	{
		r = engine->RegisterObjectBehaviour("datetime", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Construct), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("datetime", asBEHAVE_CONSTRUCT, "void f(const datetime &in)", asFUNCTION(ConstructCopy), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "datetime &opAssign(const datetime &in)", asMETHOD(CDateTime, operator=), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "int get_year() const", asMETHOD(CDateTime, getYear), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "int get_month() const", asMETHOD(CDateTime, getMonth), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "int get_day() const", asMETHOD(CDateTime, getDay), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "int get_hour() const", asMETHOD(CDateTime, getHour), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "int get_minute() const", asMETHOD(CDateTime, getMinute), asCALL_THISCALL); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "int get_second() const", asMETHOD(CDateTime, getSecond), asCALL_THISCALL); assert(r >= 0);
	}
	else
	{
		r = engine->RegisterObjectBehaviour("datetime", asBEHAVE_CONSTRUCT, "void f()", WRAP_OBJ_LAST(Construct), asCALL_GENERIC); assert(r >= 0);
		r = engine->RegisterObjectBehaviour("datetime", asBEHAVE_CONSTRUCT, "void f(const datetime &in)", WRAP_OBJ_LAST(ConstructCopy), asCALL_GENERIC); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "datetime &opAssign(const datetime &in)", WRAP_MFN(CDateTime, operator=), asCALL_GENERIC); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "int get_year() const", WRAP_MFN(CDateTime, getYear), asCALL_GENERIC); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "int get_month() const", WRAP_MFN(CDateTime, getMonth), asCALL_GENERIC); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "int get_day() const", WRAP_MFN(CDateTime, getDay), asCALL_GENERIC); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "int get_hour() const", WRAP_MFN(CDateTime, getHour), asCALL_GENERIC); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "int get_minute() const", WRAP_MFN(CDateTime, getMinute), asCALL_GENERIC); assert(r >= 0);
		r = engine->RegisterObjectMethod("datetime", "int get_second() const", WRAP_MFN(CDateTime, getSecond), asCALL_GENERIC); assert(r >= 0);
	}
}

END_AS_NAMESPACE