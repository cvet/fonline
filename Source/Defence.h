#ifndef __DEFENCE__
#define __DEFENCE__

#include <Iphlpapi.h>
#include <intrin.h>
#pragma comment(lib,"Iphlpapi.lib")

uint UIDDUMMY0=0xF145668A;
uint UIDDUMMY1=321125;
uint UIDCACHE2[5];
uint UIDDUMMY2=-1;
uint UIDXOR=0xF145668A;
uint UIDDUMMY3[33];
uint UIDCALC=0x45012345;
uint UIDDUMMY4[13];
uint UIDOR=0;
uint UIDDUMMY5=23423111;
uint UIDDUMMY6[17];
uint UIDDUMMY7=-1;
uint UIDCACHE[5];
uint UIDDUMMY8=23423111;
uint UIDDUMMY9[55];
uint UIDDUMMY10=-1;

#define CHECK_UID0(uid) (uid[0]!=*UID0 || uid[1]!=*UID1 || uid[2]!=*UID2 || uid[3]!=*UID3 || uid[4]!=*UID4)
#define CHECK_UID1(uid) (uid[0]!=UIDCACHE[0] || uid[1]!=UIDCACHE[1] || uid[2]!=UIDCACHE[2] || uid[3]!=UIDCACHE[3] || uid[4]!=UIDCACHE[4])
#define CHECK_UID2(uid) (uid[0]!=UIDCACHE2[0] || uid[1]!=UIDCACHE2[1] || uid[2]!=UIDCACHE2[2] || uid[3]!=UIDCACHE2[3] || uid[4]!=UIDCACHE2[4])

#define UID_FLAGS(result,set,unset) if(result) SETFLAG(result,set); UNSETFLAG(result,unset)
#define UID_CALC(result) UIDXOR^=result; UIDOR|=result; UIDCALC+=result

#define UID_DUMMY_CALCS0 for(int i=0;i<Random(50,100);i++) {UIDDUMMY3[Random(5,25)]^=UIDDUMMY5;}
#define UID_DUMMY_CALCS1 UIDDUMMY3[16]&=UIDDUMMY9[44]; UID_DUMMY_CALCS3
#define UID_DUMMY_CALCS2 for(int i=0;i<96;i++) {UIDDUMMY3[Random(5,25)]^=UIDDUMMY5;}
#define UID_DUMMY_CALCS3 for(int i=0,j=Random(111,569);i<j;i++) {UIDDUMMY3[Random(5,25)]^=UIDDUMMY5;}
#define UID_DUMMY_CALCS4 UIDDUMMY9[6]=UIDDUMMY9[44]; UIDDUMMY1=UIDDUMMY4[5]; UID_DUMMY_CALCS9
#define UID_DUMMY_CALCS5 UIDDUMMY1=UIDDUMMY9[11]; UIDDUMMY7=UIDDUMMY9[5]; UIDDUMMY9[5]=Random(0,11)
#define UID_DUMMY_CALCS6 UIDDUMMY3[5]^=UIDDUMMY0; UID_DUMMY_CALCS0
#define UID_DUMMY_CALCS7 for(int i=0;i<12;i++) {UIDDUMMY3[Random(5,25)]^=UIDDUMMY5;}
#define UID_DUMMY_CALCS8 UIDDUMMY4[1]^=UIDDUMMY4[2]|=UIDDUMMY4[6]*=UIDDUMMY4[7]
#define UID_DUMMY_CALCS9 UIDDUMMY9[23]=Random(0,11)

#define UID_CHANGE    (0)

// -
// +-
// -
// -
// +

#define GET_UID0(result) \
	SYSTEM_INFO d_sysinfo;\
	ZeroMemory(&d_sysinfo,sizeof(d_sysinfo));\
	UID_DUMMY_CALCS0;\
	GetSystemInfo(&d_sysinfo);\
	result=new uint();\
	*result=d_sysinfo.wProcessorArchitecture+UID_CHANGE;\
	UID_DUMMY_CALCS1;\
	*result^=(d_sysinfo.dwNumberOfProcessors<<8);\
	*result*=(d_sysinfo.dwProcessorType<<10);\
	UID_DUMMY_CALCS2;\
	*result^=(d_sysinfo.wProcessorLevel<<12);\
	UID_DUMMY_CALCS3;\
	*result+=(d_sysinfo.wProcessorRevision<<16);\
	UID_DUMMY_CALCS4;\
	for(int d_i=0;d_i<32;d_i++) *result|=((IsProcessorFeaturePresent(d_i)?1:0)<<d_i);\
	UID_DUMMY_CALCS1;\
	UID_FLAGS(*result,0x00800000,0x00000400);\
	UID_CALC(*result);\
	UIDCACHE[0]=*result;\
	UID_DUMMY_CALCS4;\
	UIDCACHE2[0]=*result

