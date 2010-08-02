#ifndef __CRAFT_MANAGER__
#define __CRAFT_MANAGER__

#include "Defines.h"
#include "Text.h"

#define MRFIXIT_METADATA	'!'
#define MRFIXIT_NEXT		'@'
#define MRFIXIT_SPACE		' '
#define MRFIXIT_AND			'&'
#define MRFIXIT_OR			'|'

#define MRFIXIT_METADATA_S	"!"
#define MRFIXIT_NEXT_S		"@"
#define MRFIXIT_SPACE_S		" "
#define MRFIXIT_AND_S		"&"
#define MRFIXIT_OR_S		"|"

#define FIXBOY_TIME_OUT     (GameOpt.TimeMultiplier*60) // 1 minute

struct CraftItem
{
public:
	// Number, name, info
	DWORD Num;
	string Name;
	string Info;

	// Need parameters to show craft
	DwordVec ShowPNum;
	IntVec ShowPVal;
	ByteVec ShowPOr;

	// Need parameters to craft
	DwordVec NeedPNum;
	IntVec NeedPVal;
	ByteVec NeedPOr;

	// Need items to craft
	WordVec NeedItems;
	DwordVec NeedItemsVal;
	ByteVec NeedItemsOr;

	// Need tools to craft
	WordVec NeedTools;
	DwordVec NeedToolsVal;
	ByteVec NeedToolsOr;

	// New items
	WordVec OutItems;
	DwordVec OutItemsVal;

	// Other
	string Script;
	DWORD ScriptBindId; //In runtime
	DWORD Experience;

	// Operator =
	CraftItem();
	CraftItem& operator=(const CraftItem& _right);
	bool IsValid();
	void Clear();

// Set, get parse
#ifdef FONLINE_CLIENT
	void SetName(FOMsg& msg_game, FOMsg& msg_item);
#endif
	
	int SetStr(DWORD num, const char* str);
	const char* GetStr(bool metadata);

#ifdef FONLINE_SERVER
private:
	int SetStrParam(const char*& pstr_in, DwordVec& num_vec, IntVec& val_vec, ByteVec& or_vec);
	int SetStrItem(const char*& pstr_in, WordVec& pid_vec, DwordVec& count_vec, ByteVec& or_vec);
	void GetStrParam(char* pstr_out, DwordVec& num_vec, IntVec& val_vec, ByteVec& or_vec);
	void GetStrItem(char* pstr_out, WordVec& pid_vec, DwordVec& count_vec, ByteVec& or_vec);
#endif
};

typedef map<DWORD,CraftItem*,less<DWORD> > CraftItemMap;
typedef map<DWORD,CraftItem*,less<DWORD> >::iterator CraftItemMapIt;
typedef map<DWORD,CraftItem*,less<DWORD> >::value_type CraftItemMapVal;
typedef vector<CraftItem*> CraftItemVec;

#ifdef FONLINE_SERVER
class Critter;
#endif
#ifdef FONLINE_CLIENT
class CritterCl;
#endif

class CraftManager
{
private:
	CraftItemMap itemCraft;

public:
	bool operator==(const CraftManager& r);

#ifdef FONLINE_MRFIXIT
	// Return fail crafts
	bool LoadCrafts(const char* path);
	bool SaveCrafts(const char* path);

	CraftItemMap& GetAllCrafts(){return itemCraft;}

	void EraseCraft(DWORD num);
#endif

	// Return fail crafts
	bool LoadCrafts(FOMsg& msg);

#ifdef FONLINE_CLIENT
	// Item manager must be init!
	void GenerateNames(FOMsg& msg_game, FOMsg& msg_item);
#endif

	void Finish();

	bool AddCraft(DWORD num, const char* str);
	bool AddCraft(CraftItem* craft, bool make_copy);
	CraftItem* GetCraft(DWORD num);
	bool IsCraftExist(DWORD num);

#ifdef FONLINE_SERVER
public:
	bool IsShowCraft(Critter* cr, DWORD num);
	void GetShowCrafts(Critter* cr, CraftItemVec& craft_vec);
	bool IsTrueCraft(Critter* cr, DWORD num);
	void GetTrueCrafts(Critter* cr, CraftItemVec& craft_vec);
private:
	bool IsTrueParams(Critter* cr, DwordVec& num_vec, IntVec& val_vec, ByteVec& or_vec);
	bool IsTrueItems(Critter* cr, WordVec& pid_vec, DwordVec& count_vec, ByteVec& or_vec);
#endif
#ifdef FONLINE_CLIENT
public:
	bool IsShowCraft(CritterCl* cr, DWORD num);
	void GetShowCrafts(CritterCl* cr, CraftItemVec& craft_vec);
	bool IsTrueCraft(CritterCl* cr, DWORD num);
	void GetTrueCrafts(CritterCl* cr, CraftItemVec& craft_vec);
private:
	bool IsTrueParams(CritterCl* cr, DwordVec& num_vec, IntVec& val_vec, ByteVec& or_vec);
	bool IsTrueItems(CritterCl* cr, WordVec& pid_vec, DwordVec& count_vec, ByteVec& or_vec);
#endif

#ifdef FONLINE_SERVER
public:
	int ProcessCraft(Critter* cr, DWORD num);
#endif
};

extern CraftManager MrFixit;

#endif // __CRAFT_MANAGER__