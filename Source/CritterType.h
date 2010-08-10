#ifndef __CRITTER_TYPE__
#define __CRITTER_TYPE__

#include "Defines.h"

#define CRTYPE_FILE_NAME          "CritterTypes.cfg"
#define DEFAULT_CRTYPE            (0)
#define CRIT_VAULT_MALE           (69)
#define CRIT_VAULT_FEMALE         (74)
#define CRIT_DWELLER_MALE         (10)
#define CRIT_DWELLER_FEMALE       (4)
#define CRIT_RAT                  (24)

class FOMsg;

struct CritTypeType
{
	bool Enabled;
	char Name[64];
	char SoundName[64];
	DWORD Alias;
	DWORD Multihex;

	bool Is3d;
	bool CanWalk;
	bool CanRun;
	bool CanAim;
	bool CanArmor;
	bool CanRotate;

	bool Anim1[21];
};

namespace CritType
{
	bool InitFromFile(FOMsg* fill_msg);
	bool InitFromMsg(FOMsg* msg);

	bool IsEnabled(DWORD cr_type);
	CritTypeType& GetCritType(DWORD cr_type);
	CritTypeType& GetRealCritType(DWORD cr_type);
	const char* GetName(DWORD cr_type);
	const char* GetSoundName(DWORD cr_type);
	DWORD GetAlias(DWORD cr_type);
	DWORD GetMultihex(DWORD cr_type);
	bool IsCanWalk(DWORD cr_type);
	bool IsCanRun(DWORD cr_type);
	bool IsCanAim(DWORD cr_type);
	bool IsCanArmor(DWORD cr_type);
	bool IsCanRotate(DWORD cr_type);
	bool IsAnim1(DWORD cr_type, DWORD anim1);
	bool IsAnim3d(DWORD cr_type);

	void SetWalkParams(DWORD cr_type, DWORD time_walk, DWORD time_run, DWORD step0, DWORD step1, DWORD step2, DWORD step3);
	DWORD GetTimeWalk(DWORD cr_type);
	DWORD GetTimeRun(DWORD cr_type);
	int GetWalkFrmCnt(DWORD cr_type, DWORD step);
	int GetRunFrmCnt(DWORD cr_type, DWORD step);
}

#endif // __CRITTER_TYPE__