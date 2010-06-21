#ifndef _SOCIALS_H_
#define _SOCIALS_H_

#include "common.h"

#include "Critter.h"

struct soc_def
{
	char cmd[30];
	char NoVictimSelf[256];
	char NoVictimAll[256];
	char VictimSelf[256];
	char VictimAll[256];
	char VictimVictim[256];
	char VictimErr[256];
	char SelfSelf[256];
	char SelfAll[256];
};

#define	SOC_NOPARAMS	0
#define	SOC_NOSELF		1
#define	SOC_PARAMSOK	2

void LoadSocials(MYSQL* mysql);
void UnloadSocials();
void ParseSymbolStr(char* symstr,char* resstr, CritData* self, CritData* victim);

int GetSocialId(char* cmd);

void GetSocNoStr(int socid, char* SelfStr, char* AllStr, CritData* self);
void GetSocSelfStr(int socid, char* SelfStr, char* AllStr, CritData* self);
void GetSocVicStr(int socid, char* SelfStr, char* VicStr, char* AllStr, CritData* self, CritData* victim);
void GetSocVicErrStr(int socid, char* SelfStr, CritData* self);

int GetPossParams(int socid);

#endif