#define GET_UID1(result) \
	PIP_ADAPTER_INFO d_ainfo=NULL;\
	uint d_retv=0;\
	d_ainfo=(IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));\
	UID_DUMMY_CALCS5;\
	DWORD d_buf_len=sizeof(IP_ADAPTER_INFO);\
	UID_DUMMY_CALCS6;\
	uint d_retval=-1;\
	if((d_retval=GetAdaptersInfo(d_ainfo,&d_buf_len))!=ERROR_SUCCESS)\
	{\
		free(d_ainfo);\
		UID_DUMMY_CALCS7;\
		d_ainfo=(IP_ADAPTER_INFO*)malloc(d_buf_len);\
		d_retval=GetAdaptersInfo(d_ainfo,&d_buf_len);\
	}\
	result=new uint();\
	if(d_retval==ERROR_SUCCESS)\
	{\
		uint d_lo=*(uint*)&d_ainfo->Address[4];\
		UID_DUMMY_CALCS8;\
		d_lo|=(d_lo<<16);\
		*result=*(uint*)&d_ainfo->Address[0]+UID_CHANGE;\
		UID_DUMMY_CALCS9;\
		*result^=d_lo;\
	}\
	else *result=0;\
	UID_DUMMY_CALCS3;\
	UID_FLAGS(*result,0x04000000,0x00200000);\
	UID_DUMMY_CALCS7;\
	UID_CALC(*result);\
	UIDCACHE[1]=*result;\
	UID_DUMMY_CALCS8;\
	UIDCACHE2[1]=*result

#define GET_UID2(result) \
	UID_DUMMY_CALCS6;\
	result=new uint();\
	UID_DUMMY_CALCS4;\
	HW_PROFILE_INFO d_hwpi;\
	if(GetCurrentHwProfile(&d_hwpi))\
	{\
		*result=*(uint*)&d_hwpi.szHwProfileGuid[0];\
		for(int d_i=1;d_i<HW_PROFILE_GUIDLEN/4;d_i++) *result^=*(uint*)&d_hwpi.szHwProfileGuid[d_i*4];\
		/*WriteLog(NULL,"GetCurrentHwProfile <%s><%X>.\n",d_hwpi.szHwProfileGuid,*result);*/\
	}\
	else *result=0;\
	UID_FLAGS(*result,0x00000020,0x00000800);\
	UID_DUMMY_CALCS4;\
	UIDCACHE[2]=*result;\
	UIDCACHE2[2]=*result;\
	UID_DUMMY_CALCS1;\
	UID_CALC(*result);\
	UID_DUMMY_CALCS8

#define GET_UID3(result) \
	int d_cpuid[4]={0,0,0,0};\
	__cpuid(d_cpuid,1);\
	result=new uint();\
	UID_DUMMY_CALCS6;\
	*result=d_cpuid[0]+UID_CHANGE;\
	UID_DUMMY_CALCS4;\
	UID_DUMMY_CALCS5;\
	UID_FLAGS(*result,0x80000000,0x40000000);\
	UIDCACHE[3]=*result;\
	UID_DUMMY_CALCS1;\
	UIDCACHE2[3]=*result;\
	UID_DUMMY_CALCS9;\
	UID_DUMMY_CALCS4;\
	UID_CALC(*result);\
	UID_DUMMY_CALCS8

#define GET_UID4(result) \
	result=new uint();\
	DWORD d_vol=0+UID_CHANGE;\
	UID_DUMMY_CALCS2;\
	if(GetVolumeInformation(NULL,NULL,NULL,&d_vol,NULL,NULL,NULL,0)!=ERROR_SUCCESS) d_vol^=0x12345678;\
	else d_vol=0;\
	UID_DUMMY_CALCS4;\
	*result=d_vol;\
	UID_DUMMY_CALCS6;\
	UIDCACHE[4]=*result;\
	UID_DUMMY_CALCS6;\
	UID_DUMMY_CALCS7;\
	UID_FLAGS(*result,0x00000800,0x00004000);\
	UID_DUMMY_CALCS8;\
	UIDCACHE[4]=*result;\
	UID_CALC(*result);\
	UIDCACHE2[4]=*result;\
	UID_DUMMY_CALCS2

#endif // __DEFENCE__