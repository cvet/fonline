#ifndef __CRAFT_MANAGER__
#define __CRAFT_MANAGER__

#include "Common.h"
#include "MsgFiles.h"

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
	uint Num;
	string Name;
	string Info;

	// Need parameters to show craft
	UIntVec ShowPNum;
	IntVec ShowPVal;
	UCharVec ShowPOr;

	// Need parameters to craft
	UIntVec NeedPNum;
	IntVec NeedPVal;
	UCharVec NeedPOr;

	// Need items to craft
	UShortVec NeedItems;
	UIntVec NeedItemsVal;
	UCharVec NeedItemsOr;

	// Need tools to craft
	UShortVec NeedTools;
	UIntVec NeedToolsVal;
	UCharVec NeedToolsOr;

	// New items
	UShortVec OutItems;
	UIntVec OutItemsVal;

	// Other
	string Script;
	uint ScriptBindId; //In runtime
	uint Experience;

	// Operator =
	CraftItem();
	CraftItem& operator=(const CraftItem& _right);
	bool IsValid();
	void Clear();

// Set, get parse
#ifdef FONLINE_CLIENT
	void SetName(FOMsg& msg_game, FOMsg& msg_item);
#endif
	
	int SetStr(uint num, const char* str);
	const char* GetStr(bool metadata);

#if defined(FONLINE_SERVER) || defined(FONLINE_MRFIXIT)
private:
	int SetStrParam(const char*& pstr_in, UIntVec& num_vec, IntVec& val_vec, UCharVec& or_vec);
	int SetStrItem(const char*& pstr_in, UShortVec& pid_vec, UIntVec& count_vec, UCharVec& or_vec);
	void GetStrParam(char* pstr_out, UIntVec& num_vec, IntVec& val_vec, UCharVec& or_vec);
	void GetStrItem(char* pstr_out, UShortVec& pid_vec, UIntVec& count_vec, UCharVec& or_vec);
#endif
};

typedef map<uint,CraftItem*,less<uint> > CraftItemMap;
typedef map<uint,CraftItem*,less<uint> >::iterator CraftItemMapIt;
typedef map<uint,CraftItem*,less<uint> >::value_type CraftItemMapVal;
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

	void EraseCraft(uint num);
#endif

	// Return fail crafts
	bool LoadCrafts(FOMsg& msg);

#ifdef FONLINE_CLIENT
	// Item manager must be init!
	void GenerateNames(FOMsg& msg_game, FOMsg& msg_item);
#endif

	void Finish();

	bool AddCraft(uint num, const char* str);
	bool AddCraft(CraftItem* craft, bool make_copy);
	CraftItem* GetCraft(uint num);
	bool IsCraftExist(uint num);

#ifdef FONLINE_SERVER
public:
	bool IsShowCraft(Critter* cr, uint num);
	void GetShowCrafts(Critter* cr, CraftItemVec& craft_vec);
	bool IsTrueCraft(Critter* cr, uint num);
	void GetTrueCrafts(Critter* cr, CraftItemVec& craft_vec);
private:
	bool IsTrueParams(Critter* cr, UIntVec& num_vec, IntVec& val_vec, UCharVec& or_vec);
	bool IsTrueItems(Critter* cr, UShortVec& pid_vec, UIntVec& count_vec, UCharVec& or_vec);
#endif
#ifdef FONLINE_CLIENT
public:
	bool IsShowCraft(CritterCl* cr, uint num);
	void GetShowCrafts(CritterCl* cr, CraftItemVec& craft_vec);
	bool IsTrueCraft(CritterCl* cr, uint num);
	void GetTrueCrafts(CritterCl* cr, CraftItemVec& craft_vec);
private:
	bool IsTrueParams(CritterCl* cr, UIntVec& num_vec, IntVec& val_vec, UCharVec& or_vec);
	bool IsTrueItems(CritterCl* cr, UShortVec& pid_vec, UIntVec& count_vec, UCharVec& or_vec);
#endif

#ifdef FONLINE_SERVER
public:
	int ProcessCraft(Critter* cr, uint num);
#endif
};

extern CraftManager MrFixit;

#endif // __CRAFT_MANAGER__