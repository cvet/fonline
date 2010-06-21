#ifndef __NAMES__
#define __NAMES__

#define FONAME_PARAM    (0)
#define FONAME_ITEM     (1)
#define FONAME_DEFINE   (2)
#define FONAME_PICTURE  (3)
#define FONAME_MAX      (4)

namespace FONames
{
	void GenerateFoNames(int path_type);
	StrVec GetNames(int names);
	int GetParamId(const char* str);
	const char* GetParamName(DWORD index);
	int GetItemPid(const char* str);
	const char* GetItemName(WORD pid);
	int GetDefineValue(const char* str);
	const char* GetPictureName(DWORD index);
};

#endif // __NAMES__