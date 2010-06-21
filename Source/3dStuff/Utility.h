#ifndef __3D_UTILITY__
#define __3D_UTILITY__

#include <string>
#include <sstream>
#include <assert.h>

// Template to convert standard types into strings
template <class T>
std::string ToString(const T & t)
{
	std::ostringstream oss;
	oss.clear();
	oss << t;
	return oss.str();
}

namespace Utility3d
{
	bool FailedHr(HRESULT hr);
	void DebugString(const string &str);
	char* DuplicateCharString(const char* c_str);
};

#endif // __3D_UTILITY__