#include "StdAfx.h"
#include "CritterType.h"
#include "Text.h"
#include "FileManager.h"

/************************************************************************/
/* Parameters                                                           */
/************************************************************************/

// None, Empty, KO/Dead, KO Rise, Knife, Club, Hammer, Spear, Pistol, Uzi, Shootgun, Rifle, Minigun, Rocket Launcher, Aim, None, Sniper, Wakizashi, Rip, None
//  _      A       B        C       D     E      F       G      H      I      J        K       L           M           N    O      P         Q       R    ST
// None can be == 0

CritTypeType CrTypes[MAX_CRIT_TYPES]=
{
	// File name, Alias, Ext
		// 3d, Can Walk, Can Run, Can Aim, Can Armor, Can Rotate
                            // _ A B C D E F G H I J K L M N O P Q R S T
	{1,"hmjmps",0,  0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,0,0}, // Default animation
	{1,"hapowr",21, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,0,0},   // kt*
	{1,"harobe",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,0,0},   // kt*
	{1,"hfcmbt",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,0,0},
	{1,"hfjmps",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,0,0},
	{1,"hflthr",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,0,0},
	{1,"hfmaxx",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,0,0},
	{1,"hfmetl",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,0,0},
	{1,"hmbjmp",11, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"hmbmet",11, 0,1,0,1,1,1, 0,1,1,1,1,0,1,0,1,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"hmcmbt",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,0,0},
	{1,"hmjmps",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,0,0},
	{1,"hmlthr",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,0,0},
	{1,"hmmaxx",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,0,0},
	{1,"hmmetl",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,1,1,0,0},
	{1,"mabrom",15, 0,1,0,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"maddog",16, 0,1,1,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"mahand",17, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,1,1,0,1,0,0,0,1,0,0},
	{1,"haenvi",18, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,0,0},
	{1,"mamrat",19, 0,1,0,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"mamtn2",21, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,1,1,1,0,0,0,0,1,0,0},
	{1,"mamtnt",21, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,1,1,1,1,0,0,0,1,0,0},
	{1,"mascrp",22, 0,1,0,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"masphn",23, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"masrat",24, 0,1,0,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"mathng",25, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"nablue",11, 0,1,1,1,1,1, 0,1,1,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,1,0,0},   //kt*
	{1,"nachld",27, 0,1,1,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"naghul",11, 0,1,0,1,0,1, 0,1,1,1,1,0,0,1,0,0,1,0,0,0,0,0,0,0,1,0,0},
	{1,"naglow",11, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"napowr",21, 0,1,1,1,1,1, 0,1,1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,0,0},   //kt*
	{1,"narobe",11, 0,1,1,1,1,1, 0,1,1,1,1,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0},   //kt*
	{1,"nmval0",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,0,0},
	{1,"nfbrlp",11, 0,1,1,1,0,1, 0,1,1,1,0,0,0,0,1,0,1,0,0,0,0,0,0,0,1,0,0},
	{1,"nfmaxx",11, 0,1,1,1,0,1, 0,1,1,1,1,0,0,1,0,1,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nfmetl",11, 0,1,1,1,0,1, 0,1,1,1,1,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nfpeas",11, 0,1,1,1,0,1, 0,1,1,1,1,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nftrmp",11, 0,1,1,1,0,1, 0,1,1,1,1,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nfvred",11, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nmbpea",11, 0,1,1,1,0,1, 0,1,1,1,1,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,0},
	{1,"nmbrlp",11, 0,1,1,1,0,1, 0,1,1,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0},
	{1,"nmbsnp",11, 0,1,1,1,0,1, 0,1,1,1,0,1,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nmgrch",27, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nmlosr",11, 0,1,0,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nmlthr",11, 0,1,1,1,0,1, 0,1,1,1,0,0,0,1,1,0,1,0,0,0,0,0,0,0,1,0,0},
	{1,"nmmaxx",11, 0,1,1,1,0,1, 0,1,1,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nmval1",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,0,0},
	{1,"hmgant",47, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,0,0},
	{1,"nmpeas",11, 0,1,1,1,0,1, 0,1,1,1,1,1,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0},
	{0,"reserv",49, 0,0,0,0,0,1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{0,"reserv",50, 0,0,0,0,0,1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{1,"maclaw",51, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"mamant",52, 0,1,0,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"marobo",53, 0,1,1,1,0,1, 0,1,1,1,0,0,0,0,0,1,1,0,0,0,1,0,0,0,1,0,0},
	{1,"mafeye",54, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"mamurt",55, 0,1,0,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"nabrwn",11, 0,1,1,1,1,1, 0,1,1,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0},   //kt*
	{1,"nmdocc",11, 0,1,1,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"madegg",58, 0,0,0,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},   //kt**
	{1,"mascp2",59, 0,1,0,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"maclw2",60, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"hfprim",11, 0,1,1,1,1,1, 0,1,1,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"hmwarr",11, 0,1,1,1,1,1, 0,1,1,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nfprim",11, 0,1,1,1,1,1, 0,1,1,1,1,0,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nmwarr",11, 0,1,1,1,1,1, 0,1,1,1,1,0,1,1,0,1,0,0,0,0,0,0,0,0,1,0,0},
	{1,"maplnt",65, 0,0,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"marobt",66, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,1,1,1,0,0,0,1,0,0},
	{1,"magko2",67, 0,1,1,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"magcko",68, 0,1,1,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"nmvalt",11, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,0,0},
	{1,"macybr",16, 0,1,1,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},   //kt**
	{1,"hanpwr",21, 0,1,1,1,1,1, 0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,0,0},   //kt*
	{1,"nmnice",11, 0,1,1,1,1,1, 0,1,1,1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nfnice",11, 0,1,1,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nfvalt",11, 0,1,0,1,1,1, 0,1,1,1,1,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"macybr",16, 0,1,1,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"mabran",19, 0,1,0,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nmbonc",11, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0},
	{1,"nmbrsr",21, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0},
	{1,"navgul",11, 0,1,1,1,0,1, 0,1,1,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,1,0,0},
	{1,"malien",80, 0,1,0,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"mafire",68, 0,1,1,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,1,0,0,1,0,0,0,1,0,0},
	{1,"nmasia",11, 0,1,1,1,1,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nflynn",11, 0,1,1,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},   //kt*
	{1,"nawhit",11, 0,1,1,1,1,1, 0,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,0,0},   //kt*
	{1,"maboss",85, 0,1,0,1,0,1, 0,1,1,1,1,0,0,0,0,0,0,1,0,0,1,0,0,0,1,0,0},
	{1,"maquen",86, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"nmcopp",11, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,0,0},
	{1,"nmmyrn",11, 0,1,1,1,0,1, 0,1,1,1,1,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nmlabb",11, 0,1,1,1,0,1, 0,1,1,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"magun2",90, 0,0,0,1,0,1, 0,1,1,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"nmfatt",11, 0,1,1,1,0,1, 0,1,1,1,1,0,0,0,1,0,1,0,0,0,0,0,0,0,1,0,0},
	{1,"nmrgng",11, 0,1,1,1,1,1, 0,1,1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,1,0,0},
	{1,"nmgang",11, 0,1,1,1,1,1, 0,1,1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,1,0,0},
	{1,"nfasia",11, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"nmmexi",11, 0,1,1,1,0,1, 0,1,1,1,1,0,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0},
	{1,"nmboxx",11, 0,1,0,1,0,1, 0,1,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
	{1,"maantt",97, 0,1,0,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"nmoldd",11, 0,1,0,1,0,1, 0,1,1,1,0,0,0,0,1,0,1,0,0,0,0,0,0,0,1,0,0},
	{1,"marobe",99, 0,1,1,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"madeth",100,0,1,1,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
	{1,"magunn",101,0,0,0,1,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,1,0,1,0,0,0,1,0,0},
	{1,"mabos2",102,0,0,0,0,0,0, 0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},   //kt**
	{1,"nfchld",27, 0,1,1,0,0,1, 0,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0},
};

bool CritType::IsEnabled(DWORD cr_type)
{
	return cr_type && cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled;
}

CritTypeType& CritType::GetCritType(DWORD cr_type)
{
	return cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled?CrTypes[cr_type]:CrTypes[DEFAULT_CRTYPE];
}

CritTypeType& CritType::GetRealCritType(DWORD cr_type)
{
	return CrTypes[cr_type];
}

const char* CritType::GetName(DWORD cr_type)
{
	return GetCritType(cr_type).Name;
}

DWORD CritType::GetAlias(DWORD cr_type)
{
	return GetCritType(cr_type).Alias;
}

bool CritType::IsCanWalk(DWORD cr_type)
{
	return GetCritType(cr_type).CanWalk;
}

bool CritType::IsCanRun(DWORD cr_type)
{
	return GetCritType(cr_type).CanRun;
}

bool CritType::IsCanAim(DWORD cr_type)
{
	return GetCritType(cr_type).CanAim;
}

bool CritType::IsCanArmor(DWORD cr_type)
{
	return GetCritType(cr_type).CanArmor;
}

bool CritType::IsCanRotate(DWORD cr_type)
{
	return GetCritType(cr_type).CanRotate;
}

bool CritType::IsAnim1(DWORD cr_type, DWORD anim1)
{
	if(anim1>=21) return false;
	return GetCritType(cr_type).Anim1[anim1];
}

bool CritType::IsAnim3d(DWORD cr_type)
{
	return GetCritType(cr_type).Is3d;
}

/************************************************************************/
/* Move frm counts                                                      */
/************************************************************************/

int MoveWalk[MAX_CRIT_TYPES][6]=
{
	// Walk, Run
	{4,8,0,0,400,  200}, //0=hmjmps +
	{4,8,0,0,400,  200}, //1=hapowr  +
	{4,8,0,0,400,  200}, //2=harobe  +
	{4,8,0,0,400,  200}, //3=hfcmbt  +
	{4,8,0,0,400,  200}, //4=hfjmps  +
	{4,8,0,0,400,  200}, //5=hflthr  +
	{4,8,0,0,400,  200}, //6=hfmaxx  +
	{4,8,0,0,400,  200}, //7=hfmetl  +
	{4,8,0,0,400,  0},   //8=hmbjmp  +
	{4,8,0,0,400,  0},   //9=hmbmet  +
	{4,8,0,0,400,  200}, //10=hmcmbt +
	{4,8,0,0,400,  200}, //11=hmjmps +
	{4,8,0,0,400,  200}, //12=hmlthr +
	{4,8,0,0,400,  200}, //13=hmmaxx +
	{4,8,0,0,400,  200}, //14=hmmetl +
	{8,0,0,0,1000, 0},   //15=mabrom +
	{8,0,0,0,800,  200}, //16=maddog +
	{3,6,8,0,266,  0},   //17=mahand +
	{6,12,0,0,400, 0},   //18=haenvi + new
	{3,6,8,0,266,  0},   //19=mamrat +
	{5,10,0,0,500, 0},   //20=mamtn2 +
	{5,10,0,0,500, 0},   //21=mamtnt +
	{7,0,0,0,800,  0},   //22=mascrp + -1
	{8,0,0,0,800,  0},   //23=masphn +
	{10,0,0,0,710, 0},   //24=masrat +
	{8,0,0,0,800,  0},   //25=mathng +
	{4,8,0,0,400,  200}, //26=nablue +
	{8,0,0,0,800,  200}, //27=nachld +
	{10,0,0,0,1000,0},   //28=naghul +
	{10,0,0,0,1250,0},   //29=naglow +
	{4,8,0,0,400,  200}, //30=napowr +
	{8,0,0,0,800,  200}, //31=narobe +
	{4,8,0,0,400,  0},   //32=nmval0 + new
	{8,0,0,0,800,  200}, //33=nfbrlp +
	{4,8,0,0,400,  200}, //34=nfmaxx +
	{4,8,0,0,400,  200}, //35=nfmetl +
	{4,8,0,0,400,  200}, //36=nfpeas +
	{8,0,0,0,800,  200}, //37=nftrmp +
	{4,8,0,0,400,  0},   //38=nfvred +
	{8,0,0,0,800,  200}, //39=nmbpea +
	{8,0,0,0,800,  200}, //40=nmbrlp +
	{8,0,0,0,800,  200}, //41=nmbsnp +
	{8,0,0,0,800,  0},   //42=nmgrch +
	{12,0,0,0,996, 0},   //43=nmlosr +
	{4,8,0,0,400,  200}, //44=nmlthr +
	{4,8,0,0,400,  200}, //45=nmmaxx +
	{4,8,0,0,400,  200}, //46=nmval1 + new
	{6,11,0,0,400, 0},   //47=hmgant + new
	{8,0,0,0,800,  200}, //48=nmpeas +
	{0,0,0,0,0,    0},   //49=reserv =
	{0,0,0,0,0,    0},   //50=reserv =
	{3,6,8,0,333,  0},   //51=maclaw +
	{8,0,0,0,528,  0},   //52=mamant +
	{15,0,0,0,750, 200}, //53=marobo +
	{3,5,7,9,225,  0},   //54=mafeye +
	{8,0,0,0,800,  0},   //55=mamurt +
	{4,8,0,0,400,  200}, //56=nabrwn +
	{8,4,0,0,800,  200}, //57=nmdocc +
	{0,0,0,0,0,    0},   //58=madegg +
	{8,0,0,0,528,  0},   //59=mascp2 + need add
	{4,8,0,0,400,  0},   //60=maclw2 + need add
	{4,8,0,0,400,  200}, //61=hfprim +
	{4,8,0,0,400,  200}, //62=hmwarr +
	{4,8,0,0,400,  200}, //63=nfprim +
	{4,8,0,0,400,  200}, //64=nmwarr +
	{0,0,0,0,0,    0},   //65=maplnt +
	{7,15,0,0,750, 0},   //66=marobt +
	{15,0,0,0,750, 200}, //67=magko2 + need add
	{15,0,0,0,750, 200}, //68=magcko + need add
	{4,8,0,0,400,  200}, //69=nmvalt +
	{8,0,0,0,664,  200}, //70=macybr +
	{4,8,0,0,400,  200}, //71=hanpwr +
	{11,0,0,0,836, 200}, //72=nmnice +
	{15,0,0,0,990, 200}, //73=nfnice +
	{4,8,0,0,400,  0},   //74=nfvalt +
	{8,0,0,0,664,  200}, //75=macybr +
	{4,7,0,0,400,  0},   //76=mabran + -1
	{15,0,0,0,750, 0},   //77=nmbonc +
	{15,0,0,0,990, 0},   //78=nmbrsr +
	{11,0,0,0,1100,200}, //79=navgul +
	{10,0,0,0,465, 0},   //80=malien + -5 bad
	{15,0,0,0,495, 200}, //81=mafire + need add
	{11,0,0,0,836, 200}, //82=nmasia +
	{10,0,0,0,495, 200}, //83=nflynn + -5 bad
	{4,8,0,0,400,  200}, //84=nawhit +
	{4,9,11,0,366, 0},   //85=maboss +
	{10,0,0,0,495, 0},   //86=maquen + -5 bad like malien
	{11,0,0,0,726, 0},   //87=nmcopp +
	{11,0,0,0,836, 200}, //88=nmmyrn +
	{11,0,0,0,836, 200}, //89=nmlabb +
	{0,0,0,0,0,    0},   //90=magun2 +
	{11,0,0,0,913, 200}, //91=nmfatt +
	{11,0,0,0,726, 200}, //92=nmrgng +
	{11,0,0,0,836, 200}, //93=nmgang +
	{10,0,0,0,495, 0},   //94=nfasia + -5 bad like nflynn
	{11,0,0,0,836, 200}, //95=nmmexi +
	{6,11,0,0,550, 0},   //96=nmboxx +
	{6,0,0,0,550,  0},   //97=maantt + -5
	{11,0,0,0,1100,0},   //98=nmoldd +
	{3,7,0,0,400,  200}, //99=marobe + -1
	{3,7,0,0,400,  200}, //100=madeth + -1 not good
	{0,0,0,0,0,    0},   //101=magunn +
	{0,0,0,0,0,    0},   //102=mabos2 +
	{8,0,0,0,800,  200}, //103=nfchld +
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 199
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 199
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 199
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 199
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 199
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 104
	{0,0,0,0,0,    0},   // 199
};

void CritType::SetWalkParams(DWORD cr_type, DWORD time_walk, DWORD time_run, DWORD step0, DWORD step1, DWORD step2, DWORD step3)
{
	if(cr_type>=MAX_CRIT_TYPES) return;
	MoveWalk[cr_type][0]=step0;
	MoveWalk[cr_type][1]=step1;
	MoveWalk[cr_type][2]=step2;
	MoveWalk[cr_type][3]=step3;
	MoveWalk[cr_type][4]=time_walk;
	MoveWalk[cr_type][5]=time_run;
}

DWORD CritType::GetTimeWalk(DWORD cr_type)
{
	return cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled?MoveWalk[cr_type][4]:0;
}

DWORD CritType::GetTimeRun(DWORD cr_type)
{
	return cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled?MoveWalk[cr_type][5]:0;
}

int CritType::GetWalkFrmCnt(DWORD cr_type, DWORD step)
{
	return cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled && step<4?MoveWalk[cr_type][step]:0;
}

int CritType::GetRunFrmCnt(DWORD cr_type, DWORD step)
{
	return 0;
/*
	return cr_type<MAX_CRIT_TYPES && CrTypes[cr_type].Enabled && step<4?MoveWalk[cr_type][step]:0;

	if(cr_type>=MAX_CRIT_TYPES) return 0;
	if(step>3) return 0;

	return 0;*/
}

bool CritType::InitFromFile(FOMsg* fill_msg)
{
	AutoPtrArr<CritTypeType> CrTypesReserved(new CritTypeType[MAX_CRIT_TYPES]);
	if(!CrTypesReserved.IsValid()) return false;
	int MoveWalkReserved[MAX_CRIT_TYPES][6];
	ZeroMemory(CrTypesReserved.Get(),sizeof(CritTypeType)*MAX_CRIT_TYPES);
	ZeroMemory(MoveWalkReserved,sizeof(MoveWalkReserved));

	FileManager file;
	if(!file.LoadFile(CRTYPE_FILE_NAME,PT_SERVER_DATA_DATA))
	{
		WriteLog(__FUNCTION__" - File<%s> not found.\n",FileManager::GetFullPath(CRTYPE_FILE_NAME,PT_SERVER_DATA_DATA));
		return false;
	}

	char line[2048];
	string svalue;
	int number;
	int errors=0;
	int success=0;
	bool prev_fail=false;
	while(file.GetLine(line,2048))
	{
		if(prev_fail)
		{
			WriteLog(__FUNCTION__" - Bad data for critter type information, number<%d>.\n",number);
			prev_fail=false;
			errors++;
		}

		istrstream str(line);

		str >> svalue;
		if(svalue!="@") continue;

		prev_fail=true;

		// Number
		str >> number;
		if(str.fail() || number<0 || number>=MAX_CRIT_TYPES) continue;
		CritTypeType& ct=CrTypesReserved.Get()[number];

		// Name
		str >> svalue;
		if(str.fail() || svalue.length()!=6) continue;
		StringCopy(ct.Name,svalue.c_str());

		// Alias, 3d, Walk, Run, Aim, Armor, Rotate
		str >> ct.Alias;
		if(str.fail()) continue;
		str >> ct.Is3d;
		if(str.fail()) continue;
		str >> ct.CanWalk;
		if(str.fail()) continue;
		str >> ct.CanRun;
		if(str.fail()) continue;
		str >> ct.CanAim;
		if(str.fail()) continue;
		str >> ct.CanArmor;
		if(str.fail()) continue;
		str >> ct.CanRotate;
		if(str.fail()) continue;

		// A B C D E F G H I J K L M N O P Q R S T
		for(int i=1;i<=20;i++)
		{
			str >> ct.Anim1[i];
			if(str.fail()) break;
		}
		if(str.fail()) continue;

		// Walk, Run, Walk steps
		str >> MoveWalkReserved[number][4];
		if(str.fail()) continue;
		str >> MoveWalkReserved[number][5];
		if(str.fail()) continue;
		str >> MoveWalkReserved[number][0];
		if(str.fail()) continue;
		str >> MoveWalkReserved[number][1];
		if(str.fail()) continue;
		str >> MoveWalkReserved[number][2];
		if(str.fail()) continue;
		str >> MoveWalkReserved[number][3];
		if(str.fail()) continue;

		// Register as valid
		ct.Anim1[0]=true;
		ct.Enabled=true;
		success++;
		prev_fail=false;
	}

	if(errors) return false;

	if(!CrTypes[0].Enabled)
	{
		WriteLog(__FUNCTION__" - Default zero type not loaded.\n");
		return false;
	}

	memcpy(CrTypes,CrTypesReserved.Get(),sizeof(CrTypes));
	memcpy(MoveWalk,MoveWalkReserved,sizeof(MoveWalk));

	if(fill_msg)
	{
		char str[2048];
		for(int i=0;i<MAX_CRIT_TYPES;i++)
		{
			CritTypeType& ct=CrTypes[i];
			if(!ct.Enabled) continue;

			sprintf(str,"%s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",ct.Name,
				ct.Alias,ct.Is3d,ct.CanWalk,ct.CanRun,ct.CanAim,ct.CanArmor,ct.CanRotate,
				ct.Anim1[1],ct.Anim1[2],ct.Anim1[3],ct.Anim1[4],ct.Anim1[5],ct.Anim1[6],ct.Anim1[7],
				ct.Anim1[8],ct.Anim1[9],ct.Anim1[10],ct.Anim1[11],ct.Anim1[12],ct.Anim1[13],ct.Anim1[14],
				ct.Anim1[15],ct.Anim1[16],ct.Anim1[17],ct.Anim1[18],ct.Anim1[19],ct.Anim1[20],
				MoveWalkReserved[i][4],MoveWalkReserved[i][5],
				MoveWalkReserved[i][0],MoveWalkReserved[i][1],MoveWalkReserved[i][2],MoveWalkReserved[i][3]);

			fill_msg->AddStr(STR_INTERNAL_CRTYPE(i),str);
		}
	}

	WriteLog("Loaded<%d> critter types.\n",success);
	return true;
}

bool CritType::InitFromMsg(FOMsg* msg)
{
	if(!msg)
	{
		WriteLog(__FUNCTION__" - Msg nullptr.\n");
		return false;
	}

	AutoPtrArr<CritTypeType> CrTypesReserved(new CritTypeType[MAX_CRIT_TYPES]);
	if(!CrTypesReserved.IsValid()) return false;
	int MoveWalkReserved[MAX_CRIT_TYPES][6];
	ZeroMemory(CrTypesReserved.Get(),sizeof(CritTypeType)*MAX_CRIT_TYPES);
	ZeroMemory(MoveWalkReserved,sizeof(MoveWalkReserved));

	int errors=0;
	int success=0;
	for(int i=0;i<MAX_CRIT_TYPES;i++)
	{
		if(!msg->Count(STR_INTERNAL_CRTYPE(i))) continue;
		const char* str=msg->GetStr(STR_INTERNAL_CRTYPE(i));
		CritTypeType& ct=CrTypesReserved.Get()[i];

		char name[128]={0};
		if(sscanf(str,"%s%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d",name,
			&ct.Alias,&ct.Is3d,&ct.CanWalk,&ct.CanRun,&ct.CanAim,&ct.CanArmor,&ct.CanRotate,
			&ct.Anim1[1],&ct.Anim1[2],&ct.Anim1[3],&ct.Anim1[4],&ct.Anim1[5],&ct.Anim1[6],&ct.Anim1[7],
			&ct.Anim1[8],&ct.Anim1[9],&ct.Anim1[10],&ct.Anim1[11],&ct.Anim1[12],&ct.Anim1[13],&ct.Anim1[14],
			&ct.Anim1[15],&ct.Anim1[16],&ct.Anim1[17],&ct.Anim1[18],&ct.Anim1[19],&ct.Anim1[20],
			&MoveWalkReserved[i][4],&MoveWalkReserved[i][5],
			&MoveWalkReserved[i][0],&MoveWalkReserved[i][1],&MoveWalkReserved[i][2],&MoveWalkReserved[i][3])!=34
			|| strlen(name)!=6)
		{
			WriteLog(__FUNCTION__" - Bad data for critter type information, number<%d>, line<%s>.\n",i,str);
			errors++;
			continue;
		}

		StringCopy(ct.Name,name);
		ct.Anim1[0]=true;
		ct.Enabled=true;
		success++;
	}

	if(errors) return false;

	if(!CrTypes[0].Enabled)
	{
		WriteLog(__FUNCTION__" - Default zero type not loaded.\n");
		return false;
	}

	memcpy(CrTypes,CrTypesReserved.Get(),sizeof(CrTypes));
	memcpy(MoveWalk,MoveWalkReserved,sizeof(MoveWalk));
	WriteLog("Loaded<%d> critter types.\n",success);
	return true;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/