#ifndef SCRIPTDATETIME_H
#define SCRIPTDATETIME_H

#ifndef ANGELSCRIPT_H 
// Avoid having to inform include path if header is already include before
#include <angelscript.h>
#endif

#ifdef AS_CAN_USE_CPP11
#include <chrono>
#else
#error Sorry, this requires C++11 which your compiler doesnt appear to support
#endif

BEGIN_AS_NAMESPACE

class CDateTime
{
public:
	// Constructors
	CDateTime();
	CDateTime(const CDateTime &other);

	// Copy the stored value from another any object
	CDateTime &operator=(const CDateTime &other);

	// Accessors
	int getYear() const;
	int getMonth() const;
	int getDay() const;
	int getHour() const;
	int getMinute() const;
	int getSecond() const;

protected:
	std::chrono::system_clock::time_point tp;
};

void RegisterScriptDateTime(asIScriptEngine *engine);

END_AS_NAMESPACE

#endif
