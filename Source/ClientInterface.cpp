#include "StdAfx.h"
#include "Client.h"
#include "Version.h"

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

bool FOClient::IfaceLoadRect(INTRECT& comp, const char* name)
{
	char res[256];
	if(!IfaceIni.GetStr(name,"",res))
	{
		WriteLog("Signature<%s> not found.\n",name);
		return false;
	}

	if(sscanf(res,"%d%d%d%d",&comp[0],&comp[1],&comp[2],&comp[3])!=4)
	{
		comp.Clear();
		WriteLog("Unable to parse signature<%s>.\n",name);
		return false;
	}

	return true;
}

void FOClient::IfaceLoadRect2(INTRECT& comp, const char* name, int ox, int oy)
{
	if(IfaceLoadRect(comp,name)) comp=INTRECT(comp,ox,oy);
}

void FOClient::IfaceLoadSpr(DWORD& comp, const char* name)
{
	char res[256];
	if(!IfaceIni.GetStr(name,"none.png",res)) WriteLog("Signature<%s> not found.\n",name);
	SprMngr.SurfType=RES_IFACE;
	if(!(comp=SprMngr.LoadSprite(res,PT_ART_INTRFACE))) WriteLog("File<%s> not found.\n",res);
	SprMngr.SurfType=RES_NONE;
}

void FOClient::IfaceLoadAnim(DWORD& comp, const char* name)
{
	char res[256];
	if(!IfaceIni.GetStr(name,"none.png",res)) WriteLog("Signature<%s> not found.\n",name);
	if(!(comp=AnimLoad(res,PT_ART_INTRFACE,RES_IFACE))) WriteLog("Can't load animation<%s>.\n",res);
}

void FOClient::IfaceFreeResources()
{
	AnimFree(RES_IFACE); // Also free sprites from IfaceLoad*
	ResMngr.FreeResources(RES_IFACE_EXT);
}

bool FOClient::InitIfaceIni()
{
	if(!IfaceIni.LoadFile("default.ini",PT_ART_INTRFACE))
	{
		WriteLog(__FUNCTION__" - File \"default.ini\" not found.\n");
		return false;
	}

	FileManager fm;
	if(!fm.LoadFile("resolutions.lst",PT_ART_INTRFACE))
	{
		WriteLog(__FUNCTION__" - File \"resolutions.lst\" not found.\n");
		return false;
	}

	if(GameOpt.UserInterface=="") GameOpt.UserInterface="default";
	DWORD data_len;
	char* data=(char*)Crypt.GetCache("_user_interface",data_len);
	if(data) GameOpt.UserInterface=data;

	char line[256];
	while(fm.GetLine(line,255))
	{
		char fname[256];
		DWORD w,h;
		if(sscanf(line,"%s %u %u",fname,&w,&h)==3)
		{
			if((GameOpt.UserInterface==fname || !_stricmp("default",fname)) && MODE_WIDTH>=w && MODE_HEIGHT>=h)
			{
				sprintf(line,"%s%ux%u.ini",fname,w,h);
				IfaceIni.AppendToBegin(line,PT_ART_INTRFACE);
			}
		}
	}

	return true;
}

int FOClient::InitIface()
{
	WriteLog("Interface initialization.\n");

	if(!InitIfaceIni()) return false;

/************************************************************************/
/* Data                                                                 */
/************************************************************************/
	char res[512];

	WriteLog("Load data.\n");
	// Other
	IfaceHold=IFACE_NONE;
	TargetSmth.Clear();

	// Inventory
	IfaceLoadRect(InvWMain,"InvMain");
	IfaceLoadRect(InvWInv,"InvInv");
	IfaceLoadRect(InvWChosen,"InvChosen");
	IfaceLoadRect(InvWSlot1,"InvSlot1");
	IfaceLoadRect(InvWSlot2,"InvSlot2");
	IfaceLoadRect(InvWArmor,"InvArmor");
	IfaceLoadRect(InvBScrUp,"InvScrUp");
	IfaceLoadRect(InvBScrDn,"InvScrDn");
	IfaceLoadRect(InvBOk,"InvOk");
	IfaceLoadRect(InvWText,"InvText");
	InvX=IfaceIni.GetInt("InvX",-1);
	if(InvX==-1) InvX=(MODE_WIDTH-InvWMain[2])/2;
	InvY=IfaceIni.GetInt("InvY",-1);
	if(InvY==-1) InvY=(MODE_HEIGHT-InvWMain[3])/2;
	InvScroll=0;
	InvHeightItem=IfaceIni.GetInt("InvHeightItem",30);
	for(int i=0,j=SlotsExt.size();i<j;i++) IfaceLoadRect(SlotsExt[i].Rect,SlotsExt[i].IniName);
	InvItemInfo="";
	InvItemInfoScroll=0;
	InvItemInfoMaxScroll=0;

	// Use
	IfaceLoadRect(UseWMain,"UseMain");
	IfaceLoadRect(UseWChosen,"UseChosen");
	IfaceLoadRect(UseWInv,"UseInv");
	IfaceLoadRect(UseBScrUp,"UseScrUp");
	IfaceLoadRect(UseBScrDown,"UseScrDown");
	IfaceLoadRect(UseBCancel,"UseCancel");
	UseX=IfaceIni.GetInt("UseX",-1);
	if(UseX==-1) UseX=(MODE_WIDTH-UseWMain[2])/2;
	UseY=IfaceIni.GetInt("UseY",-1);
	if(UseY==-1) UseY=(MODE_HEIGHT-UseWMain[3])/2;
	UseVectX=0;
	UseVectY=0;
	UseScroll=0;
	UseHeightItem=IfaceIni.GetInt("UseHeightItem",30);
	UseHoldId=0;

	// Interface
	IfaceLoadRect(IntWMain,"IntMain");
	IntX=IfaceIni.GetInt("IntX",-1);
	if(IntX==-1) IntX=(MODE_WIDTH-IntWMain.W())/2;
	else if(IntX==-2) IntX=MODE_WIDTH-IntWMain.W();
	else if(IntX==-3) IntX=0;
	else
	{
		if(IntX<0) IntX=0;
		if(IntX+IntWMain.W()>MODE_WIDTH) IntX=0;
	}
	IntY=MODE_HEIGHT-IntWMain.B;
	IfaceLoadRect2(IntWMain,"IntMain",IntX,IntY);
	IfaceLoadRect2(IntWAddMess,"IntAddMessWindow",IntX,IntY);
	IfaceLoadRect2(IntBAddMess,"IntAddMess",IntX,IntY);
	IfaceLoadRect2(IntBMessFilter1,"IntMessFilter1",IntX,IntY);
	IfaceLoadRect2(IntBMessFilter2,"IntMessFilter2",IntX,IntY);
	IfaceLoadRect2(IntBMessFilter3,"IntMessFilter3",IntX,IntY);
	IfaceLoadRect2(IntBItem,"IntItem",IntX,IntY);
	IfaceLoadRect2(IntBChangeSlot,"IntChangeSlot",IntX,IntY);
	IfaceLoadRect2(IntBInv,"IntInv",IntX,IntY);
	IfaceLoadRect2(IntBMenu,"IntMenu",IntX,IntY);
	IfaceLoadRect2(IntBSkill,"IntSkill",IntX,IntY);
	IfaceLoadRect2(IntBMap,"IntMap",IntX,IntY);
	IfaceLoadRect2(IntBChar,"IntCha",IntX,IntY);
	IfaceLoadRect2(IntBPip,"IntPip",IntX,IntY);
	IfaceLoadRect2(IntBFix,"IntFix",IntX,IntY);
	IfaceLoadRect2(IntWMess,"IntMess",IntX,IntY);
	IfaceLoadRect2(IntWMessLarge,"IntMessLarge",IntX,IntY);
	IfaceLoadRect2(IntHP,"IntHp",IntX,IntY);
	IfaceLoadRect2(IntAC,"IntAc",IntX,IntY);
	IfaceLoadRect2(IntAP,"IntAp",IntX,IntY);
	IfaceLoadRect2(IntBreakTime,"IntBreakTime",IntX,IntY);
	IntAPstepX=IfaceIni.GetInt("IntApStepX",9);
	IntAPstepY=IfaceIni.GetInt("IntApStepY",0);
	IntAPMax=IfaceIni.GetInt("IntApMax",10);
	IfaceLoadRect2(IntWCombat,"IntCombat",IntX,IntY);
	IfaceLoadRect2(IntBCombatTurn,"IntCombatTurn",IntX,IntY);
	IfaceLoadRect2(IntBCombatEnd,"IntCombatEnd",IntX,IntY);
	IfaceLoadRect2(IntWApCost,"IntApCost",IntX,IntY);
	IfaceLoadRect2(IntWAmmoCount,"IntAmmoCount",IntX,IntY);
	IfaceLoadRect2(IntWWearProcent,"IntWearProcent",IntX,IntY);
	IntWAmmoCountStr=INTRECT(IntBItem,7,8);
	IntWWearProcentStr=INTRECT(IntBItem,7,19);
	IfaceLoadRect2(IntWAmmoCountStr,"IntAmmoCountText",IntX,IntY);
	IfaceLoadRect2(IntWWearProcentStr,"IntWearProcentText",IntX,IntY);
	IntVisible=true;
	IntAddMess=false;
	MessBoxFilters.clear();
	MessBoxScroll=0;
	MessBoxMaxScroll=0;
	MessBoxScrollLines=0;
	IfaceLoadRect(IntMessTab,"IntMessTab");
	IntMessTabStepX=IfaceIni.GetInt("IntMessTabStepX",0);
	IntMessTabStepY=IfaceIni.GetInt("IntMessTabStepY",40);
	IntMessTabLevelUp=false;
	IntBItemOffsX=IfaceIni.GetInt("IntItemOffsX",0);
	IntBItemOffsY=IfaceIni.GetInt("IntItemOffsY",-2);
	IntAimX=IfaceIni.GetInt("IntAimX",0);
	IntAimX+=IntX;
	IntAimY=IfaceIni.GetInt("IntAimY",0);
	IntAimY+=IntY;
	IntUseX=IfaceIni.GetInt("IntUseX",0);
	IntUseX+=IntX;
	IntUseY=IfaceIni.GetInt("IntUseY",0);
	IntUseY+=IntY;
	IntAmmoPoints.clear();
	IntWearPoints.clear();
	IntAmmoTick=0;
	IntWearTick=0;

	// Console
	ConsolePicX=IfaceIni.GetInt("ConsoleMainPicX",0);
	ConsolePicY=IfaceIni.GetInt("ConsoleMainPicY",0);
	ConsoleTextX=IfaceIni.GetInt("ConsoleTextX",0);
	ConsoleTextY=IfaceIni.GetInt("ConsoleTextY",0);
	ConsoleEdit=false;
	ConsoleLastKey=0;
	ConsoleStr[0]=0;
	ConsoleCur=0;
	ConsoleHistoryCur=0;

	// Login
	LogFocus=0;
	IfaceLoadRect(LogMain,"LogMain");
	LogX=(MODE_WIDTH-LogMain.W())/2;
	LogY=(MODE_HEIGHT-LogMain.H())/2;
	IfaceLoadRect2(LogWName,"LogName",LogX,LogY);
	IfaceLoadRect2(LogWPass,"LogPass",LogX,LogY);
	IfaceLoadRect2(LogBOk,"LogPlay",LogX,LogY);
	IfaceLoadRect2(LogBOkText,"LogPlayText",LogX,LogY);
	IfaceLoadRect2(LogBReg,"LogReg",LogX,LogY);
	IfaceLoadRect2(LogBRegText,"LogRegText",LogX,LogY);
	IfaceLoadRect2(LogBOptions,"LogOptions",LogX,LogY);
	IfaceLoadRect2(LogBOptionsText,"LogOptionsText",LogX,LogY);
	IfaceLoadRect2(LogBCredits,"LogCredits",LogX,LogY);
	IfaceLoadRect2(LogBCreditsText,"LogCreditsText",LogX,LogY);
	IfaceLoadRect2(LogBExit,"LogExit",LogX,LogY);
	IfaceLoadRect2(LogBExitText,"LogExitText",LogX,LogY);
	IfaceLoadRect(LogWChat,"LogMessageBox");
	IfaceLoadRect(LogWVersion,"LogVersion");

	// Dialog
	DlgCurAnswPage=0;
	DlgMaxAnswPage=0;
	DlgCurAnsw=-1;
	DlgHoldAnsw=-1;
	DlgVectX=0;
	DlgVectY=0;
	DlgIsNpc=0;
	DlgNpcId=0;
	DlgEndTick=0;
	DlgMainText="";
	DlgMainTextCur=0;
	DlgMainTextLinesReal=1;
	IfaceLoadRect(DlgWMain,"DlgMain");
	IfaceLoadRect(DlgWText,"DlgText");
	DlgMainTextLinesRect=SprMngr.GetLinesCount(0,DlgWText.H(),NULL);
	IfaceLoadRect(DlgBScrUp,"DlgScrUp");
	IfaceLoadRect(DlgBScrDn,"DlgScrDn");
	IfaceLoadRect(DlgAnsw,"DlgAnsw");
	IfaceLoadRect2(DlgAnswText,"DlgAnswText",DlgAnsw[0],DlgAnsw[1]);
	IfaceLoadRect2(DlgWMoney,"DlgMoney",DlgAnsw[0],DlgAnsw[1]);
	IfaceLoadRect2(DlgBBarter,"DlgBarter",DlgAnsw[0],DlgAnsw[1]);
	IfaceLoadRect2(DlgBBarterText,"DlgBarterText",DlgAnsw[0],DlgAnsw[1]);
	IfaceLoadRect2(DlgBSay,"DlgSay",DlgAnsw[0],DlgAnsw[1]);
	IfaceLoadRect2(DlgBSayText,"DlgSayText",DlgAnsw[0],DlgAnsw[1]);
	IfaceLoadRect(DlgWAvatar,"DlgAvatar");
	IfaceLoadRect(DlgWTimer,"DlgTimer");
	DlgNextAnswX=IfaceIni.GetInt("DlgNextAnswX",1);
	DlgNextAnswY=IfaceIni.GetInt("DlgNextAnswY",1);
	DlgX=IfaceIni.GetInt("DlgX",-1);
	if(DlgX==-1) DlgX=MODE_WIDTH/2-DlgWMain[2]/2;
	DlgY=IfaceIni.GetInt("DlgY",-1);
	if(DlgY==-1) DlgY=MODE_HEIGHT/2-DlgWMain[3]/2;
	//Barter
	BarterPlayerId=0;
	IfaceLoadRect(BarterWMain,"BarterMain");
	IfaceLoadRect2(BarterBOffer,"BarterOffer",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterBOfferText,"BarterOfferText",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterBTalk,"BarterTalk",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterBTalkText,"BarterTalkText",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterWCont1Pic,"BarterCont1Pic",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterWCont2Pic,"BarterCont2Pic",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterWCont1,"BarterCont1",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterWCont2,"BarterCont2",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterWCont1o,"BarterCont1o",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterWCont2o,"BarterCont2o",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterBCont1ScrUp,"BarterCont1ScrUp",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterBCont2ScrUp,"BarterCont2ScrUp",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterBCont1oScrUp,"BarterCont1oScrUp",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterBCont2oScrUp,"BarterCont2oScrUp",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterBCont1ScrDn,"BarterCont1ScrDn",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterBCont2ScrDn,"BarterCont2ScrDn",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterBCont1oScrDn,"BarterCont1oScrDn",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterBCont2oScrDn,"BarterCont2oScrDn",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterWCost1,"BarterCost1",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterWCost2,"BarterCost2",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterWChosen,"BarterChosen",BarterWMain[0],BarterWMain[1]);
	IfaceLoadRect2(BarterWCritter,"BarterCritter",BarterWMain[0],BarterWMain[1]);
	BarterCont1HeightItem=IfaceIni.GetInt("BarterCont1ItemHeight",30);
	BarterCont2HeightItem=IfaceIni.GetInt("BarterCont2ItemHeight",30);
	BarterCont1oHeightItem=IfaceIni.GetInt("BarterCont1oItemHeight",30);
	BarterCont2oHeightItem=IfaceIni.GetInt("BarterCont2oItemHeight",30);
	BarterHoldId=0;
	BarterCount=0;
	BarterScroll1=0;
	BarterScroll2=0;
	BarterScroll1o=0;
	BarterScroll2o=0;
	BarterK=0;
	BarterIsPlayers=false;
	BarterOpponentHide=false;
	BarterOffer=false;
	BarterOpponentOffer=false;

	// LMenu
	LMenuCurNode=0;
	LMenuActive=false;
	LMenuTryActivated=false;
	LMenuStartTime=0;
	LMenuX=0;
	LMenuY=0;
	LMenuRestoreCurX=0;
	LMenuRestoreCurY=0;
	LMenuSet(LMENU_OFF);
	LMenuNodeHeight=IfaceIni.GetInt("LMenuNodeHeight",40);
	LMenuCritNodes.push_back(LMENU_NODE_LOOK);
	LMenuCritNodes.push_back(LMENU_NODE_BREAK);
	LMenuScenNodes.push_back(LMENU_NODE_BREAK);
	LMenuNodes.push_back(LMENU_NODE_BREAK);

	// Minimap
	LmapX=IfaceIni.GetInt("LmapX",100);
	LmapY=IfaceIni.GetInt("LmapY",100);
	LmapVectX=0;
	LmapVectY=0;
	LmapZoom=2;
	LmapSwitchHi=false;
	LmapPrepareNextTick=0;
	IfaceLoadRect(LmapMain,"LmapMain");
	IfaceLoadRect(LmapWMap,"LmapMap");
	IfaceLoadRect(LmapBOk,"LmapOk");
	IfaceLoadRect(LmapBScan,"LmapScan");
	IfaceLoadRect(LmapBLoHi,"LmapLoHi");

	// Skillbox
	IfaceLoadRect(SboxWMain,"SboxMain");
	IfaceLoadRect(SboxWMainText,"SboxMainText");
	IfaceLoadRect(SboxBCancel,"SboxCancel");
	IfaceLoadRect(SboxBCancelText,"SboxCancelText");
	IfaceLoadRect(SboxBSneak,"SboxSneak");
	IfaceLoadRect(SboxBLockpick,"SboxLockpick");
	IfaceLoadRect(SboxBSteal,"SboxSteal");
	IfaceLoadRect(SboxBTrap,"SboxTrap");
	IfaceLoadRect(SboxBFirstAid,"SboxFirstAid");
	IfaceLoadRect(SboxBDoctor,"SboxDoctor");
	IfaceLoadRect(SboxBScience,"SboxScience");
	IfaceLoadRect(SboxBRepair,"SboxRepair");

	IfaceLoadRect(SboxTSneak,"SboxSneakText");
	IfaceLoadRect(SboxTLockpick,"SboxLockpickText");
	IfaceLoadRect(SboxTSteal,"SboxStealText");
	IfaceLoadRect(SboxTTrap,"SboxTrapText");
	IfaceLoadRect(SboxTFirstAid,"SboxFirstAidText");
	IfaceLoadRect(SboxTDoctor,"SboxDoctorText");
	IfaceLoadRect(SboxTScience,"SboxScienceText");
	IfaceLoadRect(SboxTRepair,"SboxRepairText");

	SboxX=IfaceIni.GetInt("SboxX",-1);
	SboxY=IfaceIni.GetInt("SboxY",-1);
	if(SboxX<0) SboxX=MODE_WIDTH-SboxWMain[2];
	if(SboxY<0) SboxY=(MODE_HEIGHT-SboxWMain[3])/2;
	SboxVectX=0;
	SboxVectY=0;
	CurSkill=0;

	// Menu option
	IfaceLoadRect(MoptMain,"MoptMain");
	MoptX=(MODE_WIDTH-MoptMain.W())/2;
	MoptY=(MODE_HEIGHT-MoptMain.H())/2;
	IfaceLoadRect2(MoptMain,"MoptMain",MoptX,MoptY);
	IfaceLoadRect2(MoptSaveGame,"MoptSaveGame",MoptX,MoptY);
	IfaceLoadRect2(MoptLoadGame,"MoptLoadGame",MoptX,MoptY);
	IfaceLoadRect2(MoptOptions,"MoptOptions",MoptX,MoptY);
	IfaceLoadRect2(MoptExit,"MoptExit",MoptX,MoptY);
	IfaceLoadRect2(MoptResume,"MoptResume",MoptX,MoptY);

	// Character
	// Main
	IfaceLoadRect(ChaWMain,"ChaMain");
	ChaX=(MODE_WIDTH-ChaWMain.W())/2;
	ChaY=(MODE_HEIGHT-ChaWMain.H())/2;
	ChaVectX=0;
	ChaVectY=0;
	IfaceLoadRect(ChaBPrint,"ChaPrint");
	IfaceLoadRect(ChaBPrintText,"ChaPrintText");
	IfaceLoadRect(ChaBOk,"ChaOk");
	IfaceLoadRect(ChaBOkText,"ChaOkText");
	IfaceLoadRect(ChaBCancel,"ChaCancel");
	IfaceLoadRect(ChaBCancelText,"ChaCancelText");
	// Switch
	IfaceLoadRect(ChaBSwitch,"ChaSwitch");
	IfaceLoadRect(ChaTSwitch,"ChaSwitchText");
	IfaceLoadRect(ChaBSwitchScrUp,"ChaSwitchScrUp");
	IfaceLoadRect(ChaBSwitchScrDn,"ChaSwitchScrDn");
	ChaCurSwitch=CHA_SWITCH_PERKS;
	ZeroMemory(ChaSwitchScroll,sizeof(ChaSwitchScroll));
	// Special
	IfaceLoadRect(ChaWSpecialText,"ChaSpecialText");
	IfaceLoadRect(ChaWSpecialValue,"ChaSpecialValue");
	IfaceLoadRect(ChaWSpecialLevel,"ChaSpecialLevel");
	ChaWSpecialNextX=IfaceIni.GetInt("ChaSpecialNextX",1);
	ChaWSpecialNextY=IfaceIni.GetInt("ChaSpecialNextY",1);
	// Skills
	IfaceLoadRect(ChaWSkillText,"ChaSkillText");
	IfaceLoadRect(ChaWSkillName,"ChaSkillName");
	IfaceLoadRect(ChaWSkillValue,"ChaSkillValue");
	IfaceLoadRect(ChaWUnspentSP,"ChaUnspentSP");
	IfaceLoadRect(ChaWUnspentSPText,"ChaUnspentSPText");
	ChaWSkillNextX=IfaceIni.GetInt("ChaSkillNextX",1);
	ChaWSkillNextY=IfaceIni.GetInt("ChaSkillNextY",1);
	ZeroMemory(ChaSkillUp,sizeof(ChaSkillUp));
	ChaUnspentSkillPoints=0;
	// Slider
	IfaceLoadRect(ChaBSliderMinus,"ChaSliderMinus");
	IfaceLoadRect(ChaBSliderPlus,"ChaSliderPlus");
	ChaWSliderX=IfaceIni.GetInt("ChaSliderX",1);
	ChaWSliderY=IfaceIni.GetInt("ChaSliderY",1);
	ChaCurSkill=0;
	// Level
	IfaceLoadRect(ChaWLevel,"ChaLevel");
	IfaceLoadRect(ChaWExp,"ChaExp");
	IfaceLoadRect(ChaWNextLevel,"ChaNextLevel");
	// Damage
	IfaceLoadRect(ChaWDmgLife,"ChaDmgLife");
	IfaceLoadRect(ChaWDmg,"ChaDmg");
	ChaWDmgNextX=IfaceIni.GetInt("ChaDmgNextX",1);
	ChaWDmgNextY=IfaceIni.GetInt("ChaDmgNextY",1);
	// Stats
	IfaceLoadRect(ChaWStatsName,"ChaStatsName");
	IfaceLoadRect(ChaWStatsValue,"ChaStatsValue");
	ChaWStatsNextX=IfaceIni.GetInt("ChaStatsNextX",1);
	ChaWStatsNextY=IfaceIni.GetInt("ChaStatsNextY",1);
	// Tips
	IfaceLoadRect(ChaWName,"ChaParamName");
	IfaceLoadRect(ChaWDesc,"ChaParamDesc");
	IfaceLoadRect(ChaWPic,"ChaParamPic");
	ZeroMemory(ChaName,sizeof(ChaName));
	ZeroMemory(ChaDesc,sizeof(ChaDesc));
	ChaSkilldexPic=-1;
	// Buttons
	IfaceLoadRect(ChaBName,"ChaName");
	IfaceLoadRect(ChaBAge,"ChaAge");
	IfaceLoadRect(ChaBSex,"ChaSex");
	// Registration
	RegNewCr=NULL;
	RegWMain=ChaWMain;
	IfaceLoadRect(RegWMain,"RegMain");
	IfaceLoadRect(RegBSpecialPlus,"RegSpecialPlus");
	IfaceLoadRect(RegBSpecialMinus,"RegSpecialMinus");
	IfaceLoadRect(RegBTagSkill,"RegTagSkill");
	IfaceLoadRect(RegWUnspentSpecial,"RegUnspentSpecial");
	IfaceLoadRect(RegWUnspentSpecialText,"RegUnspentSpecialText");
	RegBSpecialNextX=IfaceIni.GetInt("RegSpecialNextX",1);
	RegBSpecialNextY=IfaceIni.GetInt("RegSpecialNextY",1);
	RegBTagSkillNextX=IfaceIni.GetInt("RegTagSkillNextX",1);
	RegBTagSkillNextY=IfaceIni.GetInt("RegTagSkillNextY",1);
	RegCurSpecial=0;
	RegCurTagSkill=0;
	// Trait
	IfaceLoadRect(RegBTraitL,"RegTraitL");
	IfaceLoadRect(RegBTraitR,"RegTraitR");
	IfaceLoadRect(RegWTraitL,"RegTraitLText");
	IfaceLoadRect(RegWTraitR,"RegTraitRText");
	RegTraitNextX=IfaceIni.GetInt("RegTraitNextX",1);
	RegTraitNextY=IfaceIni.GetInt("RegTraitNextY",1);
	RegTraitNum=0;

	// ChaName
	IfaceLoadRect(ChaNameWMain,"ChaNameMain");
	IfaceLoadRect(ChaNameWName,"ChaNameName");
	IfaceLoadRect(ChaNameWNameText,"ChaNameNameText");
	IfaceLoadRect(ChaNameWPass,"ChaNamePass");
	IfaceLoadRect(ChaNameWPassText,"ChaNamePassText");
	ChaNameX=0;
	ChaNameY=0;

	// ChaAge
	IfaceLoadRect(ChaAgeWMain,"ChaAgeMain");
	IfaceLoadRect(ChaAgeBUp,"ChaAgeUp");
	IfaceLoadRect(ChaAgeBDown,"ChaAgeDown");
	IfaceLoadRect(ChaAgeWAge,"ChaAgeAge");
	ChaAgeX=0;
	ChaAgeY=0;

	// ChaSex
	IfaceLoadRect(ChaSexWMain,"ChaSexMain");
	IfaceLoadRect(ChaSexBMale,"ChaSexMale");
	IfaceLoadRect(ChaSexBFemale,"ChaSexFemale");
	ChaSexX=0;
	ChaSexY=0;

	// Perk
	IfaceLoadRect(PerkWMain,"PerkMain");
	IfaceLoadRect(PerkWText,"PerkText");
	IfaceLoadRect(PerkWPerks,"PerkPerks");
	IfaceLoadRect(PerkWPic,"PerkPic");
	IfaceLoadRect(PerkBScrUp,"PerkScrUp");
	IfaceLoadRect(PerkBScrDn,"PerkScrDn");
	IfaceLoadRect(PerkBOk,"PerkOk");
	IfaceLoadRect(PerkBCancel,"PerkCancel");
	IfaceLoadRect(PerkBOkText,"PerkOkText");
	IfaceLoadRect(PerkBCancelText,"PerkCancelText");
	PerkX=(MODE_WIDTH-PerkWMain.W())/2;
	PerkY=(MODE_HEIGHT-PerkWMain.H())/2;
	PerkNextX=IfaceIni.GetInt("PerkNextX",0);
	PerkNextY=IfaceIni.GetInt("PerkNextY",11);
	PerkVectX=0;
	PerkVectY=0;
	PerkScroll=0;
	PerkCurPerk=-1;

	// Town view
	IfaceLoadRect(TViewWMain,"TViewMain");
	IfaceLoadRect(TViewBBack,"TViewBack");
	IfaceLoadRect(TViewBEnter,"TViewEnter");
	IfaceLoadRect(TViewBContours,"TViewContours");
	TViewX=MODE_WIDTH-TViewWMain.W()-10;
	TViewY=10;
	TViewVectX=0;
	TViewVectY=0;
	TViewShowCountours=false;
	TViewType=TOWN_VIEW_FROM_NONE;
	TViewGmapLocId=0;
	TViewGmapLocEntrance=0;

	// Global map
	if(!IfaceIni.GetStr("GmapTilesPic","",GmapTilesPic)) WriteLog(__FUNCTION__" - <GmapTilesPic> signature not found.\n");
	GmapTilesX=IfaceIni.GetInt("GmapTilesX",0);
	GmapTilesY=IfaceIni.GetInt("GmapTilesY",0);
	GmapPic.resize(GmapTilesX*GmapTilesY);
	GmapFog.Create(GM__MAXZONEX,GM__MAXZONEY,NULL);
	// Relief
	SAFEDEL(GmapRelief);
	if(!IfaceIni.GetStr("GmapReliefMask","",res)) WriteLog(__FUNCTION__" - Global map mask signature<GmapReliefMask> not found.\n");
	else if(!FileMngr.LoadFile(res,PT_MAPS)) WriteLog(__FUNCTION__" - Global map mask file<%s> not found.\n",res);
	else if(FileMngr.GetLEWord()!=0x4D42) WriteLog(__FUNCTION__" - Invalid file format of global map mask<%s>.\n",res);
	else
	{
		FileMngr.SetCurPos(28);
		if(FileMngr.GetLEWord()!=4) WriteLog(__FUNCTION__" - Invalid bit per pixel format of global map mask<%s>.\n",res);
		else
		{
			FileMngr.SetCurPos(18);
			WORD mask_w=FileMngr.GetLEDWord();
			WORD mask_h=FileMngr.GetLEDWord();
			FileMngr.SetCurPos(118);
			GmapRelief=new C4BitMask(mask_w,mask_h,0xF);
			int padd_len=mask_w/2+(mask_w/2)%4;
			for(int x,y=0,w=0;y<mask_h;)
			{
				BYTE b=FileMngr.GetByte();
				x=w*2;
				GmapRelief->Set4Bit(x,y,b>>4);
				GmapRelief->Set4Bit(x+1,y,b&0xF);
				w++;
				if(w>=mask_w/2)
				{
					w=0;
					y++;
					FileMngr.SetCurPos(118+y*padd_len);
				}
			}
		}
	}
	// Other
	IfaceLoadRect(GmapWMain,"GmapMain");
	GmapX=GmapWMain.L;
	GmapY=GmapWMain.T;
	IfaceLoadRect2(GmapWMap,"GmapMap",GmapX,GmapY);
	IfaceLoadRect2(GmapBTown,"GmapTown",GmapX,GmapY);
	IfaceLoadRect2(GmapWName,"GmapName",GmapX,GmapY);
	IfaceLoadRect2(GmapWChat,"GmapMessageBox",GmapX,GmapY);
	IfaceLoadRect2(GmapWPanel,"GmapPanel",GmapX,GmapY);
	IfaceLoadRect2(GmapWCar,"GmapCar",GmapX,GmapY);
	IfaceLoadRect2(GmapWTime,"GmapTime",GmapX,GmapY);
	IfaceLoadRect2(GmapWDayTime,"GmapDayTime",GmapX,GmapY);
	IfaceLoadRect2(GmapBInv,"GmapInv",GmapX,GmapY);
	IfaceLoadRect2(GmapBMenu,"GmapMenu",GmapX,GmapY);
	IfaceLoadRect2(GmapBCha,"GmapCha",GmapX,GmapY);
	IfaceLoadRect2(GmapBPip,"GmapPip",GmapX,GmapY);
	IfaceLoadRect2(GmapBFix,"GmapFix",GmapX,GmapY);
	GmapMapScrX=GmapWMap.W()/2+GmapWMap.L;
	GmapMapScrY=GmapWMap.H()/2+GmapWMap.T;
	IfaceLoadRect2(GmapWLock,"GmapLock",GmapX,GmapY);
	GmapWNameStepX=IfaceIni.GetInt("GmapNameStepX",0);
	GmapWNameStepY=IfaceIni.GetInt("GmapNameStepY",22);
	IfaceLoadRect2(GmapWTabs,"GmapTabs",GmapX,GmapY);
	IfaceLoadRect2(GmapBTabsScrUp,"GmapTabsScrUp",GmapX,GmapY);
	IfaceLoadRect2(GmapBTabsScrDn,"GmapTabsScrDn",GmapX,GmapY);
	IfaceLoadRect(GmapWTab,"GmapTab");
	IfaceLoadRect(GmapWTabLoc,"GmapTabLocImage");
	IfaceLoadRect(GmapBTabLoc,"GmapTabLoc");
	GmapTabNextX=IfaceIni.GetInt("GmapTabNextX",0);
	GmapTabNextY=IfaceIni.GetInt("GmapTabNextY",0);
	GmapNullParams();
	GmapTabsScrX=0;
	GmapTabsScrY=0;
	GmapVectX=0;
	GmapVectY=0;
	GmapMapCutOff.clear();
	SprMngr.PrepareSquare(GmapMapCutOff,FLTRECT(0,0,MODE_WIDTH,GmapWMap.T),D3DCOLOR_XRGB(0,0,0));
	SprMngr.PrepareSquare(GmapMapCutOff,FLTRECT(0,GmapWMap.T,GmapWMap.L,GmapWMap.B),D3DCOLOR_XRGB(0,0,0));
	SprMngr.PrepareSquare(GmapMapCutOff,FLTRECT(GmapWMap.R,GmapWMap.T,MODE_WIDTH,GmapWMap.B),D3DCOLOR_XRGB(0,0,0));
	SprMngr.PrepareSquare(GmapMapCutOff,FLTRECT(0,GmapWMap.B,MODE_WIDTH,MODE_HEIGHT),D3DCOLOR_XRGB(0,0,0));
	GmapNextShowEntrancesTick=0;
	GmapShowEntrancesLocId=0;
	ZeroMemory(GmapShowEntrances,sizeof(GmapShowEntrances));
	GmapPTownInOffsX=IfaceIni.GetInt("GmapTownInOffsX",0);
	GmapPTownInOffsY=IfaceIni.GetInt("GmapTownInOffsY",0);
	GmapPTownViewOffsX=IfaceIni.GetInt("GmapTownViewOffsX",0);
	GmapPTownViewOffsY=IfaceIni.GetInt("GmapTownViewOffsY",0);
	GmapZoom=1.0f;

	// PickUp
	IfaceLoadRect(PupWMain,"PupMain");
	IfaceLoadRect(PupWInfo,"PupInfo");
	IfaceLoadRect(PupWCont1,"PupCont1");
	IfaceLoadRect(PupWCont2,"PupCont2");
	IfaceLoadRect(PupBTakeAll,"PupTakeAll");
	IfaceLoadRect(PupBOk,"PupOk");
	IfaceLoadRect(PupBScrUp1,"PupScrUp1");
	IfaceLoadRect(PupBScrDw1,"PupScrDw1");
	IfaceLoadRect(PupBScrUp2,"PupScrUp2");
	IfaceLoadRect(PupBScrDw2,"PupScrDw2");
	IfaceLoadRect(PupBNextCritLeft,"PupNextCritLeft");
	IfaceLoadRect(PupBNextCritRight,"PupNextCritRight");
	PupX=(MODE_WIDTH-PupWMain.W())/2;
	PupY=(MODE_HEIGHT-PupWMain.H())/2;
	PupHeightItem1=IfaceIni.GetInt("PupHeightCont1",0);
	PupHeightItem2=IfaceIni.GetInt("PupHeightCont2",0);
	PupHoldId=0;
	PupScroll1=0;
	PupScroll2=0;
	PupScrollCrit=0;
	PupVectX=0;
	PupVectY=0;
	PupTransferType=0;
	PupContId=0;
	PupContPid=0;
	PupCount=0;
	PupCont2.clear();
	PupLastPutId=0;

	// Pipboy
	IfaceLoadRect(PipWMain,"PipMain");
	IfaceLoadRect(PipWMonitor,"PipMonitor");
	IfaceLoadRect(PipBStatus,"PipStatus");
	//IfaceLoadRect(PipBGames,"PipGames");
	IfaceLoadRect(PipBAutomaps,"PipAutomaps");
	IfaceLoadRect(PipBArchives,"PipArchives");
	IfaceLoadRect(PipBClose,"PipClose");
	IfaceLoadRect(PipWTime,"PipTime");
	PipX=(MODE_WIDTH-PipWMain.W())/2;
	PipY=(MODE_HEIGHT-PipWMain.H())/2;
	PipVectX=0;
	PipVectY=0;
	PipMode=PIP__NONE;
	ZeroMemory(PipScroll,sizeof(PipScroll));
	PipInfoNum=0;
	ZeroMemory(HoloInfo,sizeof(HoloInfo));
	ScoresNextUploadTick=0;
	ZeroMemory(BestScores,sizeof(BestScores));
	Automaps.clear();
	AutomapWaitPids.clear();
	AutomapReceivedPids.clear();
	AutomapPoints.clear();
	AutomapCurMapPid=0;
	AutomapScrX=0.0f;
	AutomapScrY=0.0f;
	AutomapZoom=1.0f;

	// Aim
	IfaceLoadRect(AimWMain,"AimMain");
	IfaceLoadRect(AimBCancel,"AimCancel");
	IfaceLoadRect(AimWHeadT,"AimHeadText");
	IfaceLoadRect(AimWLArmT,"AimLArmText");
	IfaceLoadRect(AimWRArmT,"AimRArmText");
	IfaceLoadRect(AimWTorsoT,"AimTorsoText");
	IfaceLoadRect(AimWRLegT,"AimRLegText");
	IfaceLoadRect(AimWLLegT,"AimLLegText");
	IfaceLoadRect(AimWEyesT,"AimEyesText");
	IfaceLoadRect(AimWGroinT,"AimGroinText");
	IfaceLoadRect(AimWHeadP,"AimHeadProc");
	IfaceLoadRect(AimWLArmP,"AimLArmProc");
	IfaceLoadRect(AimWRArmP,"AimRArmProc");
	IfaceLoadRect(AimWTorsoP,"AimTorsoProc");
	IfaceLoadRect(AimWRLegP,"AimRLegProc");
	IfaceLoadRect(AimWLLegP,"AimLLegProc");
	IfaceLoadRect(AimWEyesP,"AimEyesProc");
	IfaceLoadRect(AimWGroinP,"AimGroinProc");
	AimPicX=IfaceIni.GetInt("AimPicX",0);
	AimPicY=IfaceIni.GetInt("AimPicY",0);
	AimX=(MODE_WIDTH-AimWMain.W())/2;
	AimY=(MODE_HEIGHT-AimWMain.H())/2;
	AimVectX=0;
	AimVectY=0;
	AimPic=0;
	AimLoadedPics.clear();
	AimTargetId=0;

	// Dialog box
	IfaceLoadRect(DlgboxWTop,"DlgboxTop");
	IfaceLoadRect(DlgboxWMiddle,"DlgboxMiddle");
	IfaceLoadRect(DlgboxWBottom,"DlgboxBottom");
	IfaceLoadRect(DlgboxWText,"DlgboxText");
	IfaceLoadRect(DlgboxBButton,"DlgboxButton");
	IfaceLoadRect(DlgboxBButtonText,"DlgboxButtonText");
	DlgboxX=(MODE_WIDTH-DlgboxWTop.W())/2;
	DlgboxY=(MODE_HEIGHT-DlgboxWTop.H())/2;
	DlgboxVectX=0;
	DlgboxVectY=0;
	FollowRuleId=0;
	DlgboxType=DIALOGBOX_NONE;
	FollowMap=0;
	DlgboxWait=0;
	ZeroMemory(DlgboxText,sizeof(DlgboxText));
	FollowType=0;
	DlgboxButtonsCount=0;
	DlgboxSelectedButton=0;
	FollowType=0;
	FollowRuleId=0;
	FollowMap=0;
	PBarterPlayerId=0;
	PBarterHide=false;

	// Elevator
	ElevatorMainPic=0;
	ElevatorExtPic=0;
	ElevatorIndicatorAnim=0;
	ElevatorButtonPicDown=0;
	ElevatorButtonsCount=0;
	ElevatorType=0;
	ElevatorLevelsCount=0;
	ElevatorStartLevel=0;
	ElevatorCurrentLevel=0;
	ElevatorX=0;
	ElevatorY=0;
	ElevatorVectX=0;
	ElevatorVectY=0;
	ElevatorSelectedButton=-1;
	ElevatorAnswerDone=false;
	ElevatorSendAnswerTick=0;

	// Say box
	IfaceLoadRect(SayWMain,"SayMain");
	IfaceLoadRect(SayWMainText,"SayMainText");
	IfaceLoadRect(SayWSay,"SaySay");
	IfaceLoadRect(SayBOk,"SayOk");
	IfaceLoadRect(SayBOkText,"SayOkText");
	IfaceLoadRect(SayBCancel,"SayCancel");
	IfaceLoadRect(SayBCancelText,"SayCancelText");
	SayX=(MODE_WIDTH-SayWMain.W())/2;
	SayY=(MODE_HEIGHT-SayWMain.H())/2;
	SayVectX=0;
	SayVectY=0;
	SayType=DIALOGSAY_NONE;
	ZeroMemory(SayText,sizeof(SayText));

	// Split
	IfaceLoadRect(SplitWMain,"SplitMain");
	IfaceLoadRect(SplitWTitle,"SplitTitle");
	IfaceLoadRect(SplitWItem,"SplitItem");
	IfaceLoadRect(SplitBUp,"SplitUp");
	IfaceLoadRect(SplitBDown,"SplitDown");
	IfaceLoadRect(SplitBAll,"SplitAll");
	IfaceLoadRect(SplitWValue,"SplitValue");
	IfaceLoadRect(SplitBDone,"SplitDone");
	IfaceLoadRect(SplitBCancel,"SplitCancel");
	SplitVectX=0;
	SplitVectY=0;
	SplitItemId=0;
	SplitCont=0;
	SplitValue=0;
	SplitMinValue=0;
	SplitMaxValue=0;
	SplitValueKeyPressed=false;
	SplitX=(MODE_WIDTH-SplitWMain.W())/2;
	SplitY=(MODE_HEIGHT-SplitWMain.H())/2;
	SplitItemPic=0;
	SplitItemColor=0;
	SplitParentScreen=SCREEN_NONE;

	// Timer
	IfaceLoadRect(TimerWMain,"TimerMain");
	IfaceLoadRect(TimerWTitle,"TimerTitle");
	IfaceLoadRect(TimerWItem,"TimerItem");
	IfaceLoadRect(TimerBUp,"TimerUp");
	IfaceLoadRect(TimerBDown,"TimerDown");
	IfaceLoadRect(TimerWValue,"TimerValue");
	IfaceLoadRect(TimerBDone,"TimerDone");
	IfaceLoadRect(TimerBCancel,"TimerCancel");
	TimerVectX=0;
	TimerVectY=0;
	TimerItemId=0;
	TimerValue=0;
	TimerX=(MODE_WIDTH-TimerWMain.W())/2;
	TimerY=(MODE_HEIGHT-TimerWMain.H())/2;
	TimerItemPic=0;
	TimerItemColor=0;

	// FixBoy
	IfaceLoadRect(FixWMain,"FixMain");
	IfaceLoadRect(FixBScrUp,"FixScrUp");
	IfaceLoadRect(FixBScrDn,"FixScrDn");
	IfaceLoadRect(FixBDone,"FixDone");
	IfaceLoadRect(FixWWin,"FixWin");
	IfaceLoadRect(FixBFix,"FixFix");
	FixVectX=0;
	FixVectY=0;
	FixX=(MODE_WIDTH-FixWMain.W())/2;
	FixY=(MODE_HEIGHT-FixWMain.H())/2;
	FixMode=FIX_MODE_LIST;
	FixCurCraft=-1;
	FixScrollLst=0;
	FixScrollFix=0;
	FixResult=0;
	FixResultStr="";
	FixShowCraft.clear();
	FixNextShowCraftTick=0;

	// Input box
	IfaceLoadRect(IboxWMain,"IboxMain");
	IfaceLoadRect(IboxWTitle,"IboxTitle");
	IfaceLoadRect(IboxWText,"IboxText");
	IfaceLoadRect(IboxBDone,"IboxDone");
	IfaceLoadRect(IboxBDoneText,"IboxDoneText");
	IfaceLoadRect(IboxBCancel,"IboxCancel");
	IfaceLoadRect(IboxBCancelText,"IboxCancelText");
	IboxMode=IBOX_MODE_NONE;
	IboxX=(MODE_WIDTH-IboxWMain.W())/2;
	IboxY=(MODE_HEIGHT-IboxWMain.H())/2;
	IboxVectX=0;
	IboxVectY=0;
	IboxTitle="";
	IboxText="";
	IboxTitleCur=0;
	IboxTextCur=0;
	IboxLastKey=0;
	IboxHolodiskId=0;

	// Save/Load
	IfaceLoadRect(SaveLoadMain,"SaveLoadMain");
	IfaceLoadRect(SaveLoadText,"SaveLoadText");
	IfaceLoadRect(SaveLoadScrUp,"SaveLoadScrUp");
	IfaceLoadRect(SaveLoadScrDown,"SaveLoadScrDown");
	IfaceLoadRect(SaveLoadSlots,"SaveLoadSlots");
	IfaceLoadRect(SaveLoadPic,"SaveLoadPic");
	IfaceLoadRect(SaveLoadInfo,"SaveLoadInfo");
	IfaceLoadRect(SaveLoadDone,"SaveLoadDone");
	IfaceLoadRect(SaveLoadDoneText,"SaveLoadDoneText");
	IfaceLoadRect(SaveLoadBack,"SaveLoadBack");
	IfaceLoadRect(SaveLoadBackText,"SaveLoadBackText");
	SaveLoadCX=(MODE_WIDTH-SaveLoadMain.W())/2;
	SaveLoadCY=(MODE_HEIGHT-SaveLoadMain.H())/2;
	SaveLoadX=SaveLoadCX;
	SaveLoadY=SaveLoadCY;
	SaveLoadVectX=0;
	SaveLoadVectY=0;
	SaveLoadLoginScreen=false;
	SaveLoadSave=false;
	SaveLoadClickSlotTick=0;
	SaveLoadClickSlotIndex=0;
	SaveLoadSlotIndex=0;
	SaveLoadSlotScroll=0;
	SaveLoadSlotsMax=0;
	SaveLoadDataSlots.clear();
	// Save/load surface creating
	if(!SaveLoadDraft && Singleplayer)
	{
		if(FAILED(SprMngr.GetDevice()->CreateRenderTarget(SAVE_LOAD_IMAGE_WIDTH,SAVE_LOAD_IMAGE_HEIGHT,
			D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&SaveLoadDraft,NULL))) WriteLog("Create save/load draft surface fail.\n");
	}
	SaveLoadProcessDraft=false;
	SaveLoadDraftValid=false;

/************************************************************************/
/* Sprites                                                              */
/************************************************************************/
	WriteLog("Load sprites.\n");
	IfaceFreeResources();

	// Hex field sprites
	SprMngr.SurfType=RES_IFACE;
	if(!HexMngr.ReloadSprites(&SprMngr))
	{
		WriteLog(__FUNCTION__" - Unable to reload hex field sprites.\n");
		SprMngr.SurfType=RES_NONE;
		return __LINE__;
	}
	SprMngr.SurfType=RES_NONE;

	// Interface
	IfaceLoadSpr(IntPWAddMess,"IntAddMessWindowPic");
	IfaceLoadSpr(IntPBAddMessDn,"IntAddMessPicDn");
	IfaceLoadSpr(IntPBMessFilter1Dn,"IntMessFilter1PicDn");
	IfaceLoadSpr(IntPBMessFilter2Dn,"IntMessFilter2PicDn");
	IfaceLoadSpr(IntPBMessFilter3Dn,"IntMessFilter3PicDn");
	IfaceLoadSpr(IntDiodeG,"IntApGreenPic");
	IfaceLoadSpr(IntDiodeY,"IntApYellowPic");
	IfaceLoadSpr(IntDiodeR,"IntApRedPic");
	IfaceLoadSpr(IntBreakTimePic,"IntBreakTimePic");
	IfaceLoadSpr(IntPBScrUpDn,"IntScrUpPicDn");
	IfaceLoadSpr(IntPBScrDnDn,"IntScrDownPicDn");
	IfaceLoadSpr(IntPBSlotsDn,"IntChangeSlotPicDn");
	IfaceLoadSpr(IntPBInvDn,"IntInvPicDn");
	IfaceLoadSpr(IntPBMenuDn,"IntMenuPicDn");
	IfaceLoadSpr(IntPBSkillDn,"IntSkillPicDn");
	IfaceLoadSpr(IntPBMapDn,"IntMapPicDn");
	IfaceLoadSpr(IntPBChaDn,"IntChaPicDn");
	IfaceLoadSpr(IntPBPipDn,"IntPipPicDn");
	IfaceLoadSpr(IntPBFixDn,"IntFixPicDn");
	IfaceLoadSpr(IntMessTabPicNone,"IntMessTabPic");
	IfaceLoadSpr(IntBItemPicDn,"IntItemPicDn");
	IfaceLoadSpr(IntAimPic,"IntAimPic");
	IfaceLoadSpr(IntWApCostPicNone,"IntApCostPic");
	IfaceLoadAnim(IntWCombatAnim,"IntCombatAnim");
	IfaceLoadSpr(IntBCombatTurnPicDown,"IntCombatTurnPicDn");
	IfaceLoadSpr(IntBCombatEndPicDown,"IntCombatEndPicDn");
	IfaceLoadSpr(IntMainPic,"IntMainPic");

	// Console
	IfaceLoadSpr(ConsolePic,"ConsoleMainPic");

	// Default animations
	SprMngr.SurfType=RES_IFACE;
	ItemHex::DefaultAnim=ResMngr.GetAnim(Str::GetHash("art\\items\\reserved.frm"),0);
	if(!ItemHex::DefaultAnim)
	{
		WriteLog(__FUNCTION__" - Default item animation not found.\n");
		return __LINE__;
	}

	SprMngr.SurfType=RES_IFACE;
	CritterCl::DefaultAnim=SprMngr.LoadAnyAnimation("reservaa.frm",PT_ART_CRITTERS,true,0);
	SprMngr.SurfType=RES_NONE;
	if(!CritterCl::DefaultAnim)
	{
		WriteLog(__FUNCTION__" - Default critter animation not found.\n");
		return __LINE__;
	}

	// Inventory
	IfaceLoadSpr(InvPWMain,"InvMainPic");
	IfaceLoadSpr(InvPBOkUp,"InvOkPic");
	IfaceLoadSpr(InvPBOkDw,"InvOkPicDn");
	IfaceLoadSpr(InvPBScrUpUp,"InvScrUpPic");
	IfaceLoadSpr(InvPBScrUpDw,"InvScrUpPicDn");
	IfaceLoadSpr(InvPBScrUpOff,"InvScrUpPicNa");
	IfaceLoadSpr(InvPBScrDwUp,"InvScrDnPic");
	IfaceLoadSpr(InvPBScrDwDw,"InvScrDnPicDn");
	IfaceLoadSpr(InvPBScrDwOff,"InvScrDnPicNa");

	// Use
	IfaceLoadSpr(UseWMainPicNone,"UseMainPic");
	IfaceLoadSpr(UseBCancelPicDown,"UseCancelPicDn");
	IfaceLoadSpr(UseBScrUpPicDown,"UseScrUpPicDn");
	IfaceLoadSpr(UseBScrUpPicUp,"UseScrUpPic");
	IfaceLoadSpr(UseBScrUpPicOff,"UseScrUpPicOff");
	IfaceLoadSpr(UseBScrDownPicDown,"UseScrDownPicDn");
	IfaceLoadSpr(UseBScrDownPicUp,"UseScrDownPic");
	IfaceLoadSpr(UseBScrDownPicOff,"UseScrDownPicOff");

	// Login/Password
	IfaceLoadSpr(LogPMain,"LogMainPic");
	IfaceLoadSpr(LogPSingleplayerMain,"LogSingleplayerMainPic");
	if(!LogPSingleplayerMain) LogPSingleplayerMain=LogPMain;
	IfaceLoadSpr(LogPBLogin,"LogPlayPicDn");
	IfaceLoadSpr(LogPBReg,"LogRegPicDn");
	IfaceLoadSpr(LogPBOptions,"LogOptionsPicDn");
	IfaceLoadSpr(LogPBCredits,"LogCreditsPicDn");
	IfaceLoadSpr(LogPBExit,"LogExitPicDn");

	// Dialog
	IfaceLoadSpr(DlgPMain,"DlgMainPic");
	IfaceLoadSpr(DlgPAnsw,"DlgAnswPic");
	IfaceLoadSpr(DlgPBBarter,"DlgBarterPicDn");
	IfaceLoadSpr(DlgPBSay,"DlgSayPicDn");
	DlgAvatarSprId=0;
	// Barter
	IfaceLoadSpr(BarterPMain,"BarterMainPic");
	IfaceLoadSpr(BarterPBOfferDn,"BarterOfferPic");
	IfaceLoadSpr(BarterPBTalkDn,"BarterTalkPic");
	IfaceLoadSpr(BarterPBC1ScrUpDn,"BarterCont1ScrUpPicDn");
	IfaceLoadSpr(BarterPBC2ScrUpDn,"BarterCont2ScrUpPicDn");
	IfaceLoadSpr(BarterPBC1oScrUpDn,"BarterCont1oScrUpPicDn");
	IfaceLoadSpr(BarterPBC2oScrUpDn,"BarterCont2oScrUpPicDn");
	IfaceLoadSpr(BarterPBC1ScrDnDn,"BarterCont1ScrDnPicDn");
	IfaceLoadSpr(BarterPBC2ScrDnDn,"BarterCont2ScrDnPicDn");
	IfaceLoadSpr(BarterPBC1oScrDnDn,"BarterCont1oScrDnPicDn");
	IfaceLoadSpr(BarterPBC2oScrDnDn,"BarterCont2oScrDnPicDn");

	// Cursors
	SprMngr.SurfType=RES_IFACE;
	CurPMove=SprMngr.LoadSprite("msef001.frm",PT_ART_INTRFACE);
	CurPMoveBlock=SprMngr.LoadSprite("msef002.frm",PT_ART_INTRFACE);
	CurPUseItem=SprMngr.LoadSprite("acttohit.frm",PT_ART_INTRFACE);
	CurPUseSkill=SprMngr.LoadSprite("crossuse.frm",PT_ART_INTRFACE);
	CurPWait=SprMngr.LoadSprite("wait2.frm",PT_ART_INTRFACE);
	CurPHand=SprMngr.LoadSprite("hand.frm",PT_ART_INTRFACE);
	CurPDef=SprMngr.LoadSprite("actarrow.frm",PT_ART_INTRFACE);
	CurPScrRt=SprMngr.LoadSprite("screast.frm",PT_ART_INTRFACE);
	CurPScrLt=SprMngr.LoadSprite("scrwest.frm",PT_ART_INTRFACE);
	CurPScrUp=SprMngr.LoadSprite("scrnorth.frm",PT_ART_INTRFACE);
	CurPScrDw=SprMngr.LoadSprite("scrsouth.frm",PT_ART_INTRFACE);
	CurPScrRU=SprMngr.LoadSprite("scrneast.frm",PT_ART_INTRFACE);
	CurPScrLU=SprMngr.LoadSprite("scrnwest.frm",PT_ART_INTRFACE);
	CurPScrRD=SprMngr.LoadSprite("scrseast.frm",PT_ART_INTRFACE);
	CurPScrLD=SprMngr.LoadSprite("scrswest.frm",PT_ART_INTRFACE);
	SprMngr.SurfType=RES_NONE;
	if(!CurPMove) return __LINE__;
	if(!CurPMoveBlock) return __LINE__;
	if(!CurPUseItem) return __LINE__;
	if(!CurPUseSkill) return __LINE__;
	if(!CurPWait) return __LINE__;
	if(!CurPHand) return __LINE__;
	if(!CurPDef) return __LINE__;
	if(!CurPScrRt) return __LINE__;
	if(!CurPScrLt) return __LINE__;
	if(!CurPScrUp) return __LINE__;
	if(!CurPScrDw) return __LINE__;
	if(!CurPScrRU) return __LINE__;
	if(!CurPScrLU) return __LINE__;
	if(!CurPScrRD) return __LINE__;
	if(!CurPScrLD) return __LINE__;

	// LMenu
	IfaceLoadSpr(LmenuPTalkOff,"LMenuTalkPic");
	IfaceLoadSpr(LmenuPTalkOn,"LMenuTalkPicDn");
	IfaceLoadSpr(LmenuPLookOff,"LMenuLookPic");
	IfaceLoadSpr(LmenuPLookOn,"LMenuLookPicDn");
	IfaceLoadSpr(LmenuPBreakOff,"LMenuCancelPic");
	IfaceLoadSpr(LmenuPBreakOn,"LMenuCancelPicDn");
	IfaceLoadSpr(LmenuPUseOff,"LMenuUsePic");
	IfaceLoadSpr(LmenuPUseOn,"LMenuUsePicDn");
	IfaceLoadSpr(LmenuPGMFollowOff,"LMenuGMFollowPic");
	IfaceLoadSpr(LmenuPGMFollowOn,"LMenuGMFollowPicDn");
	IfaceLoadSpr(LmenuPRotateOff,"LMenuRotatePic");
	IfaceLoadSpr(LmenuPRotateOn,"LMenuRotatePicDn");
	IfaceLoadSpr(LmenuPDropOff,"LMenuDropPic");
	IfaceLoadSpr(LmenuPDropOn,"LMenuDropPicDn");
	IfaceLoadSpr(LmenuPUnloadOff,"LMenuUnloadPic");
	IfaceLoadSpr(LmenuPUnloadOn,"LMenuUnloadPicDn");
	IfaceLoadSpr(LmenuPSortUpOff,"LMenuSortUpPic");
	IfaceLoadSpr(LmenuPSortUpOn,"LMenuSortUpPicDn");
	IfaceLoadSpr(LmenuPSortDownOff,"LMenuSortDownPic");
	IfaceLoadSpr(LmenuPSortDownOn,"LMenuSortDownPicDn");
	IfaceLoadSpr(LmenuPPickItemOff,"LMenuPickItemPic");
	IfaceLoadSpr(LmenuPPickItemOn,"LMenuPickItemPicDn");
	IfaceLoadSpr(LmenuPPushOff,"LMenuPushPic");
	IfaceLoadSpr(LmenuPPushOn,"LMenuPushPicDn");
	IfaceLoadSpr(LmenuPBagOff,"LMenuBagPic");
	IfaceLoadSpr(LmenuPBagOn,"LMenuBagPicDn");
	IfaceLoadSpr(LmenuPSkillOff,"LMenuSkillPic");
	IfaceLoadSpr(LmenuPSkillOn,"LMenuSkillPicDn");
	IfaceLoadSpr(LmenuPBarterOpenOff,"LMenuBarterOpenPic");
	IfaceLoadSpr(LmenuPBarterOpenOn,"LMenuBarterOpenPicDn");
	IfaceLoadSpr(LmenuPBarterHideOff,"LMenuBarterHidePic");
	IfaceLoadSpr(LmenuPBarterHideOn,"LMenuBarterHidePicDn");
	IfaceLoadSpr(LmenuPGmapKickOff,"LMenuGmapKickPic");
	IfaceLoadSpr(LmenuPGmapKickOn,"LMenuGmapKickPicDn");
	IfaceLoadSpr(LmenuPGmapRuleOff,"LMenuGmapRulePic");
	IfaceLoadSpr(LmenuPGmapRuleOn,"LMenuGmapRulePicDn");
	IfaceLoadSpr(LmenuPVoteUpOff,"LMenuVoteUpPic");
	IfaceLoadSpr(LmenuPVoteUpOn,"LMenuVoteUpPicDn");
	IfaceLoadSpr(LmenuPVoteDownOff,"LMenuVoteDownPic");
	IfaceLoadSpr(LmenuPVoteDownOn,"LMenuVoteDownPicDn");

	// MiniMap
	IfaceLoadSpr(LmapPMain,"LmapMainPic");
	IfaceLoadSpr(LmapPBOkDw,"LmapOkPicDn");
	IfaceLoadSpr(LmapPBScanDw,"LmapScanPicDn");
	IfaceLoadSpr(LmapPBLoHiDw,"LmapLoHiPicDn");
	SprMngr.SurfType=RES_IFACE;
	LmapPPix=SprMngr.LoadSprite("green_pix.png",PT_ART_INTRFACE);
	SprMngr.SurfType=RES_NONE;
	if(!LmapPPix) return __LINE__;

	// Skillbox
	IfaceLoadSpr(SboxPMain,"SboxMainPic");
	IfaceLoadSpr(SboxPBCancelDn,"SboxCancelPicDn");
	IfaceLoadSpr(SboxPBSneakDn,"SboxSneakPicDn");
	IfaceLoadSpr(SboxPBLockPickDn,"SboxLockpickPicDn");
	IfaceLoadSpr(SboxPBStealDn,"SboxStealPicDn");
	IfaceLoadSpr(SboxPBTrapsDn,"SboxTrapPicDn");
	IfaceLoadSpr(SboxPBFirstaidDn,"SboxFirstAidPicDn");
	IfaceLoadSpr(SboxPBDoctorDn,"SboxDoctorPicDn");
	IfaceLoadSpr(SboxPBScienceDn,"SboxSciencePicDn");
	IfaceLoadSpr(SboxPBRepairDn,"SboxRepairPicDn");

	// Menu option
	IfaceLoadSpr(MoptMainPic,"MoptMainPic");
	IfaceLoadSpr(MoptSingleplayerMainPic,"MoptSingleplayerMainPic");
	if(!MoptSingleplayerMainPic) MoptSingleplayerMainPic=MoptMainPic;
	IfaceLoadSpr(MoptSaveGamePicDown,"MoptSaveGamePicDn");
	IfaceLoadSpr(MoptLoadGamePicDown,"MoptLoadGamePicDn");
	IfaceLoadSpr(MoptOptionsPicDown,"MoptOptionsPicDn");
	IfaceLoadSpr(MoptExitPicDown,"MoptExitPicDn");
	IfaceLoadSpr(MoptResumePicDown,"MoptResumePicDn");

	// Character
	IfaceLoadSpr(ChaPMain,"ChaMainPic");
	IfaceLoadSpr(ChaPBPrintDn,"ChaPrintPicDn");
	IfaceLoadSpr(ChaPBOkDn,"ChaOkPicDn");
	IfaceLoadSpr(ChaPBCancelDn,"ChaCancelPicDn");
	IfaceLoadSpr(ChaPWSlider,"ChaSliderPic");
	IfaceLoadSpr(ChaPBSliderPlusDn,"ChaSliderPlusPicDn");
	IfaceLoadSpr(ChaPBSliderMinusDn,"ChaSliderMinusPicDn");

	// Switch
	IfaceLoadSpr(ChaPBSwitchPerks,"ChaSwitchPerksPic");
	IfaceLoadSpr(ChaPBSwitchKarma,"ChaSwitchKarmaPic");
	IfaceLoadSpr(ChaPBSwitchKills,"ChaSwitchKillsPic");
	IfaceLoadSpr(ChaPBSwitchMask,"ChaSwitchMaskPic");
	IfaceLoadSpr(ChaPBSwitchScrUpUp,"ChaSwitchScrUpPic");
	IfaceLoadSpr(ChaPBSwitchScrUpDn,"ChaSwitchScrUpPicDn");
	IfaceLoadSpr(ChaPBSwitchScrDnUp,"ChaSwitchScrDnPic");
	IfaceLoadSpr(ChaPBSwitchScrDnDn,"ChaSwitchScrDnPicDn");

	// Registration
	IfaceLoadSpr(RegPMain,"RegMainPic");
	IfaceLoadSpr(RegPBSpecialPlusDn,"RegSpecialPlusPicDn");
	IfaceLoadSpr(RegPBSpecialMinusDn,"RegSpecialMinusPicDn");
	IfaceLoadSpr(RegPBTagSkillDn,"RegTagSkillPicDn");

	// Traits
	IfaceLoadSpr(RegPBTraitDn,"RegTraitPicDn");

	// Buttons
	IfaceLoadSpr(ChaPBNameDn,"ChaNamePicDn");
	IfaceLoadSpr(ChaPBAgeDn,"ChaAgePicDn");
	IfaceLoadSpr(ChaPBSexDn,"ChaSexPicDn");

	// ChaName
	IfaceLoadSpr(ChaNameMainPic,"ChaNameMainPic");
	IfaceLoadSpr(ChaNameSingleplayerMainPic,"ChaNameSingleplayerMainPic");
	if(!ChaNameSingleplayerMainPic) ChaNameSingleplayerMainPic=ChaNameMainPic;

	// ChaAge
	IfaceLoadSpr(ChaAgePic,"ChaAgeMainPic");
	IfaceLoadSpr(ChaAgeBUpDn,"ChaAgeUpPicDn");
	IfaceLoadSpr(ChaAgeBDownDn,"ChaAgeDownPicDn");

	// ChaSex
	IfaceLoadSpr(ChaSexPic,"ChaSexMainPic");
	IfaceLoadSpr(ChaSexBMaleDn,"ChaSexMalePicDn");
	IfaceLoadSpr(ChaSexBFemaleDn,"ChaSexFemalePicDn");

	// Perk
	IfaceLoadSpr(PerkPMain,"PerkMainPic");
	IfaceLoadSpr(PerkPBScrUpDn,"PerkScrUpPic");
	IfaceLoadSpr(PerkPBScrDnDn,"PerkScrDnPic");
	IfaceLoadSpr(PerkPBOkDn,"PerkOkPic");
	IfaceLoadSpr(PerkPBCancelDn,"PerkCancelPic");

	// Town view
	IfaceLoadSpr(TViewWMainPic,"TViewMainPic");
	IfaceLoadSpr(TViewBBackPicDn,"TViewBackPicDn");
	IfaceLoadSpr(TViewBEnterPicDn,"TViewEnterPicDn");
	IfaceLoadSpr(TViewBContoursPicDn,"TViewContoursPicDn");

	// Global map
	IfaceLoadSpr(GmapWMainPic,"GmapMainPic");
	IfaceLoadSpr(GmapPBTownDw,"GmapTownPicDn");
	IfaceLoadSpr(GmapPGr,"GmapGroupLocPic");
	IfaceLoadSpr(GmapPTarg,"GmapGroupTargPic");
	IfaceLoadSpr(GmapPStay,"GmapStayPic");
	IfaceLoadSpr(GmapPStayDn,"GmapStayPicDn");
	IfaceLoadSpr(GmapPStayMask,"GmapStayPicMask");
	IfaceLoadSpr(GmapPTownInPic,"GmapTownInPic");
	IfaceLoadSpr(GmapPTownInPicDn,"GmapTownInPicDn");
	IfaceLoadSpr(GmapPTownInPicMask,"GmapTownInPicMask");
	IfaceLoadSpr(GmapPTownViewPic,"GmapTownViewPic");
	IfaceLoadSpr(GmapPTownViewPicDn,"GmapTownViewPicDn");
	IfaceLoadSpr(GmapPTownViewPicMask,"GmapTownViewPicMask");
	IfaceLoadSpr(GmapLocPic,"GmapLocPic");
	IfaceLoadSpr(GmapPFollowCrit,"GmapFollowCritPic");
	IfaceLoadSpr(GmapPFollowCritSelf,"GmapFollowCritSelfPic");
	IfaceLoadSpr(GmapPWTab,"GmapTabPic");
	IfaceLoadSpr(GmapPWBlankTab,"GmapBlankTabPic");
	IfaceLoadSpr(GmapPBTabLoc,"GmapTabLocPicDn");
	IfaceLoadSpr(GmapPTabScrUpDw,"GmapTabsScrUpPicDn");
	IfaceLoadSpr(GmapPTabScrDwDw,"GmapTabsScrDnPicDn");
	IfaceLoadAnim(GmapWDayTimeAnim,"GmapDayTimeAnim");
	IfaceLoadSpr(GmapBInvPicDown,"GmapInvPicDn");
	IfaceLoadSpr(GmapBMenuPicDown,"GmapMenuPicDn");
	IfaceLoadSpr(GmapBChaPicDown,"GmapChaPicDn");
	IfaceLoadSpr(GmapBPipPicDown,"GmapPipPicDn");
	IfaceLoadSpr(GmapBFixPicDown,"GmapFixPicDn");
	IfaceLoadSpr(GmapPLightPic0,"GmapLightPic0");
	IfaceLoadSpr(GmapPLightPic1,"GmapLightPic1");

	// PickUp
	IfaceLoadSpr(PupPMain,"PupMainPic");
	IfaceLoadSpr(PupPTakeAllOn,"PupTAPicDn");
	IfaceLoadSpr(PupPBOkOn,"PupOkPicDn");
	IfaceLoadSpr(PupPBScrUpOn1,"PupScrUp1PicDn");
	IfaceLoadSpr(PupPBScrUpOff1,"PupScrUp1PicOff");
	IfaceLoadSpr(PupPBScrDwOn1,"PupScrDw1PicDn");
	IfaceLoadSpr(PupPBScrDwOff1,"PupScrDw1PicOff");
	IfaceLoadSpr(PupPBScrUpOn2,"PupScrUp2PicDn");
	IfaceLoadSpr(PupPBScrUpOff2,"PupScrUp2PicOff");
	IfaceLoadSpr(PupPBScrDwOn2,"PupScrDw2PicDn");
	IfaceLoadSpr(PupPBScrDwOff2,"PupScrDw2PicOff");
	IfaceLoadSpr(PupBNextCritLeftPicUp,"PupNextCritLeftPic");
	IfaceLoadSpr(PupBNextCritLeftPicDown,"PupNextCritLeftPicDn");
	IfaceLoadSpr(PupBNextCritRightPicUp,"PupNextCritRightPic");
	IfaceLoadSpr(PupBNextCritRightPicDown,"PupNextCritRightPicDn");

	// Pipboy
	IfaceLoadSpr(PipPMain,"PipMainPic");
	IfaceLoadSpr(PipPWMonitor,"PipMonitorPic");
	IfaceLoadSpr(PipPBStatusDn,"PipStatusPicDn");
	//IfaceLoadSpr(PipPBGamesDn,"PipGamesPicDn");
	IfaceLoadSpr(PipPBAutomapsDn,"PipAutomapsPicDn");
	IfaceLoadSpr(PipPBArchivesDn,"PipArchivesPicDn");
	IfaceLoadSpr(PipPBCloseDn,"PipClosePicDn");

	// Aim
	IfaceLoadSpr(AimPWMain,"AimMainPic");
	IfaceLoadSpr(AimPBCancelDn,"AimCancelPicDn");

	// Dialog box
	IfaceLoadSpr(DlgboxWTopPicNone,"DlgboxTopPic");
	IfaceLoadSpr(DlgboxWMiddlePicNone,"DlgboxMiddlePic");
	IfaceLoadSpr(DlgboxWBottomPicNone,"DlgboxBottomPic");
	IfaceLoadSpr(DlgboxBButtonPicDown,"DlgboxButtonPicDn");

	// Say box
	IfaceLoadSpr(SayWMainPicNone,"SayMainPic");
	IfaceLoadSpr(SayBOkPicDown,"SayOkPicDn");
	IfaceLoadSpr(SayBCancelPicDown,"SayCancelPicDn");

	// Split
	IfaceLoadSpr(SplitMainPic,"SplitMainPic");
	IfaceLoadSpr(SplitPBUpDn,"SplitUpPicDn");
	IfaceLoadSpr(SplitPBDnDn,"SplitDownPicDn");
	IfaceLoadSpr(SplitPBAllDn,"SplitAllPicDn");
	IfaceLoadSpr(SplitPBDoneDn,"SplitDonePic");
	IfaceLoadSpr(SplitPBCancelDn,"SplitCancelPic");

	// Timer
	IfaceLoadSpr(TimerMainPic,"TimerMainPic");
	IfaceLoadSpr(TimerBUpPicDown,"TimerUpPicDn");
	IfaceLoadSpr(TimerBDownPicDown,"TimerDownPicDn");
	IfaceLoadSpr(TimerBDonePicDown,"TimerDonePicDn");
	IfaceLoadSpr(TimerBCancelPicDown,"TimerCancelPicDn");

	// FixBoy
	IfaceLoadSpr(FixMainPic,"FixMainPic");
	IfaceLoadSpr(FixPBScrUpDn,"FixScrUpPicDn");
	IfaceLoadSpr(FixPBScrDnDn,"FixScrDnPicDn");
	IfaceLoadSpr(FixPBDoneDn,"FixDonePicDn");
	IfaceLoadSpr(FixPBFixDn,"FixFixPicDn");

	// Input box
	IfaceLoadSpr(IboxWMainPicNone,"IboxMainPic");
	IfaceLoadSpr(IboxBDonePicDown,"IboxDonePicDn");
	IfaceLoadSpr(IboxBCancelPicDown,"IboxCancelPicDn");

	// Save/Load
	IfaceLoadSpr(SaveLoadMainPic,"SaveLoadMainPic");
	IfaceLoadSpr(SaveLoadScrUpPicDown,"SaveLoadScrUpPicDn");
	IfaceLoadSpr(SaveLoadScrDownPicDown,"SaveLoadScrDownPicDn");
	IfaceLoadSpr(SaveLoadDonePicDown,"SaveLoadDonePicDn");
	IfaceLoadSpr(SaveLoadBackPicDown,"SaveLoadBackPicDn");

	WriteLog("Interface initialization complete.\n");
	return 0;
}

#define INDICATOR_CHANGE_TICK        (35)
void FOClient::DrawIndicator(INTRECT& rect, PointVec& points, DWORD color, int procent, DWORD& tick, bool is_vertical, bool from_top_or_left)
{
	if(Timer::GameTick()>=tick)
	{
		int points_count=(is_vertical?rect.H():rect.W())/2*procent/100;
		if(!points_count && procent) points_count=1;
		if(points.size()!=points_count)
		{
			if(points_count>points.size()) points_count=points.size()+1;
			else points_count=points.size()-1;
			points.resize(points_count);
			for(int i=0;i<points_count;i++)
			{
				if(is_vertical)
				{
					if(from_top_or_left) points[i]=PrepPoint(rect[0],rect[1]+i*2,color,NULL,NULL);
					else points[i]=PrepPoint(rect[0],rect[3]-i*2,color,NULL,NULL);
				}
				else
				{
					if(from_top_or_left) points[i]=PrepPoint(rect[0]+i*2,rect[1],color,NULL,NULL);
					else points[i]=PrepPoint(rect[2]-i*2,rect[1],color,NULL,NULL);
				}
			}
		}
		tick=Timer::GameTick()+INDICATOR_CHANGE_TICK;
	}
	if(points.size()>0) SprMngr.DrawPoints(points,D3DPT_POINTLIST);
}

DWORD FOClient::GetCurContainerItemId(INTRECT& pos, int height, int scroll, ItemVec& cont)
{
	if(!IsCurInRect(pos)) return 0;
	ItemVecIt it=cont.begin();
	int pos_cur=(CurY-pos.T)/height;
	for(int i=0;it!=cont.end();++it,++i)
	{
		if(i-scroll!=pos_cur) continue;
		return (*it).GetId();
	}
	return 0;
}

void FOClient::ContainerDraw(INTRECT& pos, int height, int scroll, ItemVec& cont, DWORD skip_id)
{
	int i=0,i2=0;
	for(ItemVecIt it=cont.begin(),end=cont.end();it!=end;++it)
	{
		Item& item=*it;
		if(item.GetId()==skip_id) continue;
		if(i>=scroll && i<scroll+pos.H()/height)
		{
			AnyFrames* anim=ResMngr.GetInvAnim(item.GetPicInv());
			if(anim) SprMngr.DrawSpriteSize(anim->GetCurSprId(),pos.L,pos.T+(i2*height),pos.W(),height,false,true,item.GetInvColor());
			i2++;
		}
		i++;
	}

	SprMngr.Flush();

	i=0,i2=0;
	for(ItemVecIt it=cont.begin(),end=cont.end();it!=end;++it)
	{
		Item& item=*it;
		if(item.GetId()==skip_id) continue;
		if(i>=scroll && i<scroll+pos.H()/height)
		{
			if(item.GetCount()>1) SprMngr.DrawStr(INTRECT(pos.L,pos.T+(i2*height),pos.R,pos.T+(i2*height)+height),Str::Format("x%u",item.GetCount()),0,COLOR_TEXT_WHITE);
			i2++;
		}
		i++;
	}
}

Item* FOClient::GetContainerItem(ItemVec& cont, DWORD id)
{
	ItemVecIt it=std::find(cont.begin(),cont.end(),id);
	return it!=cont.end()?&(*it):NULL;
}

void FOClient::CollectContItems()
{
	InvContInit.clear();
	if(Chosen) Chosen->GetInvItems(InvContInit);

	if(IsScreenPresent(SCREEN__BARTER))
	{
		// Manage offered items
		for(ItemVecIt it=BarterCont1oInit.begin();it!=BarterCont1oInit.end();)
		{
			Item& item=*it;
			ItemVecIt it_=std::find(InvContInit.begin(),InvContInit.end(),item.GetId());
			if(it_==InvContInit.end() || (*it_).GetCount()<item.GetCount()) it=BarterCont1oInit.erase(it);
			else ++it;
		}
		for(ItemVecIt it=InvContInit.begin();it!=InvContInit.end();)
		{
			Item& item=*it;
			ItemVecIt it_=std::find(BarterCont1oInit.begin(),BarterCont1oInit.end(),item.GetId());
			if(it_!=BarterCont1oInit.end())
			{
				Item& item_=*it_;

				DWORD count=item_.GetCount();
				item_.Data=item.Data;
				item_.Count_Set(count);

				if(item.IsGrouped()) item.Count_Sub(item_.GetCount());
				if(!item.IsGrouped() || !item.GetCount())
				{
					it=InvContInit.erase(it);
					continue;
				}
			}
			++it;
		}
	}

	ProcessItemsCollection(ITEMS_INVENTORY,InvContInit,InvCont);
	ProcessItemsCollection(ITEMS_USE,InvContInit,UseCont);
	ProcessItemsCollection(ITEMS_BARTER,InvContInit,BarterCont1);
	ProcessItemsCollection(ITEMS_BARTER_OFFER,BarterCont1oInit,BarterCont1o);
	ProcessItemsCollection(ITEMS_BARTER_OPPONENT,BarterCont2Init,BarterCont2);
	ProcessItemsCollection(ITEMS_BARTER_OPPONENT_OFFER,BarterCont2oInit,BarterCont2o);
	ProcessItemsCollection(ITEMS_PICKUP,InvContInit,PupCont1);
	ProcessItemsCollection(ITEMS_PICKUP_FROM,PupCont2Init,PupCont2);
}

void FOClient::ProcessItemsCollection(int collection, ItemVec& init_items, ItemVec& result)
{
	result=init_items;
	if(result.empty()) return;

	if(Script::PrepareContext(ClientFunctions.ItemsCollection,CALL_FUNC_STR,"Game"))
	{
		asIScriptArray* arr=Script::CreateArray("ItemCl@[]");
		if(arr)
		{
			ItemPtrVec items_ptr;
			items_ptr.reserve(init_items.size());
			for(ItemVecIt it=init_items.begin(),end=init_items.end();it!=end;++it) items_ptr.push_back((*it).Clone());
			Script::AppendVectorToArray(items_ptr,arr);

			Script::SetArgDword(collection);
			Script::SetArgObject(arr);
			if(Script::RunPrepared())
			{
				items_ptr.clear();
				result.clear();
				Script::AssignScriptArrayInVector(items_ptr,arr);
				for(ItemPtrVecIt it=items_ptr.begin(),end=items_ptr.end();it!=end;++it)
				{
					Item* item=*it;
					if(item) result.push_back(*item);
				}
			}

			arr->Release();
		}
	}
}

void FOClient::UpdateContLexems(ItemVec& cont, DWORD item_id, const char* lexems)
{
	ItemVecIt it=std::find(cont.begin(),cont.end(),item_id);
	if(it!=cont.end()) (*it).Lexems=lexems;
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::InvDraw() 
{
	// Check
	if(!Chosen) return;

	// Main pic
	SprMngr.DrawSprite(InvPWMain,InvX,InvY,COLOR_IFACE);

	// Scroll up
	if(InvScroll<=0)
		SprMngr.DrawSprite(InvPBScrUpOff,InvBScrUp[0]+InvX,InvBScrUp[1]+InvY);
	else
	{
		if(IfaceHold==IFACE_INV_SCRUP)
			SprMngr.DrawSprite(InvPBScrUpDw,InvBScrUp[0]+InvX,InvBScrUp[1]+InvY);
		else
			SprMngr.DrawSprite(InvPBScrUpUp,InvBScrUp[0]+InvX,InvBScrUp[1]+InvY);
	}

	// Scroll down
	if(InvScroll>=(int)InvCont.size()-(InvWInv[3]-InvWInv[1])/InvHeightItem)
		SprMngr.DrawSprite(InvPBScrDwOff,InvBScrDn[0]+InvX,InvBScrDn[1]+InvY);
	else
	{
		if(IfaceHold==IFACE_INV_SCRDW)
			SprMngr.DrawSprite(InvPBScrDwDw,InvBScrDn[0]+InvX,InvBScrDn[1]+InvY);
		else
			SprMngr.DrawSprite(InvPBScrDwUp,InvBScrDn[0]+InvX,InvBScrDn[1]+InvY);
	}

	// Ok button
	if(IfaceHold==IFACE_INV_OK)
		SprMngr.DrawSprite(InvPBOkDw,InvBOk[0]+InvX,InvBOk[1]+InvY);
	else
		SprMngr.DrawSprite(InvPBOkUp,InvBOk[0]+InvX,InvBOk[1]+InvY);

	// Chosen
	Chosen->DrawStay(INTRECT(InvWChosen,InvX,InvY));

	// Slot Main
	if(Chosen->ItemSlotMain->GetId() && (IsCurMode(CUR_DEFAULT) || IfaceHold!=IFACE_INV_SLOT1))
	{
		AnyFrames* anim=ResMngr.GetInvAnim(Chosen->ItemSlotMain->GetPicInv());
		if(anim) SprMngr.DrawSpriteSize(anim->GetCurSprId(),InvWSlot1[0]+InvX,InvWSlot1[1]+InvY,InvWSlot1.W(),InvWSlot1.H(),false,true,Chosen->ItemSlotMain->GetInvColor());
	}

	// Slot Extra
	if(Chosen->ItemSlotExt->GetId() && (IsCurMode(CUR_DEFAULT) || IfaceHold!=IFACE_INV_SLOT2))
	{
		AnyFrames* anim=ResMngr.GetInvAnim(Chosen->ItemSlotExt->GetPicInv());
		if(anim) SprMngr.DrawSpriteSize(anim->GetCurSprId(),InvWSlot2[0]+InvX,InvWSlot2[1]+InvY,InvWSlot2.W(),InvWSlot2.H(),false,true,Chosen->ItemSlotExt->GetInvColor());
	}

	// Slot Armor
	if(Chosen->ItemSlotArmor->GetId() && (IsCurMode(CUR_DEFAULT) || IfaceHold!=IFACE_INV_ARMOR))
	{
		AnyFrames* anim=ResMngr.GetInvAnim(Chosen->ItemSlotArmor->GetPicInv());
		if(anim) SprMngr.DrawSpriteSize(anim->GetCurSprId(),InvWArmor[0]+InvX,InvWArmor[1]+InvY,InvWArmor.W(),InvWArmor.H(),false,true,Chosen->ItemSlotArmor->GetInvColor());
	}

	// Extended slots
	for(SlotExtVecIt it=SlotsExt.begin(),end=SlotsExt.end();it!=end;++it)
	{
		SlotExt& se=*it;
		if(se.Rect.IsZero()) continue;
		Item* item=Chosen->GetItemSlot(se.Index);
		if(!item || (!IsCurMode(CUR_DEFAULT) && IfaceHold==IFACE_INV_SLOTS_EXT && item->GetId()==InvHoldId)) continue;
		AnyFrames* anim=ResMngr.GetInvAnim(item->GetPicInv());
		if(!anim) continue;
		SprMngr.DrawSpriteSize(anim->GetCurSprId(),se.Rect[0]+InvX,se.Rect[1]+InvY,se.Rect.W(),se.Rect.H(),false,true,item->GetInvColor());
	}

	// Items in inventory
	DWORD skip_id=0;
	if(IsCurMode(CUR_HAND) && IfaceHold==IFACE_INV_INV && InvHoldId) skip_id=InvHoldId;
	ContainerDraw(INTRECT(InvWInv,InvX,InvY),InvHeightItem,InvScroll,InvCont,skip_id);

	if(InvItemInfo.empty())
	{
		// Inventory info
		int ox,oy;
		const char* result=FmtGenericDesc(DESC_INVENTORY_MAIN,ox,oy);
		if(result) SprMngr.DrawStr(INTRECT(InvWText,InvX+ox,InvY+oy),result,FT_NOBREAK_LINE);
		result=FmtGenericDesc(DESC_INVENTORY_SPECIAL,ox,oy);
		if(result) SprMngr.DrawStr(INTRECT(InvWText,InvX+ox,InvY+oy),result,FT_NOBREAK_LINE);
		result=FmtGenericDesc(DESC_INVENTORY_STATS,ox,oy);
		if(result) SprMngr.DrawStr(INTRECT(InvWText,InvX+ox,InvY+oy),result,FT_NOBREAK_LINE);
		result=FmtGenericDesc(DESC_INVENTORY_RESIST,ox,oy);
		if(result) SprMngr.DrawStr(INTRECT(InvWText,InvX+ox,InvY+oy),result,FT_NOBREAK_LINE);
	}
	else
	{
		// Item info
		SprMngr.DrawStr(INTRECT(InvWText,InvX,InvY),InvItemInfo.c_str(),FT_SKIPLINES(InvItemInfoScroll));
	}
}

void FOClient::InvLMouseDown()
{
	IfaceHold=IFACE_NONE;
	InvHoldId=0;
	InvFocus=INVF_NONE;
	if(!Chosen) return;

	if(IsCurInRect(InvWMain,InvX,InvY))
	{
		if(IsCurInRect(InvWInv,InvX,InvY))
		{
			InvHoldId=GetCurContainerItemId(INTRECT(InvWInv,InvX,InvY),InvHeightItem,InvScroll,InvCont);
			if(InvHoldId) IfaceHold=IFACE_INV_INV;
		}
		else if(IsCurInRect(InvWSlot1,InvX,InvY) && Chosen->ItemSlotMain->GetId())
		{
			InvHoldId=Chosen->ItemSlotMain->GetId();
			IfaceHold=IFACE_INV_SLOT1;
		}
		else if(IsCurInRect(InvWSlot2,InvX,InvY) && Chosen->ItemSlotExt->GetId())
		{
			InvHoldId=Chosen->ItemSlotExt->GetId();
			IfaceHold=IFACE_INV_SLOT2;
		}
		else if(IsCurInRect(InvWArmor,InvX,InvY) && Chosen->ItemSlotArmor->GetId())
		{
			InvHoldId=Chosen->ItemSlotArmor->GetId();
			IfaceHold=IFACE_INV_ARMOR;
		}
		else if(IsCurInRect(InvBScrUp,InvX,InvY))
		{
			Timer::StartAccelerator(ACCELERATE_INV_SCRUP);
			IfaceHold=IFACE_INV_SCRUP;
		}
		else if(IsCurInRect(InvBScrDn,InvX,InvY))
		{
			Timer::StartAccelerator(ACCELERATE_INV_SCRDOWN);
			IfaceHold=IFACE_INV_SCRDW;
		}
		else if(IsCurInRect(InvBOk,InvX,InvY)) IfaceHold=IFACE_INV_OK;
		else
		{
			// Try find extended slot
			for(SlotExtVecIt it=SlotsExt.begin(),end=SlotsExt.end();it!=end;++it)
			{
				SlotExt& se=*it;
				if(!se.Rect.IsZero() && IsCurInRect(se.Rect,InvX,InvY))
				{
					Item* item=Chosen->GetItemSlot(se.Index);
					if(item)
					{
						InvHoldId=item->GetId();
						IfaceHold=IFACE_INV_SLOTS_EXT;
						break;
					}
				}
			}

			// Move screen
			if(IfaceHold==IFACE_NONE)
			{
				InvVectX=CurX-InvX;
				InvVectY=CurY-InvY;
				IfaceHold=IFACE_INV_MAIN;
			}
		}

		if(IsCurMode(CUR_DEFAULT) && (IfaceHold==IFACE_INV_INV || IfaceHold==IFACE_INV_SLOT1 || IfaceHold==IFACE_INV_SLOT2 ||
			IfaceHold==IFACE_INV_ARMOR || IfaceHold==IFACE_INV_SLOTS_EXT)) LMenuTryActivate();
	}
}

void FOClient::InvLMouseUp()
{
	if(!Chosen) return;
	if(IsCurMode(CUR_HAND) && (IfaceHold==IFACE_INV_INV || IfaceHold==IFACE_INV_SLOT1 || IfaceHold==IFACE_INV_SLOT2 || IfaceHold==IFACE_INV_ARMOR || IfaceHold==IFACE_INV_SLOTS_EXT))
	{
		int to_slot=-1;
		Item* to_weap=NULL;
		if(IsCurInRect(InvWInv,InvX,InvY)) to_slot=SLOT_INV;
		else if(IsCurInRect(InvWSlot1,InvX,InvY))
		{
			to_slot=SLOT_HAND1;
			if(Chosen->ItemSlotMain->GetId()) to_weap=Chosen->ItemSlotMain;
		}
		else if(IsCurInRect(InvWSlot2,InvX,InvY))
		{
			to_slot=SLOT_HAND2;
			if(Chosen->ItemSlotExt->GetId()) to_weap=Chosen->ItemSlotExt;
		}
		else if(IsCurInRect(InvWArmor,InvX,InvY)) to_slot=SLOT_ARMOR;
		else if(!IsCurInRect(InvWMain,InvX,InvY)) to_slot=SLOT_GROUND;
		else
		{
			// Find extended slot
			for(SlotExtVecIt it=SlotsExt.begin(),end=SlotsExt.end();it!=end;++it)
			{
				SlotExt& se=*it;
				if(!se.Rect.IsZero() && IsCurInRect(se.Rect,InvX,InvY))
				{
					to_slot=se.Index;
					break;
				}
			}
		}

		if(to_slot==-1)
		{
			IfaceHold=0;
			InvHoldId=0;
			return;
		}

		if(to_weap && !to_weap->IsWeapon()) to_weap=NULL;

		int from_slot=-1;
		switch(IfaceHold)
		{
		case IFACE_INV_INV: from_slot=SLOT_INV; break;
		case IFACE_INV_SLOT1: from_slot=SLOT_HAND1; break;
		case IFACE_INV_SLOT2: from_slot=SLOT_HAND2; break;
		case IFACE_INV_ARMOR: from_slot=SLOT_ARMOR; break;
		default: // IFACE_INV_SLOTS_EXT:
			{
				Item* item=Chosen->GetItem(InvHoldId);
				if(item) from_slot=item->ACC_CRITTER.Slot;
			}
			break;
		}
		IfaceHold=IFACE_NONE;

		if(from_slot==-1 || from_slot==to_slot) return;

		Item* item=Chosen->GetItem(InvHoldId);
		InvHoldId=0;
		if(!item || item->ACC_CRITTER.Slot!=from_slot) return; 

		// Load weapon
		if(item->IsAmmo() && to_weap)
		{
			if(item->Proto->Ammo.Caliber==to_weap->Proto->Weapon.Caliber && (to_weap->Data.TechInfo.AmmoCount<to_weap->Proto->Weapon.VolHolder || to_weap->Data.TechInfo.AmmoPid!=item->GetProtoId()))
			{
				AddActionBack(CHOSEN_USE_ITEM,to_weap->GetId(),0,TARGET_SELF_ITEM,item->GetId(),USE_RELOAD);
			}
		}
		// Transit
		else
		{
			// Split
			if(to_slot==SLOT_GROUND && item->IsGrouped() && item->GetCount()>1)
			{
				SplitStart(item,to_slot);
				return;
			}
			// Without split
			else
			{
				AddActionBack(CHOSEN_MOVE_ITEM,item->GetId(),item->GetCount(),to_slot);
			}
		}
	}
	else if(IfaceHold==IFACE_INV_SCRUP && IsCurInRect(InvBScrUp,InvX,InvY))
	{
		if(InvScroll>0) InvScroll--;
	}
	else if(IfaceHold==IFACE_INV_SCRDW && IsCurInRect(InvBScrDn,InvX,InvY))
	{
		if(InvScroll<(int)InvCont.size()-InvWInv.H()/InvHeightItem) InvScroll++;
	}
	else if(IfaceHold==IFACE_INV_OK && IsCurInRect(InvBOk,InvX,InvY))
	{
		ShowScreen(SCREEN_NONE);
	}

	InvHoldId=0;
	IfaceHold=IFACE_NONE;
}

void FOClient::InvRMouseDown()
{
	if(IfaceHold==IFACE_NONE)
	{
		SetCurCastling(CUR_DEFAULT,CUR_HAND);
		if(IsCurMode(CUR_HAND)) InvItemInfo="";
	}
}

void FOClient::InvMouseMove()
{
	if(IfaceHold==IFACE_INV_MAIN)
	{
		InvX=CurX-InvVectX;
		InvY=CurY-InvVectY;

		if(InvX<0) InvX=0;
		if(InvX+InvWMain[2]>MODE_WIDTH) InvX=MODE_WIDTH-InvWMain[2];
		if(InvY<0) InvY=0;
		//if(InvY+InvMain[3]>IntY) InvY=IntY-InvMain[3];
		if(InvY+InvWMain[3]>MODE_HEIGHT) InvY=MODE_HEIGHT-InvWMain[3];
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::UseDraw()
{
	// Check
	if(!Chosen) return;

	// Main
	SprMngr.DrawSprite(UseWMainPicNone,UseX,UseY);

	// Scroll up
	if(UseScroll<=0)
		SprMngr.DrawSprite(UseBScrUpPicOff,UseBScrUp[0]+UseX,UseBScrUp[1]+UseY);
	else
	{
		if(IfaceHold==IFACE_USE_SCRUP)
			SprMngr.DrawSprite(UseBScrUpPicDown,UseBScrUp[0]+UseX,UseBScrUp[1]+UseY);
		else
			SprMngr.DrawSprite(UseBScrUpPicUp,UseBScrUp[0]+UseX,UseBScrUp[1]+UseY);
	}

	// Scroll down
	if(UseScroll>=(int)UseCont.size()-UseWInv.H()/UseHeightItem)
		SprMngr.DrawSprite(UseBScrDownPicOff,UseBScrDown[0]+UseX,UseBScrDown[1]+UseY);
	else
	{
		if(IfaceHold==IFACE_USE_SCRDW)
			SprMngr.DrawSprite(UseBScrDownPicDown,UseBScrDown[0]+UseX,UseBScrDown[1]+UseY);
		else
			SprMngr.DrawSprite(UseBScrDownPicUp,UseBScrDown[0]+UseX,UseBScrDown[1]+UseY);
	}

	// Cancel button
	if(IfaceHold==IFACE_USE_CANCEL) SprMngr.DrawSprite(UseBCancelPicDown,UseBCancel[0]+UseX,UseBCancel[1]+UseY);

	// Chosen
	Chosen->DrawStay(INTRECT(UseWChosen,UseX,UseY));

	// Items
	ContainerDraw(INTRECT(UseWInv,UseX,UseY),UseHeightItem,UseScroll,UseCont,0);
}

void FOClient::UseLMouseDown()
{
	IfaceHold=IFACE_NONE;

	if(!IsCurInRect(UseWMain,UseX,UseY)) return;

	if(IsCurInRect(UseWInv,UseX,UseY))
	{
		UseHoldId=GetCurContainerItemId(INTRECT(UseWInv,UseX,UseY),UseHeightItem,UseScroll,UseCont);
		if(UseHoldId && IsCurMode(CUR_HAND)) IfaceHold=IFACE_USE_INV;
		if(UseHoldId && IsCurMode(CUR_DEFAULT)) LMenuTryActivate();
	}
	else if(IsCurInRect(UseBScrUp,UseX,UseY))
	{
		Timer::StartAccelerator(ACCELERATE_USE_SCRUP);
		IfaceHold=IFACE_USE_SCRUP;
	}
	else if(IsCurInRect(UseBScrDown,UseX,UseY))
	{
		Timer::StartAccelerator(ACCELERATE_USE_SCRDOWN);
		IfaceHold=IFACE_USE_SCRDW;
	}
	else if(IsCurInRect(UseBCancel,UseX,UseY)) IfaceHold=IFACE_USE_CANCEL;
	else
	{
		UseVectX=CurX-UseX;
		UseVectY=CurY-UseY;
		IfaceHold=IFACE_USE_MAIN;
	}
}

void FOClient::UseLMouseUp()
{
	switch(IfaceHold)
	{
	case IFACE_USE_INV:
		if(!IsCurInRect(UseWInv,UseX,UseY) || !IsCurMode(CUR_HAND) || !UseHoldId) break;

		if(ShowScreenType)
		{
			if(ShowScreenNeedAnswer) Net_SendScreenAnswer(UseHoldId,"");
		}
		else
		{
			if(UseSelect.IsCritter())
			{
				AddActionBack(CHOSEN_USE_ITEM,UseHoldId,0,TARGET_CRITTER,UseSelect.GetId(),USE_USE);
			}
			else if(UseSelect.IsItem())
			{
				ItemHex* item=GetItem(UseSelect.GetId());
				if(item) AddActionBack(CHOSEN_USE_ITEM,UseHoldId,0,item->IsItem()?TARGET_ITEM:TARGET_SCENERY,UseSelect.GetId(),USE_USE);
			}
		}

		ShowScreen(SCREEN_NONE);
		break;
	case IFACE_USE_SCRUP:
		if(!IsCurInRect(UseBScrUp,UseX,UseY) || UseScroll<=0) break;
		UseScroll--;
		break;
	case IFACE_USE_SCRDW:
		if(!IsCurInRect(UseBScrDown,UseX,UseY) || UseScroll>=(int)UseCont.size()-UseWInv.H()/UseHeightItem) break;
		UseScroll++;
		break;
	case IFACE_USE_CANCEL:
		if(!IsCurInRect(UseBCancel,UseX,UseY)) break;
		ShowScreen(SCREEN_NONE);
		break;
	default:
		break;
	}

	UseHoldId=0;
	IfaceHold=IFACE_NONE;
}

void FOClient::UseRMouseDown()
{
	if(IfaceHold==IFACE_NONE) SetCurCastling(CUR_DEFAULT,CUR_HAND);
}

void FOClient::UseMouseMove()
{
	if(IfaceHold==IFACE_USE_MAIN)
	{
		UseX=CurX-UseVectX;
		UseY=CurY-UseVectY;
		if(UseX<0) UseX=0;
		if(UseX+UseWMain[2]>MODE_WIDTH) UseX=MODE_WIDTH-UseWMain[2];
		if(UseY<0) UseY=0;
		if(UseY+UseWMain[3]>MODE_HEIGHT) UseY=MODE_HEIGHT-UseWMain[3];
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::ConsoleDraw()
{
	bool is_game_screen=(IsMainScreen(SCREEN_GAME) || IsMainScreen(SCREEN_GLOBAL_MAP));

	// Pause indicator
	if(Timer::IsGamePaused() && is_game_screen && !IsScreenPresent(SCREEN__MENU_OPTION))
		SprMngr.DrawStr(INTRECT(0,20,MODE_WIDTH,MODE_HEIGHT),MsgGame->GetStr(STR_GAME_PAUSED),FT_CENTERX|FT_COLORIZE,COLOR_TEXT_DRED,FONT_BIG);

	// Console
	if(ConsoleEdit && is_game_screen)
	{
		if(IsMainScreen(SCREEN_GAME)) SprMngr.DrawSprite(ConsolePic,IntX+ConsolePicX,(IntVisible?(IntAddMess?IntWAddMess[1]:IntY):MODE_HEIGHT)+ConsolePicY);

		INTRECT rect(IntX+ConsoleTextX,(IntVisible?(IntAddMess?IntWAddMess[1]:IntY):MODE_HEIGHT)+ConsoleTextY,MODE_WIDTH,MODE_HEIGHT);
		if(IsMainScreen(SCREEN_GLOBAL_MAP)) rect=GmapWPanel;

		char* buf=(char*)Str::Format("%s",ConsoleStr);
		Str::Insert(&buf[ConsoleCur],Timer::FastTick()%800<400?"!":".");
		SprMngr.DrawStr(rect,buf,FT_NOBREAK);
	}

	// Help info
	if(GameOpt.HelpInfo)
	{
		if(!Chosen) return;

		//SprMngr.DrawSprite(ResMngr.GetIfaceSprId(297),0,0);

		if(GameOpt.DebugInfo)
		{
			GetMouseHex();
			FLTPOINT p=Animation3d::Convert2dTo3d(CurX,CurY);
			SprMngr.DrawStr(INTRECT(250,5,450,300),Str::Format(
				"cr_hx<%u>, cr_hy<%u>,\nhx<%u>, hy<%u>,\ncur_x<%d>, cur_y<%d>\nCond<%u>, CondExt<%u>\nox<%d>, oy<%d>\nFarDir<%d>\n3dXY<%f,%f>",
				Chosen->HexX,Chosen->HexY,TargetX,TargetY,CurX,CurY,Chosen->Cond,Chosen->CondExt,GameOpt.ScrOx,GameOpt.ScrOy,
				GetFarDir(Chosen->HexX,Chosen->HexY,TargetX,TargetY),p.X,p.Y
				),FT_CENTERX,D3DCOLOR_XRGB(255,240,0));

			SprMngr.DrawStr(INTRECT(450,5,650,300),Str::Format(
				"Anim info: cur_id %d, cur_ox %d, cur_oy %d",
				Chosen->SprId,Chosen->SprOx,Chosen->SprOy
				),FT_CENTERX,D3DCOLOR_XRGB(255,240,0));

			SprMngr.DrawStr(INTRECT(650,5,800,300),Str::Format(
				"Time:%02d:%02d %02d:%02d:%04d x%02d\nSleep:%d\nSound:%d\nMusic:%d",
				GameOpt.Hour,GameOpt.Minute,GameOpt.Day,GameOpt.Month,GameOpt.Year,GameOpt.TimeMultiplier,
				GameOpt.Sleep,SndMngr.GetSoundVolume(),SndMngr.GetMusicVolume()
				),FT_CENTERX,D3DCOLOR_XRGB(255,240,0));
		}

		SprMngr.DrawStr(INTRECT(10,10,MODE_WIDTH,MODE_HEIGHT),Str::Format(
			"|0xFFBBBBBB FOnline %s\n"
			"by Gamers for Gamers\n"
			"version %04X-%02X beta\n\n"
			"Traffic, bytes:\n"
			"Send: %u\n"
			"Receive: %u\n"
			"Sum: %u\n"
			//"Receive Real: %u\n"
			"\n"
			"FPS: %d\n"
			"Ping: %d\n"
			"\n"
			//"sleep: %d\n"
			"Sound: %d\n"
			"Music: %d\n"
			"\n"
			"Sleep: %d\n",
			Singleplayer?"Singleplayer":"",
			CLIENT_VERSION,FO_PROTOCOL_VERSION&0xFF,
			BytesSend,BytesReceive,BytesReceive+BytesSend,/*BytesRealReceive,*/
			FPS,PingTime,SndMngr.GetSoundVolume(),SndMngr.GetMusicVolume(),GameOpt.Sleep
			),FT_COLORIZE,D3DCOLOR_XRGB(255,248,0),FONT_BIG);

		SprMngr.DrawStr(INTRECT(0,0,MODE_WIDTH,MODE_HEIGHT),MsgGame->GetStr(STR_GAME_HELP),FT_CENTERX|FT_CENTERY,COLOR_TEXT_WHITE,FONT_DEFAULT);
	}	
}

void FOClient::ConsoleKeyDown(BYTE dik)
{
	if(!IsMainScreen(SCREEN_GAME) && !IsMainScreen(SCREEN_GLOBAL_MAP)) return;

	if(dik==DIK_RETURN || dik==DIK_NUMPADENTER)
	{
		if(!ConsoleEdit)
		{
			ConsoleEdit=true;
			ConsoleStr[0]=0;
			ConsoleCur=0;
			ConsoleHistoryCur=ConsoleHistory.size();
			return;
		}
		
		if(!ConsoleStr[0])
		{
			ConsoleEdit=false;
			return;
		}

		ConsoleHistory.push_back(string(ConsoleStr));
		for(int i=0;i<ConsoleHistory.size()-1;i++)
		{
			if(ConsoleHistory[i]==ConsoleHistory[ConsoleHistory.size()-1])
			{
				ConsoleHistory.erase(ConsoleHistory.begin()+i);
				i=-1;
			}
		}
		ConsoleHistoryCur=ConsoleHistory.size();

		if(Keyb::CtrlDwn) Net_SendText(ConsoleStr,SAY_SHOUT);
		else if(Keyb::AltDwn) Net_SendText(ConsoleStr,SAY_WHISP);
		else if(Keyb::ShiftDwn) Net_SendText(ConsoleStr,SAY_RADIO);
		else Net_SendText(ConsoleStr,SAY_NORM);

		ConsoleStr[0]=0;
		ConsoleCur=0;
	}

	if(!ConsoleEdit) return;

	switch(dik)
	{
	case DIK_UP:
		if(ConsoleHistoryCur-1<0) return;
		ConsoleHistoryCur--;	
		StringCopy(ConsoleStr,ConsoleHistory[ConsoleHistoryCur].c_str());
		ConsoleCur=strlen(ConsoleStr);
		return;
	case DIK_DOWN:
		if(ConsoleHistoryCur+1>=ConsoleHistory.size())
		{
			ConsoleHistoryCur=ConsoleHistory.size();
			StringCopy(ConsoleStr,"");
			ConsoleCur=0;
			return;
		}
		ConsoleHistoryCur++;
		StringCopy(ConsoleStr,ConsoleHistory[ConsoleHistoryCur].c_str());
		ConsoleCur=strlen(ConsoleStr);
		return;
	default:
		Keyb::GetChar(dik,ConsoleStr,&ConsoleCur,MAX_NET_TEXT,KIF_NO_SPEC_SYMBOLS);
		if(dik==DIK_PAUSE) break;
		ConsoleLastKey=dik;
		Timer::StartAccelerator(ACCELERATE_CONSOLE);
		return;
	}
}

void FOClient::ConsoleKeyUp(BYTE key)
{
	ConsoleLastKey=0;
}

void FOClient::ConsoleProcess()
{
	if(ConsoleLastKey && Timer::ProcessAccelerator(ACCELERATE_CONSOLE)) Keyb::GetChar(ConsoleLastKey,ConsoleStr,&ConsoleCur,MAX_NET_TEXT,KIF_NO_SPEC_SYMBOLS);
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::GameDraw()
{
	// Move cursor
	if(IsCurMode(CUR_MOVE))
	{
		if((GameOpt.ScrollMouseRight || GameOpt.ScrollMouseLeft || GameOpt.ScrollMouseUp || GameOpt.ScrollMouseDown) || !GetMouseHex()) HexMngr.SetCursorVisible(false);
		else
		{
			HexMngr.SetCursorVisible(true);
			HexMngr.SetCursorPos(CurX,CurY,Keyb::CtrlDwn,false);
		}
	}

	// Map
	HexMngr.DrawMap();

	// Look borders
	LookBordersDraw();

	// Critters
	for(CritMapIt it=HexMngr.allCritters.begin();it!=HexMngr.allCritters.end();it++)
	{
		CritterCl* cr=(*it).second;

		// Follow pic
		if(GameOpt.ShowGroups && cr->SprDrawValid && Chosen)
		{
			if(cr->GetId()==(DWORD)Chosen->Params[ST_FOLLOW_CRIT])
			{
				SpriteInfo* si=SprMngr.GetSpriteInfo(GmapPFollowCrit);
				INTRECT tr=cr->GetTextRect();
				int x=(tr.L+((tr.R-tr.L)/2)+GameOpt.ScrOx)/GameOpt.SpritesZoom-(si?si->Width/2:0);
				int y=(tr.T+GameOpt.ScrOy)/GameOpt.SpritesZoom-(si?si->Height:0);
				DWORD col=(CheckDist(cr->GetHexX(),cr->GetHexY(),Chosen->GetHexX(),Chosen->GetHexY(),FOLLOW_DIST)/* && Chosen->IsFree()*/)?COLOR_IFACE:COLOR_IFACE_RED;
				SprMngr.DrawSprite(GmapPFollowCrit,x,y,col);
			}
			if(Chosen->GetId()==(DWORD)cr->Params[ST_FOLLOW_CRIT])
			{
				SpriteInfo* si=SprMngr.GetSpriteInfo(GmapPFollowCritSelf);
				INTRECT tr=cr->GetTextRect();
				int x=(tr.L+((tr.R-tr.L)/2)+GameOpt.ScrOx)/GameOpt.SpritesZoom-(si?si->Width/2:0);
				int y=(tr.T+GameOpt.ScrOy)/GameOpt.SpritesZoom-(si?si->Height:0);
				DWORD col=(CheckDist(cr->GetHexX(),cr->GetHexY(),Chosen->GetHexX(),Chosen->GetHexY(),FOLLOW_DIST)/* && Chosen->IsFree()*/)?COLOR_IFACE:COLOR_IFACE_RED;
				SprMngr.DrawSprite(GmapPFollowCritSelf,x,y,col);
			}
		}

		// Text on head
		cr->DrawTextOnHead();
	}

	// Texts on map
	DWORD tick=Timer::GameTick();
	for(MapTextVecIt it=GameMapTexts.begin();it!=GameMapTexts.end();)
	{
		MapText& mt=(*it);
		if(tick>=mt.StartTick+mt.Tick) it=GameMapTexts.erase(it);
		else
		{
			DWORD dt=tick-mt.StartTick;
			int procent=Procent(mt.Tick,dt);
			INTRECT r=AverageFlexRect(mt.Rect,mt.EndRect,procent);
			Field& f=HexMngr.GetField(mt.HexX,mt.HexY);
			int x=(f.ScrX+16+GameOpt.ScrOx)/GameOpt.SpritesZoom-100-(mt.Rect.L-r.L);
			int y=(f.ScrY+6-mt.Rect.H()-(mt.Rect.T-r.T)+GameOpt.ScrOy)/GameOpt.SpritesZoom-70;

			DWORD color=mt.Color;
			if(mt.Fade) color=(color^0xFF000000)|((0xFF*(100-procent)/100)<<24);
			else if(mt.Tick>500)
			{
				DWORD hide=mt.Tick-200;
				if(dt>=hide)
				{
					DWORD alpha=0xFF*(100-Procent(mt.Tick-hide,dt-hide))/100;
					color=(alpha<<24)|(color&0xFFFFFF);
				}
			}

			SprMngr.DrawStr(INTRECT(x,y,x+200,y+70),mt.Text.c_str(),FT_CENTERX|FT_BOTTOM|FT_COLORIZE|FT_BORDERED,color);
			it++;
		}
	}
}

void FOClient::GameKeyDown(BYTE dik)
{
	if(ConsoleEdit) return;

	if(Keyb::AltDwn)
	{
		switch(dik)
		{
#ifdef DEV_VESRION
		// Show/Hide sprites
		case DIK_1: GameOpt.ShowItem=!GameOpt.ShowItem; HexMngr.RefreshMap(); break;
		case DIK_2: GameOpt.ShowScen=!GameOpt.ShowScen; HexMngr.RefreshMap(); break;
		case DIK_3: GameOpt.ShowWall=!GameOpt.ShowWall; HexMngr.RefreshMap(); break;
		case DIK_4: GameOpt.ShowCrit=!GameOpt.ShowCrit; HexMngr.RefreshMap(); break;
		case DIK_5: GameOpt.ShowTile=!GameOpt.ShowTile; HexMngr.RefreshMap(); break;
		case DIK_6: GameOpt.ShowRoof=!GameOpt.ShowRoof; HexMngr.RefreshMap(); break;

		// Chosen animate
#define CHOSEN_ANIMATE(anim2) if(Chosen) Chosen->Animate(0,anim2,NULL)
		case DIK_A: CHOSEN_ANIMATE(1); break;
		case DIK_B: CHOSEN_ANIMATE(2); break;
		case DIK_C: CHOSEN_ANIMATE(3); break;
		case DIK_D: CHOSEN_ANIMATE(4); break;
		case DIK_E: CHOSEN_ANIMATE(5); break;
		case DIK_F: CHOSEN_ANIMATE(6); break;
		case DIK_G: CHOSEN_ANIMATE(7); break;
		case DIK_H: CHOSEN_ANIMATE(8); break;
		case DIK_I: CHOSEN_ANIMATE(9); break;
		case DIK_J: CHOSEN_ANIMATE(10); break;
		case DIK_K: CHOSEN_ANIMATE(11); break;
		case DIK_L: CHOSEN_ANIMATE(12); break;
		case DIK_M: CHOSEN_ANIMATE(13); break;
		case DIK_N: CHOSEN_ANIMATE(14); break;
		case DIK_O: CHOSEN_ANIMATE(15); break;
		case DIK_P: CHOSEN_ANIMATE(16); break;
		case DIK_Q: CHOSEN_ANIMATE(17); break;
		case DIK_R: CHOSEN_ANIMATE(18); break;
		case DIK_S: CHOSEN_ANIMATE(19); break;
		case DIK_T: CHOSEN_ANIMATE(20); break;
#endif
		case -1: break;
		default: break;
		}
	
		return;
	}

	if(Keyb::CtrlDwn || Keyb::ShiftDwn || Keyb::AltDwn) return;

	switch(dik)
	{
	case DIK_EQUALS:
	case DIK_ADD: GameOpt.Light+=5; if(GameOpt.Light>50) GameOpt.Light=50; SetDayTime(true); break;
	case DIK_MINUS:
	case DIK_SUBTRACT: GameOpt.Light-=5; if(GameOpt.Light<0) GameOpt.Light=0; SetDayTime(true); break;
	default: break;
	}

	if(Chosen)
	{
		switch(dik)
		{
		// Hot keys
		case DIK_A: if(Chosen->ItemSlotMain->IsWeapon() && Chosen->GetUse()<MAX_USES) SetCurMode(CUR_USE_WEAPON); break;
		case DIK_C: ShowScreen(SCREEN__CHARACTER); if(Chosen->Params[ST_UNSPENT_PERKS]) ShowScreen(SCREEN__PERK); break;
		case DIK_G: TryPickItemOnGround(); break;
		case DIK_T: GameOpt.ShowGroups=!GameOpt.ShowGroups; break;
		case DIK_I: ShowScreen(SCREEN__INVENTORY); break;
		case DIK_P: ShowScreen(SCREEN__PIP_BOY); break;
		case DIK_F: ShowScreen(SCREEN__FIX_BOY); break;
		//case DIK_Z: PipBoy clock
		case DIK_O: ShowScreen(SCREEN__MENU_OPTION); break;
		case DIK_B: ChosenChangeSlot(); break;
		case DIK_M: IfaceHold=IFACE_GAME_MNEXT; GameRMouseUp(); break;
		case DIK_N: if(Chosen->NextRateItem(false)) Net_SendRateItem(); break;
		case DIK_S: SboxUseOn.Clear(); ShowScreen(SCREEN__SKILLBOX); break;
		case DIK_SLASH: AddMess(FOMB_GAME,Str::Format("Time: %02d.%02d.%d %02d:%02d:%02d x%u",GameOpt.Day,GameOpt.Month,GameOpt.Year,GameOpt.Hour,GameOpt.Minute,GameOpt.Second,GameOpt.TimeMultiplier)); break;
		case DIK_COMMA: SetAction(CHOSEN_DIR,1/*CW*/); break;
		case DIK_PERIOD: SetAction(CHOSEN_DIR,0/*CCW*/); break;
		case DIK_TAB: ShowScreen(SCREEN__MINI_MAP); break;
		case DIK_HOME: HexMngr.ScrollToHex(Chosen->HexX,Chosen->HexY,0.1,true); break;
		case DIK_SCROLL: if(HexMngr.AutoScroll.LockedCritter) HexMngr.AutoScroll.LockedCritter=0; else HexMngr.AutoScroll.LockedCritter=Chosen->GetId(); break;
		// Skills
		case DIK_1: SetAction(CHOSEN_USE_SKL_ON_CRITTER,SK_SNEAK); break;
		case DIK_2: CurSkill=SK_LOCKPICK; SetCurMode(CUR_USE_SKILL); break;
		case DIK_3: CurSkill=SK_STEAL; SetCurMode(CUR_USE_SKILL); break;
		case DIK_4: CurSkill=SK_TRAPS; SetCurMode(CUR_USE_SKILL); break;
		case DIK_5: CurSkill=SK_FIRST_AID; SetCurMode(CUR_USE_SKILL); break;
		case DIK_6: CurSkill=SK_DOCTOR; SetCurMode(CUR_USE_SKILL); break;
		case DIK_7: CurSkill=SK_SCIENCE; SetCurMode(CUR_USE_SKILL); break;
		case DIK_8: CurSkill=SK_REPAIR; SetCurMode(CUR_USE_SKILL); break;
		default: break;
		}
	}
}

void FOClient::GameLMouseDown()
{
	IfaceHold=IFACE_NONE;
	if(!Chosen) return;

	if(IsCurMode(CUR_DEFAULT))
	{
		LMenuTryActivate();
	}
	else if(IsCurMode(CUR_MOVE))
	{
		ActionEvent* act=(IsAction(CHOSEN_MOVE)?&ChosenAction[0]:NULL);
		if(act && Timer::FastTick()-act->Param[4]<GetDoubleClickTime())
		{
			act->Param[2]=1/*run*/;
			act->Param[4]=0;
		}
		else if(GetMouseHex())
		{
			SetAction(CHOSEN_MOVE,TargetX,TargetY,(act?0:0x100)|(Keyb::ShiftDwn==true?1/*run*/:0/*walk*/),0,Timer::FastTick());
		}
	}
	else if(IsCurMode(CUR_USE_ITEM) || IsCurMode(CUR_USE_WEAPON))
	{
		CritterCl* cr;
		ItemHex* item;
		if(Chosen->ItemSlotMain->IsWeapon() && Chosen->GetUse()<MAX_USES)
		{
			cr=HexMngr.GetCritterPixel(CurX,CurY,true);
			if(cr==Chosen) cr=NULL;
			item=NULL;
		}
		else
		{
			HexMngr.GetSmthPixel(CurX,CurY,item,cr);
		}

		if(cr)
		{
			// Memory target id
			TargetSmth.SetCritter(cr->GetId());

			// Aim shoot
			if(Chosen->ItemSlotMain->IsWeapon() && Chosen->GetUse()<MAX_USES && cr!=Chosen && Chosen->IsAim())
			{
				if(!CritType::IsCanAim(Chosen->GetCrType())) AddMess(FOMB_GAME,"Aim attack is not aviable for this critter type.");
				else if(!Chosen->IsPerk(TRAIT_FAST_SHOT)) ShowScreen(SCREEN__AIM);
				return;
			}

			// Use item
			SetAction(CHOSEN_USE_ITEM,Chosen->ItemSlotMain->GetId(),Chosen->ItemSlotMain->GetProtoId(),TARGET_CRITTER,cr->GetId(),Chosen->GetFullRate());
		}
		else if(item)
		{
			TargetSmth.SetItem(item->GetId());
			SetAction(CHOSEN_USE_ITEM,Chosen->ItemSlotMain->GetId(),Chosen->ItemSlotMain->GetProtoId(),item->IsItem()?TARGET_ITEM:TARGET_SCENERY,item->GetId(),USE_USE);
		}
	}
	else if(IsCurMode(CUR_USE_SKILL))
	{
		CritterCl* cr;
		ItemHex* item;
		HexMngr.GetSmthPixel(CurX,CurY,item,cr);

		// Use skill
		if(cr)
		{
			SetAction(CHOSEN_USE_SKL_ON_CRITTER,CurSkill,cr->GetId(),Chosen->GetFullRate());
		}
		else if(item)
		{
			if(item->IsScenOrGrid())
				SetAction(CHOSEN_USE_SKL_ON_SCEN,CurSkill,item->GetProtoId(),item->GetHexX(),item->GetHexY());
			else
				SetAction(CHOSEN_USE_SKL_ON_ITEM,false,CurSkill,item->GetId());
		}

		SetCurMode(CUR_DEFAULT);
	}
}

void FOClient::GameLMouseUp()
{
	IfaceHold=IFACE_NONE;
}

void FOClient::GameRMouseDown()
{
	IfaceHold=IFACE_NONE;

	if(!(IntVisible && ((IsCurInRect(IntWMain) && SprMngr.IsPixNoTransp(IntMainPic,CurX-IntWMain[0],CurY-IntWMain[1],false)) ||
		(IntAddMess && IsCurInRect(IntWAddMess) && SprMngr.IsPixNoTransp(IntPWAddMess,CurX-IntWAddMess[0],CurY-IntWAddMess[1],false))))) IfaceHold=IFACE_GAME_MNEXT;
}

void FOClient::GameRMouseUp()
{
	if(IfaceHold==IFACE_GAME_MNEXT && Chosen)
	{
		switch(GetCurMode())
		{
		case CUR_DEFAULT: SetCurMode(CUR_MOVE); break;
		case CUR_MOVE:
			if(Chosen->GetTimeout(TO_BATTLE) && Chosen->ItemSlotMain->IsWeapon())
				SetCurMode(CUR_USE_WEAPON);
			else
				SetCurMode(CUR_DEFAULT);
			break;
		case CUR_USE_ITEM: SetCurMode(CUR_DEFAULT); break;
		case CUR_USE_WEAPON: SetCurMode(CUR_DEFAULT); break;
		case CUR_USE_SKILL: SetCurMode(CUR_MOVE); break;
		default: SetCurMode(CUR_DEFAULT); break;
		}
	}
	IfaceHold=IFACE_NONE;
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::IntDraw()
{
	if(!ConsoleEdit && Keyb::KeyPressed[DIK_Z] && GameOpt.SpritesZoomMin!=GameOpt.SpritesZoomMax)
	{
		int screen=GetActiveScreen();
		if(screen==SCREEN_NONE || screen==SCREEN__TOWN_VIEW)
		{
			SprMngr.DrawStr(INTRECT(0,0,MODE_WIDTH,MODE_HEIGHT),
				FmtGameText(STR_ZOOM,(int)(1.0f/GameOpt.SpritesZoom*100.0f)),FT_CENTERX|FT_CENTERY|FT_COLORIZE,COLOR_TEXT_SAND,FONT_BIG);
		}
	}

	if(!Chosen || !IntVisible) return;

	SprMngr.DrawSprite(IntMainPic,IntX,IntY);
	SprMngr.DrawSprite(AnimGetCurSpr(IntWCombatAnim),IntWCombat[0],IntWCombat[1]);

	if(IntAddMess)
	{
		SprMngr.DrawSprite(IntPWAddMess,IntWAddMess[0],IntWAddMess[1]);
		SprMngr.DrawSprite(IntPBAddMessDn,IntBAddMess[0],IntBAddMess[1]);
	}

	if(std::find(MessBoxFilters.begin(),MessBoxFilters.end(),FOMB_COMBAT_RESULT)!=MessBoxFilters.end()) SprMngr.DrawSprite(IntPBMessFilter1Dn,IntBMessFilter1[0],IntBMessFilter1[1]);
	if(std::find(MessBoxFilters.begin(),MessBoxFilters.end(),FOMB_TALK)!=MessBoxFilters.end()) SprMngr.DrawSprite(IntPBMessFilter2Dn,IntBMessFilter2[0],IntBMessFilter2[1]);
	if(std::find(MessBoxFilters.begin(),MessBoxFilters.end(),FOMB_VIEW)!=MessBoxFilters.end()) SprMngr.DrawSprite(IntPBMessFilter3Dn,IntBMessFilter3[0],IntBMessFilter3[1]);

	switch(IfaceHold)
	{
	case IFACE_INT_CHSLOT: SprMngr.DrawSprite(IntPBSlotsDn,IntBChangeSlot[0],IntBChangeSlot[1]); break;
	case IFACE_INT_INV: SprMngr.DrawSprite(IntPBInvDn,IntBInv[0],IntBInv[1]); break;
	case IFACE_INT_MENU: SprMngr.DrawSprite(IntPBMenuDn,IntBMenu[0],IntBMenu[1]); break;
	case IFACE_INT_SKILL: SprMngr.DrawSprite(IntPBSkillDn,IntBSkill[0],IntBSkill[1]); break;
	case IFACE_INT_MAP: SprMngr.DrawSprite(IntPBMapDn,IntBMap[0],IntBMap[1]); break;
	case IFACE_INT_CHAR: SprMngr.DrawSprite(IntPBChaDn,IntBChar[0],IntBChar[1]); break;
	case IFACE_INT_PIP: SprMngr.DrawSprite(IntPBPipDn,IntBPip[0],IntBPip[1]); break;
	case IFACE_INT_FIX: SprMngr.DrawSprite(IntPBFixDn,IntBFix[0],IntBFix[1]); break;
	case IFACE_INT_ITEM: SprMngr.DrawSprite(IntBItemPicDn,IntBItem[0],IntBItem[1]); break;
	case IFACE_INT_ADDMESS: SprMngr.DrawSprite(IntPBAddMessDn,IntBAddMess[0],IntBAddMess[1]); break;
	case IFACE_INT_FILTER1: SprMngr.DrawSprite(IntPBMessFilter1Dn,IntBMessFilter1[0],IntBMessFilter1[1]); break;
	case IFACE_INT_FILTER2: SprMngr.DrawSprite(IntPBMessFilter2Dn,IntBMessFilter2[0],IntBMessFilter2[1]); break;
	case IFACE_INT_FILTER3: SprMngr.DrawSprite(IntPBMessFilter3Dn,IntBMessFilter3[0],IntBMessFilter3[1]); break;
	case IFACE_INT_COMBAT_TURN: SprMngr.DrawSprite(IntBCombatTurnPicDown,IntBCombatTurn[0],IntBCombatTurn[1]); break;
	case IFACE_INT_COMBAT_END: SprMngr.DrawSprite(IntBCombatEndPicDown,IntBCombatEnd[0],IntBCombatEnd[1]); break;
	default: break;
	}

	if(IsTurnBased && Chosen->IsPerk(MODE_END_COMBAT)) SprMngr.DrawSprite(IntBCombatEndPicDown,IntBCombatEnd[0],IntBCombatEnd[1]);

	// Item offsets
	int item_offsx,item_offsy;
	item_offsx=(IfaceHold==IFACE_INT_ITEM?IntBItemOffsX:0);
	item_offsy=(IfaceHold==IFACE_INT_ITEM?IntBItemOffsY:0);

	// Item
	if(Chosen->ItemSlotMain->GetId())
	{
		SprMngr.DrawSpriteSize(
			ResMngr.GetInvSprId(Chosen->ItemSlotMain->GetPicInv()),
			IntBItem[0]+item_offsx,IntBItem[1]+item_offsy,
			IntBItem[2]-IntBItem[0],IntBItem[3]-IntBItem[1],
			false,true,Chosen->ItemSlotMain->GetInvColor());
	}

	// Use
	SpriteInfo* si=ResMngr.GetIfaceSprInfo(Chosen->GetUsePic(SLOT_HAND1));
	if(si) SprMngr.DrawSprite(ResMngr.GetIfaceSprId(Chosen->GetUsePic(SLOT_HAND1)),IntUseX+item_offsx-si->Width,IntUseY+item_offsy);

	// Aim
	if(Chosen->IsAim()) SprMngr.DrawSprite(IntAimPic,IntAimX+item_offsx,IntAimY+item_offsy);

	// Ap cost
	DWORD ap_cost=Chosen->GetUseApCost(Chosen->ItemSlotMain,Chosen->GetFullRate());
	if(ap_cost)
	{
		SprMngr.DrawSprite(IntWApCostPicNone,IntWApCost[0]+item_offsx,IntWApCost[1]+item_offsy);
		SprMngr.DrawStr(INTRECT(IntWApCost,item_offsx+20,item_offsy),Str::Format("%u",ap_cost),0,COLOR_IFACE_FIX,FONT_SAND_NUM);
	}

	// Ap
	if(Chosen->GetAp()>=0)
	{
		for(int i=0,j=Chosen->GetAp();i<j && i<IntAPMax;i++)
			SprMngr.DrawSprite(IntDiodeG,IntAP[0]+i*IntAPstepX,IntAP[1]+i*IntAPstepY);
	}
	else
	{
		for(int i=0,j=abs(Chosen->GetAp());i<j && i<IntAPMax;i++)
			SprMngr.DrawSprite(IntDiodeR,IntAP[0]+i*IntAPstepX,IntAP[1]+i*IntAPstepY);
	}

	// Move Ap
	if(IsTurnBased)
	{
		for(int i=Chosen->GetAp(),j=Chosen->GetAp()+Chosen->GetParam(ST_MOVE_AP);i<j && i<IntAPMax;i++)
			SprMngr.DrawSprite(IntDiodeY,IntAP[0]+i*IntAPstepX,IntAP[1]+i*IntAPstepY);
	}

	// Break time
	if(!Chosen->IsFree()) SprMngr.DrawSprite(IntBreakTimePic,IntBreakTime[0],IntBreakTime[1]);

	// Chosen mess tabs
	int cur_mess_tab=0;
	if(Chosen->IsPerk(MODE_HIDE)) cur_mess_tab++;
	if(IntMessTabLevelUp) cur_mess_tab++;
	if(Chosen->IsOverweight()) cur_mess_tab++;
	if(Chosen->IsPerk(DAMAGE_POISONED)) cur_mess_tab++;
	if(Chosen->IsPerk(DAMAGE_RADIATED)) cur_mess_tab++;
	if(Chosen->IsInjured()) cur_mess_tab++;
	if(Chosen->IsAddicted()) cur_mess_tab++;
	if(Chosen->GetTimeout(TO_TRANSFER)) cur_mess_tab++;
	if(IsTurnBasedMyTurn()) cur_mess_tab++;
	if(IsTurnBased && !IsTurnBasedMyTurn()) cur_mess_tab++;

	for(int i=0;i<cur_mess_tab;i++)
		SprMngr.DrawSprite(IntMessTabPicNone,IntMessTab[0]+IntMessTabStepX*i,IntMessTab[1]+IntMessTabStepY*i);

	// Chosen mess text
//=============================================
#define INTDRAW_MESSTAB_TEXT(expr,str,color) \
	if(expr)\
	{\
		SprMngr.DrawStr(INTRECT(IntMessTab,IntMessTabStepX*cur_mess_tab,IntMessTabStepY*cur_mess_tab),str,FT_CENTERX|FT_CENTERY,color);\
		cur_mess_tab++;\
	}
//=============================================
	cur_mess_tab=0;
	INTDRAW_MESSTAB_TEXT(Chosen->IsPerk(MODE_HIDE),FmtGameText(STR_HIDEMODE_TITLE),COLOR_TEXT_DGREEN);
	INTDRAW_MESSTAB_TEXT(IntMessTabLevelUp,FmtGameText(STR_LEVELUP_TITLE),COLOR_TEXT_DGREEN);
	INTDRAW_MESSTAB_TEXT(Chosen->IsOverweight(),FmtGameText(STR_OVERWEIGHT_TITLE),COLOR_TEXT_DRED);
	INTDRAW_MESSTAB_TEXT(Chosen->IsPerk(DAMAGE_POISONED),FmtGameText(STR_POISONED_TITLE),COLOR_TEXT_DRED);
	INTDRAW_MESSTAB_TEXT(Chosen->IsPerk(DAMAGE_RADIATED),FmtGameText(STR_RADIATED_TITLE),COLOR_TEXT_DRED);
	INTDRAW_MESSTAB_TEXT(Chosen->IsInjured(),FmtGameText(STR_INJURED_TITLE),COLOR_TEXT_DRED);
	INTDRAW_MESSTAB_TEXT(Chosen->IsAddicted(),FmtGameText(STR_ADDICTED_TITLE),COLOR_TEXT_DRED);
	INTDRAW_MESSTAB_TEXT(Chosen->GetTimeout(TO_TRANSFER),FmtGameText(STR_TIMEOUT_TITLE,Chosen->GetTimeout(TO_TRANSFER)),COLOR_TEXT_DRED);
	INTDRAW_MESSTAB_TEXT(IsTurnBasedMyTurn(),FmtGameText(STR_YOU_TURN_TITLE,GetTurnBasedMyTime()/1000),COLOR_TEXT_DGREEN);
	INTDRAW_MESSTAB_TEXT(IsTurnBased && !IsTurnBasedMyTurn(),FmtGameText(STR_TURN_BASED_TITLE),COLOR_TEXT_DGREEN);

	// Hp
	char bin_str[32];
	sprintf(bin_str,"%c%03d",bin_str[0]='9'+4,abs(Chosen->GetParam(ST_CURRENT_HP)));
	if(Chosen->GetParam(ST_CURRENT_HP)<0) bin_str[0]='9'+3;

	if((Chosen->GetParam(ST_CURRENT_HP)*100)/Chosen->GetParam(ST_MAX_LIFE)<=20)
		Str::ChangeValue(bin_str,0x20); //Red
	else if((Chosen->GetParam(ST_CURRENT_HP)*100)/Chosen->GetParam(ST_MAX_LIFE)<=40) //TODO:
		Str::ChangeValue(bin_str,0x10); //Yellow

	SprMngr.DrawStr(IntHP,bin_str,0,COLOR_IFACE,FONT_NUM);

	// Ac
	SprMngr.DrawStr(IntAC,Str::Format("%c%03d",'9'+4,Chosen->GetParam(ST_ARMOR_CLASS)),0,COLOR_IFACE,FONT_NUM);

	// Indicator
	Item* item=Chosen->ItemSlotMain;
	int indicator_max=item->Proto->IndicatorMax;
	int indicator_cur=item->Data.Indicator;

	if(item->IsWeapon() && item->WeapGetMaxAmmoCount())
	{
		indicator_max=item->WeapGetMaxAmmoCount();
		indicator_cur=item->WeapGetAmmoCount();
	}

	if(indicator_max)
	{
		if(GameOpt.IndicatorType==INDICATOR_LINES || GameOpt.IndicatorType==INDICATOR_BOTH)
			DrawIndicator(IntWAmmoCount,IntAmmoPoints,COLOR_TEXT_GREEN,Procent(indicator_max,indicator_cur),IntAmmoTick,true,false);
		if(GameOpt.IndicatorType==INDICATOR_NUMBERS || GameOpt.IndicatorType==INDICATOR_BOTH)
			SprMngr.DrawStr(INTRECT(IntWAmmoCountStr,item_offsx,item_offsy),Str::Format("%03d",indicator_cur),0,IfaceHold==IFACE_INT_ITEM?COLOR_TEXT_DGREEN:COLOR_TEXT,FONT_SPECIAL);
	}
	else if(GameOpt.IndicatorType==INDICATOR_LINES || GameOpt.IndicatorType==INDICATOR_BOTH)
	{
		DrawIndicator(IntWAmmoCount,IntAmmoPoints,COLOR_TEXT_GREEN,0,IntAmmoTick,true,false);
	}

	// Deteoration indicator
	if(item->IsWeared())
	{
		if(GameOpt.IndicatorType==INDICATOR_LINES || GameOpt.IndicatorType==INDICATOR_BOTH)
			DrawIndicator(IntWWearProcent,IntWearPoints,COLOR_TEXT_RED,item->GetWearProc(),IntWearTick,true,false);
		if(GameOpt.IndicatorType==INDICATOR_NUMBERS || GameOpt.IndicatorType==INDICATOR_BOTH)
			SprMngr.DrawStr(INTRECT(IntWWearProcentStr,item_offsx,item_offsy),Str::Format("%d%%",item->GetWearProc()),0,IfaceHold==IFACE_INT_ITEM?COLOR_TEXT_DRED:COLOR_TEXT_RED,FONT_SPECIAL);
	}
	else if(GameOpt.IndicatorType==INDICATOR_LINES || GameOpt.IndicatorType==INDICATOR_BOTH)
	{
		DrawIndicator(IntWWearProcent,IntWearPoints,COLOR_TEXT_RED,0,IntWearTick,true,false);
	}

}

int FOClient::IntLMouseDown()
{
	IfaceHold=IFACE_NONE;
	if(!Chosen || !IntVisible) return IFACE_NONE;

	if(IsCurInRect(IntBItem,0,0))
	{
		IfaceHold=IFACE_INT_ITEM;
		return IfaceHold;
	}

	//if(!IsCurMode(CUR_DEFAULT) && !IsCurMode(CUR_MOVE)) return 0;

	if(IsCurInRect(IntBChangeSlot)) IfaceHold=IFACE_INT_CHSLOT;
	else if(IsCurInRect(IntBInv)) IfaceHold=IFACE_INT_INV;
	else if(IsCurInRect(IntBMenu)) IfaceHold=IFACE_INT_MENU;
	else if(IsCurInRect(IntBSkill)) IfaceHold=IFACE_INT_SKILL;
	else if(IsCurInRect(IntBMap)) IfaceHold=IFACE_INT_MAP;
	else if(IsCurInRect(IntBChar)) IfaceHold=IFACE_INT_CHAR;
	else if(IsCurInRect(IntBPip)) IfaceHold=IFACE_INT_PIP;
	else if(IsCurInRect(IntBFix)) IfaceHold=IFACE_INT_FIX;
	else if(IsCurInRect(IntBAddMess)) IfaceHold=IFACE_INT_ADDMESS;
	else if(IsCurInRect(IntBMessFilter1)) IfaceHold=IFACE_INT_FILTER1;
	else if(IsCurInRect(IntBMessFilter2)) IfaceHold=IFACE_INT_FILTER2;
	else if(IsCurInRect(IntBMessFilter3)) IfaceHold=IFACE_INT_FILTER3;
	else if(IsCurInRect(IntBCombatTurn) && IsTurnBasedMyTurn()) IfaceHold=IFACE_INT_COMBAT_TURN;
	else if(IsCurInRect(IntBCombatEnd) && IsTurnBased) IfaceHold=IFACE_INT_COMBAT_END;
	else if(IsCurInRectNoTransp(IntMainPic,IntWMain,0,0)) IfaceHold=IFACE_INT_MAIN;
	else if(IntAddMess && IsCurInRectNoTransp(IntPWAddMess,IntWAddMess,0,0)) IfaceHold=IFACE_INT_MAIN;

	return IfaceHold;
}

void FOClient::IntRMouseDown()
{
	IfaceHold=IFACE_NONE;
	if(!Chosen || !IntVisible) return;
	if(IsCurInRect(IntBItem) && Chosen->NextRateItem(false)) Net_SendRateItem();
}

void FOClient::IntLMouseUp()
{
	if(!Chosen || IfaceHold==IFACE_NONE) return;

	if(IfaceHold==IFACE_INT_CHSLOT && IsCurInRect(IntBChangeSlot))
	{
		ChosenChangeSlot();
	}
	else if(IfaceHold==IFACE_INT_INV && IsCurInRect(IntBInv))
	{
		ShowScreen(SCREEN__INVENTORY);
	}
	else if(IfaceHold==IFACE_INT_MENU && IsCurInRect(IntBMenu))
	{
		ShowScreen(SCREEN__MENU_OPTION);
	}
	else if(IfaceHold==IFACE_INT_SKILL && IsCurInRect(IntBSkill))
	{
		SboxUseOn.Clear();
		ShowScreen(SCREEN__SKILLBOX);
	}
	else if(IfaceHold==IFACE_INT_MAP && IsCurInRect(IntBMap))
	{
		ShowScreen(SCREEN__MINI_MAP);
	}
	else if(IfaceHold==IFACE_INT_CHAR && IsCurInRect(IntBChar))
	{
		ShowScreen(SCREEN__CHARACTER);
		if(Chosen->Params[ST_UNSPENT_PERKS]) ShowScreen(SCREEN__PERK);
	}
	else if(IfaceHold==IFACE_INT_PIP && IsCurInRect(IntBPip))
	{
		ShowScreen(SCREEN__PIP_BOY);
	}
	else if(IfaceHold==IFACE_INT_FIX && IsCurInRect(IntBFix))
	{
		ShowScreen(SCREEN__FIX_BOY);
	}
	else if(IfaceHold==IFACE_INT_ITEM && IsCurInRect(IntBItem))
	{
		if(Chosen->GetUse()==USE_RELOAD)
		{
			SetAction(CHOSEN_USE_ITEM,Chosen->ItemSlotMain->GetId(),0,TARGET_SELF_ITEM,0,USE_RELOAD);
		}
		else if(Chosen->GetUse()<MAX_USES && Chosen->ItemSlotMain->IsWeapon())
		{
			SetCurMode(CUR_USE_WEAPON);
		}
		else if(Chosen->GetUse()==USE_USE && Chosen->ItemSlotMain->IsCanUseOnSmth())
		{
			SetCurMode(CUR_USE_ITEM);
		}
		else if(Chosen->GetUse()==USE_USE && Chosen->ItemSlotMain->IsCanUse())
		{
			if(!Chosen->ItemSlotMain->IsHasTimer()) SetAction(CHOSEN_USE_ITEM,Chosen->ItemSlotMain->GetId(),0,TARGET_SELF,0,USE_USE);
			else TimerStart(Chosen->ItemSlotMain->GetId(),ResMngr.GetInvSprId(Chosen->ItemSlotMain->GetPicInv()),Chosen->ItemSlotMain->GetInvColor());
		}
	}
	else if(IfaceHold==IFACE_INT_ADDMESS && IsCurInRect(IntBAddMess))
	{
		IntAddMess=!IntAddMess;
		MessBoxGenerate();
	}
	else if(IfaceHold==IFACE_INT_FILTER1 && IsCurInRect(IntBMessFilter1))
	{
		MessBoxScroll=0;
		IntVecIt it=std::find(MessBoxFilters.begin(),MessBoxFilters.end(),FOMB_COMBAT_RESULT);
		if(it!=MessBoxFilters.end()) MessBoxFilters.erase(it);
		else MessBoxFilters.push_back(FOMB_COMBAT_RESULT);
		MessBoxGenerate();
	}
	else if(IfaceHold==IFACE_INT_FILTER2 && IsCurInRect(IntBMessFilter2))
	{
		MessBoxScroll=0;
		IntVecIt it=std::find(MessBoxFilters.begin(),MessBoxFilters.end(),FOMB_TALK);
		if(it!=MessBoxFilters.end()) MessBoxFilters.erase(it);
		else MessBoxFilters.push_back(FOMB_TALK);
		MessBoxGenerate();
	}
	else if(IfaceHold==IFACE_INT_FILTER3 && IsCurInRect(IntBMessFilter3))
	{
		MessBoxScroll=0;
		IntVecIt it=std::find(MessBoxFilters.begin(),MessBoxFilters.end(),FOMB_VIEW);
		if(it!=MessBoxFilters.end()) MessBoxFilters.erase(it);
		else MessBoxFilters.push_back(FOMB_VIEW);
		MessBoxGenerate();
	}
	else if(IfaceHold==IFACE_INT_COMBAT_TURN && IsCurInRect(IntBCombatTurn) && IsTurnBasedMyTurn())
	{
		Net_SendCombat(COMBAT_TB_END_TURN,1);
		TurnBasedTime=0;
	}
	else if(IfaceHold==IFACE_INT_COMBAT_END && IsCurInRect(IntBCombatEnd) && IsTurnBased)
	{
		if(Chosen->IsPerk(MODE_END_COMBAT)) Net_SendCombat(COMBAT_TB_END_COMBAT,0);
		else Net_SendCombat(COMBAT_TB_END_COMBAT,1);
		Chosen->Params[MODE_END_COMBAT]=!Chosen->Params[MODE_END_COMBAT];
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::IntMouseMove()
{
	
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::AddMess(int mess_type, const char* msg)
{
	if(!msg) return;
	if(mess_type==FOMB_GAME && !strcmp(msg,"error")) return;

	// Text
	const DWORD str_color[]={COLOR_TEXT_DGREEN,COLOR_TEXT,COLOR_TEXT_DRED,COLOR_TEXT_DDGREEN};
	static char str[MAX_FOTEXT];
	if(mess_type<0 || mess_type>FOMB_VIEW) sprintf_s(str,MAX_FOTEXT,"%s\n",msg);
	else sprintf_s(str,MAX_FOTEXT,"|%u %c |%u %s\n",str_color[mess_type],TEXT_SYMBOL_DOT,COLOR_TEXT,msg);

	// Time
	SYSTEMTIME sys_time;
	GetLocalTime(&sys_time);
	char mess_time[64];
	sprintf(mess_time,"%02d:%02d:%02d ",sys_time.wHour,sys_time.wMinute,sys_time.wSecond);

	// Add
	MessBox.push_back(MessBoxMessage(mess_type,str,mess_time));

	// Generate mess box
	if(std::find(MessBoxFilters.begin(),MessBoxFilters.end(),mess_type)==MessBoxFilters.end())
	{
		if(MessBoxScroll && IsCurInRect(MessBoxCurRectScroll())) MessBoxScroll++;
		else MessBoxScroll=0;
	}
	MessBoxGenerate();
}

void FOClient::MessBoxGenerate()
{
	MessBoxCurText="";
	if(MessBox.empty()) return;

	INTRECT ir=MessBoxCurRectDraw();
	int max_lines=SprMngr.GetLinesCount(0,ir.H(),NULL);

	if(ir.IsZero() || max_lines<=0)
	{
		MessBoxMaxScroll=0;
		MessBoxScrollLines=0;
		return;
	}

	MessBoxScrollLines=-1;
	bool check_filters=IsMainScreen(SCREEN_GAME);
	int cur_mess=(int)MessBox.size()-1;
	int lines=0;
	for(;cur_mess>=0;cur_mess--)
	{
		MessBoxMessage& m=MessBox[cur_mess];

		// Filters
		if(check_filters && std::find(MessBoxFilters.begin(),MessBoxFilters.end(),m.Type)!=MessBoxFilters.end()) continue;

		// Skip scrolled lines
		int lines_=lines;
		lines+=SprMngr.GetLinesCount(ir.W(),0,m.Mess.c_str());
		if(MessBoxScrollLines<0)
		{
			if(lines<=MessBoxScroll) continue;
			MessBoxScrollLines=MessBoxScroll-lines_;
		}

		if(lines_-MessBoxScroll<max_lines)
		{
			// Add to message box
			if(GameOpt.MsgboxInvert) MessBoxCurText+=m.Mess; // Back
			else MessBoxCurText=m.Mess+MessBoxCurText; // Front
		}
		else break;
	}
	MessBoxMaxScroll=lines-max_lines;
	if(MessBoxScrollLines<0) MessBoxScrollLines=0;
}

void FOClient::MessBoxDraw()
{
	if(MessBoxCurText.empty()) return;

	DWORD flags=FT_COLORIZE;
	if(!GameOpt.MsgboxInvert) flags|=FT_UPPER|FT_BOTTOM;

	INTRECT ir=MessBoxCurRectDraw();
	if(ir.IsZero()) return;

	SprMngr.DrawStr(ir,MessBoxCurText.c_str(),flags|(GameOpt.MsgboxInvert?FT_SKIPLINES(MessBoxScrollLines):FT_SKIPLINES_END(MessBoxScrollLines)));
}

INTRECT FOClient::MessBoxCurRectDraw()
{
	static INTRECT r(0,0,0,0);

	if(IsMainScreen(SCREEN_LOGIN)) return LogWChat;
	else if(IsMainScreen(SCREEN_REGISTRATION)) return r(0,0,MODE_WIDTH,60);
	else if(IsMainScreen(SCREEN_GLOBAL_MAP)) return GmapWChat;
	else if(IsMainScreen(SCREEN_GAME) && IntVisible && !IsScreenPresent(SCREEN__TOWN_VIEW))
	{
		if(IntAddMess) return IntWMessLarge;
		else return IntWMess;
	}

	return r(0,0,0,0);
}

INTRECT FOClient::MessBoxCurRectScroll()
{
	if(IsCurMode(CUR_WAIT) || IsLMenu()) return INTRECT(0,0,0,0);

	if(GetActiveScreen()==SCREEN_NONE)
	{
		if(IsMainScreen(SCREEN_LOGIN)) return LogWChat;
		else if(IsMainScreen(SCREEN_REGISTRATION)) return INTRECT(0,0,MODE_WIDTH,60);
		else if(IsMainScreen(SCREEN_GLOBAL_MAP)) return GmapWChat;
		else if(IsMainScreen(SCREEN_GAME) && IntVisible && !IsScreenPresent(SCREEN__TOWN_VIEW))
		{
			if(IntAddMess) return IntWMessLarge;
			else return IntWMess;
		}
	}

	return INTRECT(0,0,0,0);
}

bool FOClient::MessBoxLMouseDown()
{
	INTRECT rmb=MessBoxCurRectScroll();
	if(!rmb.IsZero() && IsCurInRect(rmb))
	{
		if(IsCurInRect(INTRECT(rmb.L,rmb.T,rmb.R,rmb.CY())))
		{
			if(GameOpt.MsgboxInvert && MessBoxScroll>0) MessBoxScroll--;
			if(!GameOpt.MsgboxInvert && MessBoxScroll<MessBoxMaxScroll) MessBoxScroll++;
			MessBoxGenerate();
		}
		else
		{
			if(GameOpt.MsgboxInvert && MessBoxScroll<MessBoxMaxScroll) MessBoxScroll++;
			if(!GameOpt.MsgboxInvert && MessBoxScroll>0) MessBoxScroll--;
			MessBoxGenerate();
		}
		return true;
	}
	return false;
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::LogDraw()
{
	SprMngr.DrawSprite(Singleplayer?LogPSingleplayerMain:LogPMain,LogX,LogY);

	if(IfaceHold==IFACE_LOG_PLAY__NEWGAME) SprMngr.DrawSprite(LogPBLogin,LogBOk[0],LogBOk[1]);
	if(IfaceHold==IFACE_LOG_REG__LOADGAME) SprMngr.DrawSprite(LogPBReg,LogBReg[0],LogBReg[1]);
	if(IfaceHold==IFACE_LOG_OPTIONS) SprMngr.DrawSprite(LogPBOptions,LogBOptions[0],LogBOptions[1]);
	if(IfaceHold==IFACE_LOG_CREDITS) SprMngr.DrawSprite(LogPBCredits,LogBCredits[0],LogBCredits[1]);
	if(IfaceHold==IFACE_LOG_EXIT) SprMngr.DrawSprite(LogPBExit,LogBExit[0],LogBExit[1]);

	SprMngr.DrawStr(LogBOkText,MsgGame->GetStr(Singleplayer?STR_LOGIN_NEWGAME:STR_LOGIN_PLAY),FT_CENTERY|FT_NOBREAK,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(LogBRegText,MsgGame->GetStr(Singleplayer?STR_LOGIN_LOADGAME:STR_LOGIN_REGISTRATION),FT_CENTERY|FT_NOBREAK,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(LogBOptionsText,MsgGame->GetStr(STR_LOGIN_OPTIONS),FT_CENTERY|FT_NOBREAK,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(LogBCreditsText,MsgGame->GetStr(STR_LOGIN_CREDITS),FT_CENTERY|FT_NOBREAK,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(LogBExitText,MsgGame->GetStr(STR_LOGIN_EXIT),FT_CENTERY|FT_NOBREAK,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(LogWVersion,MsgGame->GetStr(STR_VERSION_INFO),FT_CENTERY,COLOR_TEXT_WHITE,FONT_DEFAULT);

	if(!Singleplayer)
	{
		SprMngr.DrawStr(LogWName,GameOpt.Name.c_str(),FT_CENTERX|FT_CENTERY|FT_NOBREAK,LogFocus==IFACE_LOG_NAME?COLOR_TEXT_LGREEN:COLOR_TEXT_DGREEN);

		if(Keyb::CtrlDwn)
		{
			SprMngr.DrawStr(LogWPass,GameOpt.Pass.c_str(),FT_CENTERX|FT_CENTERY|FT_NOBREAK,LogFocus==IFACE_LOG_PASS?COLOR_TEXT_LGREEN:COLOR_TEXT_DGREEN);
		}
		else
		{
			char mask[MAX_NAME+1];
			for(int i=0,j=min(MAX_NAME,GameOpt.Pass.length());i<j;i++) mask[i]='#';
			mask[min(MAX_NAME,GameOpt.Pass.length())]='\0';
			SprMngr.DrawStr(LogWPass,mask,FT_CENTERX|FT_CENTERY|FT_NOBREAK,LogFocus==IFACE_LOG_PASS?COLOR_TEXT_LGREEN:COLOR_TEXT_DGREEN);
		}
	}
}

void FOClient::LogKeyDown(BYTE dik)
{
	if(Singleplayer) return;

	if(dik==DIK_RETURN || dik==DIK_NUMPADENTER)
	{
		LogTryConnect();
		return;
	}

	if(dik==DIK_TAB)
	{
		if(LogFocus==IFACE_LOG_NAME) LogFocus=IFACE_LOG_PASS;
		else if(LogFocus==IFACE_LOG_PASS) LogFocus=IFACE_LOG_NAME;
		return;
	}

	if(LogFocus==IFACE_LOG_NAME)
	{
		Keyb::GetChar(dik,GameOpt.Name,NULL,min(GameOpt.MaxNameLength,MAX_NAME),KIF_NO_SPEC_SYMBOLS);
	}
	else if(LogFocus==IFACE_LOG_PASS)
	{
		Keyb::GetChar(dik,GameOpt.Pass,NULL,min(GameOpt.MaxNameLength,MAX_NAME),KIF_NO_SPEC_SYMBOLS);
	}
}

void FOClient::LogLMouseDown()
{
	IfaceHold=IFACE_NONE;
	LogFocus=IFACE_NONE;

	if(IsCurInRect(LogWName)) IfaceHold=(LogFocus=IFACE_LOG_NAME);
	else if(IsCurInRect(LogWPass)) IfaceHold=(LogFocus=IFACE_LOG_PASS);
	else if(IsCurInRect(LogBOk)) IfaceHold=IFACE_LOG_PLAY__NEWGAME;
	else if(IsCurInRect(LogBReg)) IfaceHold=IFACE_LOG_REG__LOADGAME;
	else if(IsCurInRect(LogBOptions)) IfaceHold=IFACE_LOG_OPTIONS;
	else if(IsCurInRect(LogBCredits)) IfaceHold=IFACE_LOG_CREDITS;
	else if(IsCurInRect(LogBExit)) IfaceHold=IFACE_LOG_EXIT;
}

void FOClient::LogLMouseUp()
{
	if(IfaceHold==IFACE_LOG_PLAY__NEWGAME && IsCurInRect(LogBOk))
	{
		if(!Singleplayer) LogTryConnect();
		else ShowMainScreen(SCREEN_REGISTRATION);
	}
	else if(IfaceHold==IFACE_LOG_REG__LOADGAME && IsCurInRect(LogBReg))
	{
		if(!Singleplayer) ShowMainScreen(SCREEN_REGISTRATION);
		else
		{
			SaveLoadLoginScreen=true;
			SaveLoadSave=false;
			ShowScreen(SCREEN__SAVE_LOAD);
		}
	}
	else if(IfaceHold==IFACE_LOG_OPTIONS && IsCurInRect(LogBOptions))
	{
		AddMess(FOMB_GAME,MsgGame->GetStr(STR_OPTIONS_NOT_AVIABLE));
	}
	else if(IfaceHold==IFACE_LOG_CREDITS && IsCurInRect(LogBCredits))
	{
		ShowMainScreen(SCREEN_CREDITS);
	}
	else if(IfaceHold==IFACE_LOG_EXIT && IsCurInRect(LogBExit))
	{
		TryExit();
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::LogTryConnect()
{
	if(!Singleplayer)
	{
		while(!GameOpt.Name.empty() && GameOpt.Name[0]==' ') GameOpt.Name.erase(0,1);
		while(!GameOpt.Name.empty() && GameOpt.Name[GameOpt.Name.length()-1]==' ') GameOpt.Name.erase(GameOpt.Name.length()-1,1);

		if(GameOpt.Name.length()<MIN_NAME || GameOpt.Name.length()<GameOpt.MinNameLength ||
			GameOpt.Name.length()>MAX_NAME || GameOpt.Name.length()>GameOpt.MaxNameLength)
		{
			AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_WRONG_LOGIN));
			return;
		}

		if(!CheckUserName(GameOpt.Name.c_str()))
		{
			AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_NAME_WRONG_CHARS));
			return;
		}

		if(GameOpt.Pass.length()<MIN_NAME || GameOpt.Pass.length()<GameOpt.MinNameLength ||
			GameOpt.Pass.length()>MAX_NAME || GameOpt.Pass.length()>GameOpt.MaxNameLength)
		{
			AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_WRONG_PASS));
			return;
		}

		if(!CheckUserPass(GameOpt.Pass.c_str()))
		{
			AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_PASS_WRONG_CHARS));
			return;
		}

		// Save login and password
		Crypt.SetCache("__name",(BYTE*)GameOpt.Name.c_str(),GameOpt.Name.length()+1);
		Crypt.SetCache("__pass",(BYTE*)GameOpt.Pass.c_str(),GameOpt.Pass.length()+1);

		AddMess(FOMB_GAME,MsgGame->GetStr(STR_NET_CONNECTION));
	}

	// Connect to server
	NetState=STATE_INIT_NET;
	InitNetReason=INIT_NET_REASON_LOGIN;
	SAFEDEL(RegNewCr);
	ScreenEffects.clear();
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::DlgDraw(bool is_dialog)
{
	SprMngr.DrawSprite(DlgPMain,DlgX,DlgY);
	if(!BarterIsPlayers && DlgAvatarSprId) SprMngr.DrawSpriteSize(DlgAvatarSprId,DlgWAvatar.L+DlgX,DlgWAvatar.T+DlgY,DlgWAvatar.W(),DlgWAvatar.H(),true,true);
	if(!Chosen) return;

	// Main pic
	if(is_dialog) SprMngr.DrawSprite(DlgPAnsw,DlgAnsw[0]+DlgX,DlgAnsw[1]+DlgY);
	else SprMngr.DrawSprite(BarterPMain,BarterWMain[0]+DlgX,BarterWMain[1]+DlgY);

	// Text scroll
	const char scr_up[]={(char)TEXT_SYMBOL_UP,0};
	const char scr_down[]={(char)TEXT_SYMBOL_DOWN,0};
	if(DlgMainTextCur) SprMngr.DrawStr(INTRECT(DlgBScrUp,DlgX,DlgY),scr_up,0,IfaceHold==IFACE_DLG_SCR_UP?COLOR_TEXT_DGREEN:COLOR_TEXT);
	if(DlgMainTextCur<DlgMainTextLinesReal-DlgMainTextLinesRect) SprMngr.DrawStr(INTRECT(DlgBScrDn,DlgX,DlgY),scr_down,0,IfaceHold==IFACE_DLG_SCR_DN?COLOR_TEXT_DGREEN:COLOR_TEXT);

	// Dialog
	if(is_dialog)
	{
		switch(IfaceHold)
		{
		case IFACE_DLG_BARTER: SprMngr.DrawSprite(DlgPBBarter,DlgBBarter[0]+DlgX,DlgBBarter[1]+DlgY); break;
		case IFACE_DLG_SAY: SprMngr.DrawSprite(DlgPBSay,DlgBSay[0]+DlgX,DlgBSay[1]+DlgY); break;
		default: break;
		}

		// Texts
		SprMngr.DrawStr(INTRECT(DlgBBarterText,DlgX,DlgY),MsgGame->GetStr(STR_DIALOG_BARTER),FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
		SprMngr.DrawStr(INTRECT(DlgBSayText,DlgX,DlgY),MsgGame->GetStr(STR_DIALOG_SAY),FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);

		// Npc text
		SprMngr.DrawStr(INTRECT(DlgWText,DlgX,DlgY),DlgMainText.c_str(),FT_SKIPLINES(DlgMainTextCur),COLOR_TEXT);

		// Answers
		for(int i=0;i<DlgAnswers.size();i++)
		{
			Answer& a=DlgAnswers[i];
			if(i==DlgCurAnsw)
				SprMngr.DrawStr(INTRECT(a.Position,DlgX,DlgY),DlgAnswers[i].Text.c_str(),a.AnswerNum<0?FT_CENTERX:0,IfaceHold==IFACE_DLG_ANSWER && DlgCurAnsw==DlgHoldAnsw?COLOR_TEXT_DDGREEN:(IfaceHold!=IFACE_DLG_ANSWER?COLOR_TEXT_DGREEN:COLOR_TEXT));
			else
				SprMngr.DrawStr(INTRECT(a.Position,DlgX,DlgY),DlgAnswers[i].Text.c_str(),a.AnswerNum<0?FT_CENTERX:0,COLOR_TEXT);
		}

		// Chosen money
		if(Chosen) SprMngr.DrawStr(INTRECT(DlgWMoney,DlgX,DlgY),Chosen->GetMoneyStr(),FT_CENTERX|FT_CENTERY,COLOR_TEXT_WHITE);
	}
	// Barter
	else
	{
		if(BarterIsPlayers && BarterOffer) SprMngr.DrawSprite(BarterPBOfferDn,BarterBOffer[0]+DlgX,BarterBOffer[1]+DlgY);

		switch(IfaceHold)
		{
		case IFACE_BARTER_OFFER: SprMngr.DrawSprite(BarterPBOfferDn,BarterBOffer[0]+DlgX,BarterBOffer[1]+DlgY); break;
		case IFACE_BARTER_TALK: SprMngr.DrawSprite(BarterPBTalkDn,BarterBTalk[0]+DlgX,BarterBTalk[1]+DlgY); break;
		case IFACE_BARTER_CONT1SU: SprMngr.DrawSprite(BarterPBC1ScrUpDn,BarterBCont1ScrUp[0]+DlgX,BarterBCont1ScrUp[1]+DlgY); break;
		case IFACE_BARTER_CONT1SD: SprMngr.DrawSprite(BarterPBC1ScrDnDn,BarterBCont1ScrDn[0]+DlgX,BarterBCont1ScrDn[1]+DlgY); break;
		case IFACE_BARTER_CONT2SU: SprMngr.DrawSprite(BarterPBC2ScrUpDn,BarterBCont2ScrUp[0]+DlgX,BarterBCont2ScrUp[1]+DlgY); break;
		case IFACE_BARTER_CONT2SD: SprMngr.DrawSprite(BarterPBC2ScrDnDn,BarterBCont2ScrDn[0]+DlgX,BarterBCont2ScrDn[1]+DlgY); break;
		case IFACE_BARTER_CONT1OSU: SprMngr.DrawSprite(BarterPBC1oScrUpDn,BarterBCont1oScrUp[0]+DlgX,BarterBCont1oScrUp[1]+DlgY); break;
		case IFACE_BARTER_CONT1OSD: SprMngr.DrawSprite(BarterPBC1oScrDnDn,BarterBCont1oScrDn[0]+DlgX,BarterBCont1oScrDn[1]+DlgY); break;
		case IFACE_BARTER_CONT2OSU: SprMngr.DrawSprite(BarterPBC2oScrUpDn,BarterBCont2oScrUp[0]+DlgX,BarterBCont2oScrUp[1]+DlgY); break;
		case IFACE_BARTER_CONT2OSD: SprMngr.DrawSprite(BarterPBC2oScrDnDn,BarterBCont2oScrDn[0]+DlgX,BarterBCont2oScrDn[1]+DlgY); break;
		default: break;
		}

		Chosen->DrawStay(INTRECT(BarterWChosen,DlgX,DlgY));
		CritterCl* cr=GetCritter(BarterIsPlayers?BarterOpponentId:DlgNpcId);
		if(cr) cr->DrawStay(INTRECT(BarterWCritter,DlgX,DlgY));

		SprMngr.Flush();

		// Items
		ContainerDraw(INTRECT(BarterWCont1,DlgX,DlgY),BarterCont1HeightItem,BarterScroll1,BarterCont1,IfaceHold==IFACE_BARTER_CONT1?BarterHoldId:0);
		ContainerDraw(INTRECT(BarterWCont2,DlgX,DlgY),BarterCont2HeightItem,BarterScroll2,BarterCont2,IfaceHold==IFACE_BARTER_CONT2?BarterHoldId:0);
		ContainerDraw(INTRECT(BarterWCont1o,DlgX,DlgY),BarterCont1oHeightItem,BarterScroll1o,BarterCont1o,IfaceHold==IFACE_BARTER_CONT1O?BarterHoldId:0);
		ContainerDraw(INTRECT(BarterWCont2o,DlgX,DlgY),BarterCont2oHeightItem,BarterScroll2o,BarterCont2o,IfaceHold==IFACE_BARTER_CONT2O?BarterHoldId:0);

		// Info
		SprMngr.DrawStr(INTRECT(BarterBOfferText,DlgX,DlgY),MsgGame->GetStr(STR_BARTER_OFFER),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
		if(BarterIsPlayers)
		{
			SprMngr.DrawStr(INTRECT(BarterBTalkText,DlgX,DlgY),MsgGame->GetStr(STR_BARTER_END),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
			SprMngr.DrawStr(INTRECT(DlgWText,DlgX,DlgY),BarterText.c_str(),FT_COLORIZE|FT_UPPER);
		}
		else
		{
			SprMngr.DrawStr(INTRECT(BarterBTalkText,DlgX,DlgY),MsgGame->GetStr(STR_BARTER_TALK),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
			SprMngr.DrawStr(INTRECT(DlgWText,DlgX,DlgY),BarterText.c_str(),FT_COLORIZE);
		}

		// Cost
		DWORD c1,w1,v1,c2,w2,v2;
		ContainerCalcInfo(BarterCont1o,c1,w1,v1,-BarterK,true);
		ContainerCalcInfo(BarterCont2o,c2,w2,v2,Chosen->IsPerk(PE_MASTER_TRADER)?0:BarterK,false);
		if(!BarterIsPlayers && BarterK)
		{
			SprMngr.DrawStr(INTRECT(BarterWCost1,DlgX,DlgY),Str::Format("$%u",c1),FT_NOBREAK|FT_CENTERX,COLOR_TEXT_WHITE);//BarterCost1<BarterCost2?COLOR_TEXT_RED:COLOR_TEXT_WHITE);
			SprMngr.DrawStr(INTRECT(BarterWCost2,DlgX,DlgY),Str::Format("$%u",c2),FT_NOBREAK|FT_CENTERX,COLOR_TEXT_WHITE);//BarterCost1<BarterCost2?COLOR_TEXT_RED:COLOR_TEXT_WHITE);
		}
		else
		{
			SprMngr.DrawStr(INTRECT(BarterWCost1,DlgX,DlgY),Str::Format("%u",w1/1000),FT_NOBREAK|FT_CENTERX,COLOR_TEXT_WHITE);//BarterCost1<BarterCost2?COLOR_TEXT_RED:COLOR_TEXT_WHITE);
			SprMngr.DrawStr(INTRECT(BarterWCost2,DlgX,DlgY),Str::Format("%u",w2/1000),FT_NOBREAK|FT_CENTERX,COLOR_TEXT_WHITE);//BarterCost1<BarterCost2?COLOR_TEXT_RED:COLOR_TEXT_WHITE);
		}
		// Overweight, oversize indicate
		if(Chosen->GetFreeWeight()+w1<(int)w2) SprMngr.DrawStr(INTRECT(DlgWText.L,DlgWText.B-5,DlgWText.R,DlgWText.B+10,DlgX,DlgY),MsgGame->GetStr(STR_OVERWEIGHT_TITLE),FT_NOBREAK|FT_CENTERX,COLOR_TEXT_DDGREEN);
		else if(Chosen->GetFreeVolume()+v1<(int)v2) SprMngr.DrawStr(INTRECT(DlgWText.L,DlgWText.B-5,DlgWText.R,DlgWText.B+10,DlgX,DlgY),MsgGame->GetStr(STR_OVERVOLUME_TITLE),FT_NOBREAK|FT_CENTERX,COLOR_TEXT_DDGREEN);
	}

	// Timer
	if(!BarterIsPlayers && DlgEndTick>Timer::GameTick())
		SprMngr.DrawStr(INTRECT(DlgWTimer,DlgX,DlgY),Str::Format("%u",(DlgEndTick-Timer::GameTick())/1000),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_DGREEN);
}

void FOClient::DlgLMouseDown(bool is_dialog)
{
	IfaceHold=IFACE_NONE;
	BarterHoldId=0;

	if(!IsCurInRect(DlgWMain,DlgX,DlgY)) return;

	if(IsCurInRect(DlgBScrUp,DlgX,DlgY))
	{
		if(DlgMainTextCur>0)
		{
			IfaceHold=IFACE_DLG_SCR_UP;
			Timer::StartAccelerator(ACCELERATE_DLG_TEXT_UP);
		}
	}
	else if(IsCurInRect(DlgBScrDn,DlgX,DlgY))
	{
		if(DlgMainTextCur<DlgMainTextLinesReal-DlgMainTextLinesRect)
		{
			IfaceHold=IFACE_DLG_SCR_DN;
			Timer::StartAccelerator(ACCELERATE_DLG_TEXT_DOWN);
		}
	}

	// Dialog
	if(is_dialog && IsCurInRect(DlgWMain,DlgX,DlgY))
	{
		if(IsCurInRect(DlgAnswText,DlgX,DlgY))
		{
			IfaceHold=IFACE_DLG_ANSWER;
			DlgHoldAnsw=DlgCurAnsw;
		}
		else if(IsCurInRect(DlgBBarter,DlgX,DlgY)) IfaceHold=IFACE_DLG_BARTER;
		else if(IsCurInRect(DlgBSay,DlgX,DlgY)) IfaceHold=IFACE_DLG_SAY;
	}
	// Barter
	else if(!is_dialog && IsCurInRect(BarterWMain,DlgX,DlgY))
	{
		if(IsCurInRect(BarterWCont1,DlgX,DlgY))
		{
			BarterHoldId=GetCurContainerItemId(INTRECT(BarterWCont1,DlgX,DlgY),BarterCont1HeightItem,BarterScroll1,BarterCont1);
			if(!BarterHoldId) return;
			IfaceHold=IFACE_BARTER_CONT1;
		}
		else if(IsCurInRect(BarterWCont2,DlgX,DlgY))
		{
			if(!(BarterIsPlayers && BarterOpponentHide))
			{
				BarterHoldId=GetCurContainerItemId(INTRECT(BarterWCont2,DlgX,DlgY),BarterCont2HeightItem,BarterScroll2,BarterCont2);
				if(!BarterHoldId) return;
				IfaceHold=IFACE_BARTER_CONT2;
			}
		}
		else if(IsCurInRect(BarterWCont1o,DlgX,DlgY))
		{
			BarterHoldId=GetCurContainerItemId(INTRECT(BarterWCont1o,DlgX,DlgY),BarterCont1oHeightItem,BarterScroll1o,BarterCont1o);
			if(!BarterHoldId) return;
			IfaceHold=IFACE_BARTER_CONT1O;
		}
		else if(IsCurInRect(BarterWCont2o,DlgX,DlgY))
		{
			if(!(BarterIsPlayers && BarterOpponentHide))
			{
				BarterHoldId=GetCurContainerItemId(INTRECT(BarterWCont2o,DlgX,DlgY),BarterCont2oHeightItem,BarterScroll2o,BarterCont2o);
				if(!BarterHoldId) return;
				IfaceHold=IFACE_BARTER_CONT2O;
			}
		}
		else if(IsCurInRect(BarterBOffer,DlgX,DlgY)) IfaceHold=IFACE_BARTER_OFFER;
		else if(IsCurInRect(BarterBTalk,DlgX,DlgY)) IfaceHold=IFACE_BARTER_TALK;
		else if(IsCurInRect(BarterBCont1ScrUp,DlgX,DlgY))
		{
			Timer::StartAccelerator(ACCELERATE_BARTER_CONT1SU);
			IfaceHold=IFACE_BARTER_CONT1SU;
		}
		else if(IsCurInRect(BarterBCont1ScrDn,DlgX,DlgY))
		{
			Timer::StartAccelerator(ACCELERATE_BARTER_CONT1SD);
			IfaceHold=IFACE_BARTER_CONT1SD;
		}
		else if(IsCurInRect(BarterBCont2ScrUp,DlgX,DlgY))
		{
			Timer::StartAccelerator(ACCELERATE_BARTER_CONT2SU);
			IfaceHold=IFACE_BARTER_CONT2SU;
		}
		else if(IsCurInRect(BarterBCont2ScrDn,DlgX,DlgY))
		{
			Timer::StartAccelerator(ACCELERATE_BARTER_CONT2SD);
			IfaceHold=IFACE_BARTER_CONT2SD;
		}
		else if(IsCurInRect(BarterBCont1oScrUp,DlgX,DlgY))
		{
			Timer::StartAccelerator(ACCELERATE_BARTER_CONT1OSU);
			IfaceHold=IFACE_BARTER_CONT1OSU;
		}
		else if(IsCurInRect(BarterBCont1oScrDn,DlgX,DlgY))
		{
			Timer::StartAccelerator(ACCELERATE_BARTER_CONT1OSD);
			IfaceHold=IFACE_BARTER_CONT1OSD;
		}
		else if(IsCurInRect(BarterBCont2oScrUp,DlgX,DlgY))
		{
			Timer::StartAccelerator(ACCELERATE_BARTER_CONT2OSU);
			IfaceHold=IFACE_BARTER_CONT2OSU;
		}
		else if(IsCurInRect(BarterBCont2oScrDn,DlgX,DlgY))
		{
			Timer::StartAccelerator(ACCELERATE_BARTER_CONT2OSD);
			IfaceHold=IFACE_BARTER_CONT2OSD;
		}
		else if(IsCurInRect(DlgBScrUp,DlgX,DlgY) && BarterIsPlayers) IfaceHold=IFACE_DLG_SCR_UP;
		else if(IsCurInRect(DlgBScrDn,DlgX,DlgY) && BarterIsPlayers) IfaceHold=IFACE_DLG_SCR_DN;

		if(IsCurMode(CUR_DEFAULT) && (IfaceHold==IFACE_BARTER_CONT1 || IfaceHold==IFACE_BARTER_CONT2 || IfaceHold==IFACE_BARTER_CONT1O || IfaceHold==IFACE_BARTER_CONT2O))
		{
			IfaceHold=IFACE_NONE;
			LMenuTryActivate();
			return;
		}
	}

	if(IfaceHold==IFACE_NONE)
	{
		DlgVectX=CurX-DlgX;
		DlgVectY=CurY-DlgY;
		IfaceHold=IFACE_DLG_MAIN;
	}
}

void FOClient::DlgLMouseUp(bool is_dialog)
{
	if(!Chosen) return;

	if(IfaceHold==IFACE_DLG_SCR_UP && IsCurInRect(DlgBScrUp,DlgX,DlgY))
	{
		if(DlgMainTextCur>0) DlgMainTextCur--;
	}
	else if(IfaceHold==IFACE_DLG_SCR_DN && IsCurInRect(DlgBScrDn,DlgX,DlgY))
	{
		if(DlgMainTextCur<DlgMainTextLinesReal-DlgMainTextLinesRect) DlgMainTextCur++;
	}

	if(is_dialog)
	{
		if(IfaceHold==IFACE_DLG_ANSWER && IsCurInRect(DlgAnswText,DlgX,DlgY))
		{
			if(DlgCurAnsw==DlgHoldAnsw && DlgCurAnsw>=0 && DlgCurAnsw<DlgAnswers.size() && IsCurInRect(DlgAnswers[DlgCurAnsw].Position,DlgX,DlgY))
			{
				if(DlgAnswers[DlgCurAnsw].AnswerNum<0)
				{
					DlgCollectAnswers(DlgAnswers[DlgCurAnsw].AnswerNum==-2);
				}
				else
				{
					Net_SendTalk(DlgIsNpc,DlgNpcId,DlgAnswers[DlgCurAnsw].AnswerNum);
					WaitPing();
				}
			}
		}
		else if(IfaceHold==IFACE_DLG_BARTER && IsCurInRect(DlgBBarter,DlgX,DlgY))
		{
			CritterCl* cr=GetCritter(DlgNpcId);
			if(!cr || cr->IsPerk(MODE_NO_BARTER))
			{
				DlgMainText=MsgGame->GetStr(STR_BARTER_NO_BARTER_MODE);
				DlgMainTextCur=0;
				DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,DlgMainText.c_str());
			}
			else
			{
				Net_SendTalk(DlgIsNpc,DlgNpcId,ANSWER_BARTER);
				WaitPing();
				HideScreen(SCREEN__DIALOG);
				ShowScreen(SCREEN__BARTER);
				CollectContItems();
				BarterCont1o.clear();
				BarterCont2.clear();
				BarterCont2o.clear();
				BarterText="";
				DlgMainTextCur=0;
				DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,BarterText.c_str());
			}
		}
		else if(IfaceHold==IFACE_DLG_SAY && IsCurInRect(DlgBSay,DlgX,DlgY))
		{
			ShowScreen(SCREEN__SAY);
			SayType=DIALOGSAY_TEXT;
			SayText[0]=0;
		}
	}
	else
	{
		if(IfaceHold==IFACE_BARTER_CONT1)
		{
			if(IsCurInRect(BarterWCont1o,DlgX,DlgY))
			{
				ItemVecIt it=std::find(BarterCont1.begin(),BarterCont1.end(),BarterHoldId);
				if(it!=BarterCont1.end())
				{
					Item& item=*it;
					if(item.GetCount()>1)
						SplitStart(&item,IFACE_BARTER_CONT1);
					else
						BarterTransfer(BarterHoldId,IFACE_BARTER_CONT1,item.GetCount());
				}
			}
		}
		else if(IfaceHold==IFACE_BARTER_CONT2)
		{
			if(IsCurInRect(BarterWCont2o,DlgX,DlgY) && !(BarterIsPlayers && BarterOpponentHide))
			{
				ItemVecIt it=std::find(BarterCont2.begin(),BarterCont2.end(),BarterHoldId);
				if(it!=BarterCont2.end())
				{
					Item& item=*it;
					if(item.GetCount()>1)
						SplitStart(&item,IFACE_BARTER_CONT2);
					else
						BarterTransfer(BarterHoldId,IFACE_BARTER_CONT2,item.GetCount());
				}
			}
		}
		else if(IfaceHold==IFACE_BARTER_CONT1O)
		{
			if(IsCurInRect(BarterWCont1,DlgX,DlgY))
			{
				ItemVecIt it=std::find(BarterCont1o.begin(),BarterCont1o.end(),BarterHoldId);
				if(it!=BarterCont1o.end())
				{
					Item& item=*it;
					if(item.GetCount()>1)
						SplitStart(&item,IFACE_BARTER_CONT1O);
					else
						BarterTransfer(BarterHoldId,IFACE_BARTER_CONT1O,item.GetCount());
				}
			}
		}
		else if(IfaceHold==IFACE_BARTER_CONT2O)
		{
			if(IsCurInRect(BarterWCont2,DlgX,DlgY) && !(BarterIsPlayers && BarterOpponentHide))
			{
				ItemVecIt it=std::find(BarterCont2o.begin(),BarterCont2o.end(),BarterHoldId);
				if(it!=BarterCont2o.end())
				{
					Item& item=*it;
					if(item.GetCount()>1)
						SplitStart(&item,IFACE_BARTER_CONT2O);
					else
						BarterTransfer(BarterHoldId,IFACE_BARTER_CONT2O,item.GetCount());
				}
			}
		}
		else if(IfaceHold==IFACE_BARTER_OFFER && IsCurInRect(BarterBOffer,DlgX,DlgY))
		{
			BarterTryOffer();
		}
		else if(IfaceHold==IFACE_BARTER_TALK && IsCurInRect(BarterBTalk,DlgX,DlgY))
		{
			if(BarterIsPlayers)
			{
				Net_SendPlayersBarter(BARTER_END,0,0);
				ShowScreen(SCREEN_NONE);
			}
			else
			{
				Net_SendTalk(DlgIsNpc,DlgNpcId,ANSWER_BEGIN);
				WaitPing();
			}
			CollectContItems();
		}
		else if(IfaceHold==IFACE_BARTER_CONT1SU && IsCurInRect(BarterBCont1ScrUp,DlgX,DlgY))
		{
			if(BarterScroll1>0) BarterScroll1--;
		}
		else if(IfaceHold==IFACE_BARTER_CONT1SD && IsCurInRect(BarterBCont1ScrDn,DlgX,DlgY))
		{
			if(BarterScroll1<(int)Chosen->GetItemsCountInv()-(BarterWCont1[3]-BarterWCont1[1])/BarterCont1HeightItem-(int)BarterCont1o.size())
				BarterScroll1++;
		}
		else if(IfaceHold==IFACE_BARTER_CONT2SU && IsCurInRect(BarterBCont2ScrUp,DlgX,DlgY))
		{
			if(BarterScroll2>0) BarterScroll2--;
		}
		else if(IfaceHold==IFACE_BARTER_CONT2SD && IsCurInRect(BarterBCont2ScrDn,DlgX,DlgY))
		{
			if(BarterScroll2<(int)BarterCont2.size()-(BarterWCont2[3]-BarterWCont2[1])/BarterCont2HeightItem)
				BarterScroll2++;
		}
		else if(IfaceHold==IFACE_BARTER_CONT1OSU && IsCurInRect(BarterBCont1oScrUp,DlgX,DlgY))
		{
			if(BarterScroll1o>0) BarterScroll1o--;
		}
		else if(IfaceHold==IFACE_BARTER_CONT1OSD && IsCurInRect(BarterBCont1oScrDn,DlgX,DlgY))
		{
			if(BarterScroll1o<(int)BarterCont1o.size()-(BarterWCont1o[3]-BarterWCont1o[1])/BarterCont1oHeightItem)
				BarterScroll1o++;
		}
		else if(IfaceHold==IFACE_BARTER_CONT2OSU && IsCurInRect(BarterBCont2oScrUp,DlgX,DlgY))
		{
			if(BarterScroll2o>0) BarterScroll2o--;
		}
		else if(IfaceHold==IFACE_BARTER_CONT2OSD && IsCurInRect(BarterBCont2oScrDn,DlgX,DlgY))
		{
			if(BarterScroll2o<(int)BarterCont2o.size()-(BarterWCont2o[3]-BarterWCont2o[1])/BarterCont2oHeightItem)
				BarterScroll2o++;
		}
		else if(IfaceHold==IFACE_DLG_SCR_UP && IsCurInRect(DlgBScrUp,DlgX,DlgY) && BarterIsPlayers)
		{
		}
		else if(IfaceHold==IFACE_DLG_SCR_DN && IsCurInRect(DlgBScrDn,DlgX,DlgY) && BarterIsPlayers)
		{
		}
	}

	IfaceHold=IFACE_NONE;
	BarterHoldId=0;
}

void FOClient::DlgMouseMove(bool is_dialog)
{
	if(is_dialog)
	{
		DlgCurAnsw=-1;
		for(size_t i=0;i<DlgAnswers.size();i++)
		{
			if(IsCurInRect(DlgAnswers[i].Position,DlgX,DlgY))
			{
				DlgCurAnsw=i;
				break;
			}
		}
	}

	if(IfaceHold==IFACE_DLG_MAIN)
	{
		DlgX=CurX-DlgVectX;
		DlgY=CurY-DlgVectY;

		if(DlgX<0) DlgX=0;
		if(DlgX+DlgWMain[2]>MODE_WIDTH) DlgX=MODE_WIDTH-DlgWMain[2];
		if(DlgY<0) DlgY=0;
		//if(DlgY+DlgMain[3]>IntY) DlgY=IntY-DlgMain[3];
		if(DlgY+DlgWMain[3]>MODE_HEIGHT) DlgY=MODE_HEIGHT-DlgWMain[3];
	}
}

void FOClient::DlgRMouseDown(bool is_dialog)
{
	if(!is_dialog) SetCurCastling(CUR_DEFAULT,CUR_HAND);
}

void FOClient::DlgKeyDown(bool is_dialog, BYTE dik)
{
	int num;
	switch(dik)
	{
	case DIK_ESCAPE:
	case DIK_0:
	case DIK_NUMPAD0:
		if(BarterIsPlayers)
		{
			Net_SendPlayersBarter(BARTER_END,0,0);
			ShowScreen(SCREEN_NONE);
		}
		else
		{
			Net_SendTalk(DlgIsNpc,DlgNpcId,ANSWER_END);
			WaitPing();
		}
		CollectContItems();
		return;
	case DIK_INSERT:
		BarterTryOffer();
		return;
	case DIK_1:
	case DIK_NUMPAD1: num=0; break;
	case DIK_2:
	case DIK_NUMPAD2: num=1; break;
	case DIK_3:
	case DIK_NUMPAD3: num=2; break;
	case DIK_4:
	case DIK_NUMPAD4: num=3; break;
	case DIK_5:
	case DIK_NUMPAD5: num=4; break;
	case DIK_6:
	case DIK_NUMPAD6: num=5; break;
	case DIK_7:
	case DIK_NUMPAD7: num=6; break;
	case DIK_8:
	case DIK_NUMPAD8: num=7; break;
	case DIK_9:
	case DIK_NUMPAD9: num=8; break;
	case DIK_UP: DlgCollectAnswers(false); break;
	case DIK_DOWN: DlgCollectAnswers(true); break;
	default: return;
	}

	if(is_dialog && num<DlgAnswers.size())
	{
		if(DlgAnswers[num].AnswerNum<0)
		{
			DlgCollectAnswers(DlgAnswers[num].AnswerNum==-2);
		}
		else
		{
			Net_SendTalk(DlgIsNpc,DlgNpcId,DlgAnswers[num].AnswerNum);
			WaitPing();
		}
	}
}

void FOClient::DlgCollectAnswers(bool next)
{
	if(next && DlgCurAnswPage<DlgMaxAnswPage) DlgCurAnswPage++;
	if(!next && DlgCurAnswPage>0) DlgCurAnswPage--;

	DlgAnswers.clear();
	for(size_t i=0,j=DlgAllAnswers.size();i<j;i++)
	{
		Answer& a=DlgAllAnswers[i];
		if(a.Page==DlgCurAnswPage) DlgAnswers.push_back(a);
	}
	DlgMouseMove(true);
}

bool FOClient::IsScreenPlayersBarter()
{
	return IsScreenPresent(SCREEN__BARTER) && BarterIsPlayers;
}

void FOClient::BarterTryOffer()
{
	if(!Chosen) return;

	if(BarterIsPlayers)
	{
		BarterOffer=!BarterOffer;
		Net_SendPlayersBarter(BARTER_OFFER,BarterOffer,0);
	}
	else
	{
		DWORD c1,w1,v1,c2,w2,v2;
		ContainerCalcInfo(BarterCont1oInit,c1,w1,v1,-BarterK,true);
		ContainerCalcInfo(BarterCont2oInit,c2,w2,v2,Chosen->IsPerk(PE_MASTER_TRADER)?0:BarterK,false);
		if(!c1 && !c2 && BarterK) return;
		if(c1<c2 && BarterK) BarterText=MsgGame->GetStr(STR_BARTER_BAD_OFFER);
		else if(Chosen->GetFreeWeight()+w1<w2) BarterText=MsgGame->GetStr(STR_BARTER_OVERWEIGHT);
		else if(Chosen->GetFreeVolume()+v1<v2) BarterText=MsgGame->GetStr(STR_BARTER_OVERSIZE);
		else
		{
			Net_SendBarter(DlgNpcId,BarterCont1oInit,BarterCont2oInit);
			WaitPing();
			return;
		}
		DlgMainTextCur=0;
		DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,BarterText.c_str());
	}
}

void FOClient::BarterTransfer(DWORD item_id, int item_cont, DWORD item_count)
{
	if(!item_count || !Chosen) return;

	ItemVec* from_cont;
	ItemVec* to_cont;

	switch(item_cont)
	{
	case IFACE_BARTER_CONT1:
		from_cont=&InvContInit;
		to_cont=&BarterCont1oInit;
		break;
	case IFACE_BARTER_CONT2:
		from_cont=&BarterCont2Init;
		to_cont=&BarterCont2oInit;
		break;
	case IFACE_BARTER_CONT1O:
		from_cont=&BarterCont1oInit;
		to_cont=&InvContInit;
		break;
	case IFACE_BARTER_CONT2O:
		from_cont=&BarterCont2oInit;
		to_cont=&BarterCont2Init;
		break;
	default:
		return;
	}

	ItemVecIt it=std::find(from_cont->begin(),from_cont->end(),item_id);
	if(it==from_cont->end()) return;

	Item* item=&(*it);
	Item* to_item=NULL;

	if(item->GetCount()<item_count) return;

	if(item->IsGrouped())
	{
		ItemVecIt it_to=std::find(to_cont->begin(),to_cont->end(),item->GetId());
		if(it_to!=to_cont->end()) to_item=&(*it_to);
	}

	if(to_item)
	{
		to_item->Count_Add(item_count);
	}
	else
	{
		to_cont->push_back(*item);
		Item& last=(*to_cont)[to_cont->size()-1];
		last.Count_Set(item_count);
	}

	item->Count_Sub(item_count);
	if(!item->GetCount() || !item->IsGrouped()) from_cont->erase(it);
	CollectContItems();

	switch(item_cont)
	{
	case IFACE_BARTER_CONT1:
		if(BarterIsPlayers) Net_SendPlayersBarter(BARTER_SET_SELF,item_id,item_count);
		break;
	case IFACE_BARTER_CONT2:
		if(BarterIsPlayers) Net_SendPlayersBarter(BARTER_SET_OPPONENT,item_id,item_count);
		break;
	case IFACE_BARTER_CONT1O:
		if(BarterIsPlayers) Net_SendPlayersBarter(BARTER_UNSET_SELF,item_id,item_count);
		break;
	case IFACE_BARTER_CONT2O:
		if(BarterIsPlayers) Net_SendPlayersBarter(BARTER_UNSET_OPPONENT,item_id,item_count);
		break;
	default:
		break;
	}
}

void FOClient::ContainerCalcInfo(ItemVec& cont, DWORD& cost, DWORD& weigth, DWORD& volume, int barter_k, bool sell)
{
	cost=0;
	weigth=0;
	volume=0;
	for(ItemVecIt it=cont.begin();it!=cont.end();it++)
	{
		Item& item=*it;
		if(barter_k!=MAX_INT)
		{
			DWORD cost_=item.GetCost1st();
			if(GameOpt.CustomItemCost)
			{
				CritterCl* npc=GetCritter(PupContId);
				if(Chosen && npc && Script::PrepareContext(ClientFunctions.ItemCost,CALL_FUNC_STR,Chosen->GetInfo()))
				{
					Script::SetArgObject(&item);
					Script::SetArgObject(Chosen);
					Script::SetArgObject(npc);
					Script::SetArgBool(sell);
					if(Script::RunPrepared()) cost_=Script::GetReturnedDword();
				}
			}
			else
			{
				cost_=cost_*(100+barter_k)/100;
				if(!cost_) cost_++;
			}
			cost+=cost_*item.GetCount();
		}
		weigth+=item.GetWeight();
		volume+=item.GetVolume();
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::FormatTags(char* text, size_t text_len, CritterCl* player, CritterCl* npc, const char* lexems)
{
	if(!_stricmp(text,"error"))
	{
		StringCopy(text,text_len,"Text not found!");
		return;
	}

	char* str=text;
	vector<char*> dialogs;
	int sex=0;
	bool sex_tags=false;
	char tag[MAX_FOTEXT];
	tag[0]=0;

	for(int i=0;str[i];)
	{
		switch(str[i])
		{
		case '#':
			{
				str[i]='\n';
			}
			break;
		case '|':
			{
				if(sex_tags)
				{
					Str::CopyWord(tag,&str[i+1],'|',false);
					Str::EraseInterval(&str[i],strlen(tag)+2);

					if(sex)
					{
						if(sex==1)
						{
							if(strlen(str)+strlen(tag)<text_len) Str::Insert(&str[i],tag);
							//i+=strlen(tag);
						}

						sex--;
					}
					continue;
				}
			}
			break;
		case '@':
			{
				if(str[i+1]=='@')
				{
					str[i]=0;
					dialogs.push_back(str);
					str=&str[i+2];
					i=0;
					continue;
				}

				tag[0]=0;
				Str::CopyWord(tag,&str[i+1],'@',false);
				Str::EraseInterval(&str[i],strlen(tag)+2);

				if(!strlen(tag)) break;

				// Player name
				if(!_stricmp(tag,"pname"))
				{
					StringCopy(tag,player?player->GetName():"");
				}
				// Npc name
				else if(!_stricmp(tag,"nname"))
				{
					StringCopy(tag,npc?npc->GetName():"");
				}
				// Sex
				else if(!_stricmp(tag,"sex"))
				{
					sex=(player?player->GetParam(ST_GENDER)+1:1);
					sex_tags=true;
					continue;
				}
				// Lexems
				else if(strlen(tag)>4 && tag[0]=='l' && tag[1]=='e' && tag[2]=='x' && tag[3]==' ')
				{
					char* s=strstr((char*)lexems,Str::Format("$%s",&tag[4]));
					if(s)
					{
						s+=strlen(&tag[4])+1;
						if(*s==' ') s++;
						Str::CopyWord(tag,s,'$',false);
					}
					else StringCopy(tag,"<lexem not found>");
				}
				// Msg text
				else if(strlen(tag)>4 && tag[0]=='m' && tag[1]=='s' && tag[2]=='g' && tag[3]==' ')
				{
					char msg_type_name[64];
					DWORD str_num;
					Str::EraseChars(&tag[4],'(');
					Str::EraseChars(&tag[4],')');
					if(sscanf(&tag[4],"%s %u",msg_type_name,&str_num)!=2) StringCopy(tag,"<msg tag parse fail>");
					else
					{
						int msg_type=Str::GetMsgType(msg_type_name);
						if(msg_type<0 || msg_type>=TEXTMSG_COUNT) StringCopy(tag,"<msg tag, unknown type>");
						else if(!CurLang.Msg[msg_type].Count(str_num)) StringCopy(tag,Str::Format("<msg tag, string %u not found>",str_num));
						else StringCopy(tag,CurLang.Msg[msg_type].GetStr(str_num));
					}
				}
				// Script
				else if(strlen(tag)>7 && tag[0]=='s' && tag[1]=='c' && tag[2]=='r' && tag[3]=='i' && tag[4]=='p' && tag[5]=='t' && tag[6]==' ')
				{
					char func_name[256];
					Str::CopyWord(func_name,&tag[7],'$',false);
					int bind_id=Script::Bind("client_main",func_name,"string %s(string&)",true);
					StringCopy(tag,"<script function not found>");
					if(bind_id>0 && Script::PrepareContext(bind_id,CALL_FUNC_STR,"Game"))
					{
						CScriptString* script_lexems=new CScriptString(lexems);
						Script::SetArgObject(script_lexems);
						if(Script::RunPrepared())
						{
							CScriptString* result=(CScriptString*)Script::GetReturnedObject();
							if(result)
							{
								StringCopy(tag,result->c_str());
								result->Release();
							}
						}
						//StringCopy(lexems,script_lexems->c_str());
						//script_lexems->Release();
					}
				}
				// Error
				else
				{
					StringCopy(tag,"<error>");
				}

				if(strlen(str)+strlen(tag)<text_len) Str::Insert(str+i,tag);
				//i+=strlen(tag);
			}
			continue;
		default:
			break;
		}

		++i;
	}

	dialogs.push_back(str);
	StringCopy(text,text_len,dialogs[Random(0,dialogs.size()-1)]);
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

bool FOClient::IsLMenu()
{
	return LMenuMode!=LMENU_OFF && LMenuActive;
}

void FOClient::LMenuTryActivate()
{
	LMenuTryActivated=true;
	LMenuStartTime=Timer::FastTick();
}

void FOClient::LMenuStayOff()
{
	if(TargetSmth.IsItem())
	{
		ItemHex* item=GetItem(TargetSmth.GetId());
		if(item && item->IsDrawContour() && item->SprDrawValid) item->SprDraw->Contour=Sprite::ContourNone;
	}
	if(TargetSmth.IsCritter())
	{
		CritterCl* cr=GetCritter(TargetSmth.GetId());
		if(cr && cr->SprDrawValid && cr->SprDraw->Contour==Sprite::ContourYellow)
			cr->SprDraw->Contour=Sprite::ContourNone;
	}
	TargetSmth.Clear();
	LMenuCurNodes=NULL;
}

void FOClient::LMenuTryCreate()
{
	// Check valid current state
	if(GameOpt.DisableLMenu || !Chosen || IsScroll() || !IsCurInWindow() || !IsCurMode(CUR_DEFAULT) || GameOpt.HideCursor)
	{
		/*if(IsLMenu()) */LMenuSet(LMENU_OFF);
		return;
	}

	// Already created and showed
	if(IsLMenu()) return;

	// Add name of item
	if(!LMenuTryActivated && Timer::FastTick()-GameMouseStay>=LMENU_SHOW_TIME)
	{
		LMenuCollect();
		if(!LMenuCurNodes) return;

		static SmthSelected last_look;
		if(TargetSmth.IsItem() && TargetSmth!=last_look)
		{
			ItemHex* item=GetItem(TargetSmth.GetId());
			if(item) AddMess(FOMB_VIEW,FmtItemLook(item,ITEM_LOOK_ONLY_NAME));
			last_look=TargetSmth;
		}
		if(TargetSmth.IsCritter() && TargetSmth!=last_look)
		{
			CritterCl* cr=GetCritter(TargetSmth.GetId());
			if(cr) AddMess(FOMB_VIEW,FmtCritLook(cr,CRITTER_LOOK_SHORT));
			last_look=TargetSmth;
		}
		if(TargetSmth.IsContItem() && TargetSmth!=last_look)
		{
			Item* cont_item=GetTargetContItem();
			if(cont_item)
			{
				if(GetActiveScreen()==SCREEN__BARTER)
				{
					if(BarterIsPlayers)
					{
						const char* str=FmtItemLook(cont_item,ITEM_LOOK_DEFAULT);
						if(str)
						{
							BarterText+=str;
							BarterText+="\n";
							DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,BarterText.c_str());
						}
					}
					else
					{
						const char* str=FmtItemLook(cont_item,ITEM_LOOK_DEFAULT);
						if(str)
						{
							BarterText=FmtItemLook(cont_item,ITEM_LOOK_BARTER);
							DlgMainTextCur=0;
							DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,BarterText.c_str());
						}
					}
				}
				else
				{
					AddMess(FOMB_VIEW,FmtItemLook(cont_item,ITEM_LOOK_ONLY_NAME));
				}
			}
			last_look=TargetSmth;
		}

		if(TargetSmth.IsItem())
		{
			ItemHex* item=GetItem(TargetSmth.GetId());
			if(item && item->IsDrawContour() && item->SprDrawValid) item->SprDraw->Contour=Sprite::ContourYellow;
		}
		if(TargetSmth.IsCritter())
		{
			CritterCl* cr=GetCritter(TargetSmth.GetId());
			if(cr && cr->IsDead() && cr->SprDrawValid) cr->SprDraw->Contour=Sprite::ContourYellow;
		}
	}
	// Show LMenu
	else if(LMenuTryActivated && Timer::FastTick()-LMenuStartTime>=LMENU_SHOW_TIME)
	{
		LMenuCollect();
		if(!LMenuCurNodes) return;

		SndMngr.PlaySound(SND_LMENU);
		int height=LMenuCurNodes->size()*LMenuNodeHeight;
		if(LMenuY+height>MODE_HEIGHT) LMenuY-=LMenuY+height-MODE_HEIGHT;
		if(LMenuX+LMenuNodeHeight>MODE_WIDTH) LMenuX-=LMenuX+LMenuNodeHeight-MODE_WIDTH;
		SetCurPos(LMenuX,LMenuY);
		LMenuCurNode=0;
		LMenuTryActivated=false;
		LMenuActive=true;
	}
}

void FOClient::LMenuCollect()
{
	LMenuStayOff();
	GameMouseStay=Timer::FastTick();

	SpriteInfo* si_cur=SprMngr.GetSpriteInfo(CurPDef);
	LMenuX=CurX+(si_cur?si_cur->Width:0);
	LMenuY=CurY;
	LMenuRestoreCurX=CurX;
	LMenuRestoreCurY=CurY;

	int screen=GetActiveScreen();
	if(screen)
	{
		switch(screen)
		{
		case SCREEN__USE:
			{
				DWORD item_id=GetCurContainerItemId(INTRECT(UseWInv,UseX,UseY),UseHeightItem,UseScroll,UseCont);
				if(item_id)
				{
					TargetSmth.SetContItem(item_id,0);
					LMenuSet(LMENU_ITEM_INV);
				}
			}
			break;
		case SCREEN__INVENTORY:
			{
				if(!Chosen) break;
				DWORD item_id=GetCurContainerItemId(INTRECT(InvWInv,InvX,InvY),InvHeightItem,InvScroll,InvCont);
				if(!item_id && IsCurInRect(InvWSlot1,InvX,InvY)) item_id=Chosen->ItemSlotMain->GetId();
				if(!item_id && IsCurInRect(InvWSlot2,InvX,InvY)) item_id=Chosen->ItemSlotExt->GetId();
				if(!item_id && IsCurInRect(InvWArmor,InvX,InvY)) item_id=Chosen->ItemSlotArmor->GetId();
				if(!item_id)
				{
					// Find in extended slots
					for(SlotExtVecIt it=SlotsExt.begin(),end=SlotsExt.end();it!=end;++it)
					{
						SlotExt& se=*it;
						if(!se.Rect.IsZero() && IsCurInRect(se.Rect,InvX,InvY))
						{
							Item* item=Chosen->GetItemSlot(se.Index);
							if(item)
							{
								item_id=item->GetId();
								break;
							}
						}
					}
				}
				if(!item_id) break;

				TargetSmth.SetContItem(item_id,0);
				LMenuSet(LMENU_ITEM_INV);
			}
			break;
		case SCREEN__BARTER:
			{
				int cont_type=0;
				DWORD item_id=GetCurContainerItemId(INTRECT(BarterWCont1,DlgX,DlgY),BarterCont1HeightItem,BarterScroll1,BarterCont1);
				if(!item_id) item_id=GetCurContainerItemId(INTRECT(BarterWCont2,DlgX,DlgY),BarterCont2HeightItem,BarterScroll2,BarterCont2);
				if(!item_id)
				{
					cont_type=1;
					item_id=GetCurContainerItemId(INTRECT(BarterWCont1o,DlgX,DlgY),BarterCont1oHeightItem,BarterScroll1o,BarterCont1o);
					if(!item_id) item_id=GetCurContainerItemId(INTRECT(BarterWCont2o,DlgX,DlgY),BarterCont2oHeightItem,BarterScroll2o,BarterCont2o);
				}
				if(!item_id) break;

				TargetSmth.SetContItem(item_id,cont_type);
				LMenuSet(LMENU_ITEM_INV);
			}
			break;
		case SCREEN__PICKUP:
			{
				DWORD item_id=GetCurContainerItemId(INTRECT(PupWCont1,PupX,PupY),PupHeightItem1,PupScroll1,PupCont1);
				if(!item_id) item_id=GetCurContainerItemId(INTRECT(PupWCont2,PupX,PupY),PupHeightItem2,PupScroll2,PupCont2);
				if(!item_id) break;

				TargetSmth.SetContItem(item_id,0);
				LMenuSet(LMENU_ITEM_INV);
			}
			break;
		default:
			break;
		}
	}
	else
	{
		switch(GetMainScreen())
		{
		case SCREEN_GAME:
			{
				if(IntVisible)
				{
					if(IsCurInRectNoTransp(IntMainPic,IntWMain,0,0)) break;
					if(IntAddMess && IsCurInRectNoTransp(IntPWAddMess,IntWAddMess,0,0)) break;
				}

				CritterCl* cr;
				ItemHex* item;
				HexMngr.GetSmthPixel(CurX,CurY,item,cr);

				if(cr)
				{
					TargetSmth.SetCritter(cr->GetId());
					if(cr->IsPlayer()) LMenuSet(LMENU_PLAYER);
					else if(cr->IsNpc()) LMenuSet(LMENU_NPC);
				}
				else if(item)
				{
					TargetSmth.SetItem(item->GetId());
					LMenuSet(LMENU_ITEM);
				}
			}
			break;
		case SCREEN_GLOBAL_MAP:
			{
				int pos=0;
				for(CritMapIt it=HexMngr.allCritters.begin();it!=HexMngr.allCritters.end();it++,pos++)
				{
					CritterCl* cr=(*it).second;
					if(!IsCurInRect(INTRECT(GmapWName,GmapWNameStepX*pos,GmapWNameStepY*pos))) continue;
					TargetSmth.SetCritter(cr->GetId());
					LMenuSet(LMENU_GMAP_CRIT);
					break;
				}
			}
			break;
		}
	}

	if(!LMenuCurNodes) LMenuSet(LMENU_OFF);
}

void FOClient::LMenuSet(BYTE set_lmenu)
{
	if(IsLMenu()) SetCurPos(LMenuRestoreCurX,LMenuRestoreCurY);
	LMenuMode=set_lmenu;

	switch(LMenuMode)
	{
	case LMENU_OFF:
		{
			LMenuActive=false;
			LMenuStayOff();
			LMenuTryActivated=false;
			GameMouseStay=Timer::FastTick();
			LMenuStartTime=Timer::FastTick();
		}
		break;
	case LMENU_PLAYER:
	case LMENU_NPC:
		{
			if(!TargetSmth.IsCritter()) break;
			CritterCl* cr=GetCritter(TargetSmth.GetId());
			if(!cr || !Chosen) break;

			LMenuCritNodes.clear();

			if(Chosen->IsLife())
			{
				if(LMenuMode==LMENU_NPC)
				{
					// Npc
					if(cr->IsToTalk()) LMenuCritNodes.push_back(LMENU_NODE_TALK);
					if(cr->IsDead() && !cr->IsPerk(MODE_NO_LOOT)) LMenuCritNodes.push_back(LMENU_NODE_USE);
					LMenuCritNodes.push_back(LMENU_NODE_LOOK);
					if(cr->IsLife() && !cr->IsPerk(MODE_NO_PUSH)) LMenuCritNodes.push_back(LMENU_NODE_PUSH);
					LMenuCritNodes.push_back(LMENU_NODE_BAG);
					LMenuCritNodes.push_back(LMENU_NODE_SKILL);
				}
				else
				{
					if(cr!=Chosen)
					{
						// Player
						if(cr->IsDead() && !cr->IsPerk(MODE_NO_LOOT)) LMenuCritNodes.push_back(LMENU_NODE_USE);
						LMenuCritNodes.push_back(LMENU_NODE_LOOK);
						if(cr->IsLife() && !cr->IsPerk(MODE_NO_PUSH)) LMenuCritNodes.push_back(LMENU_NODE_PUSH);
						LMenuCritNodes.push_back(LMENU_NODE_BAG);
						LMenuCritNodes.push_back(LMENU_NODE_SKILL);
						LMenuCritNodes.push_back(LMENU_NODE_GMFOLLOW);
						if(cr->IsLife() && cr->IsPlayer() && cr->IsOnline() && CheckDist(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY(),BARTER_DIST))
						{
							LMenuCritNodes.push_back(LMENU_NODE_BARTER_OPEN);
							LMenuCritNodes.push_back(LMENU_NODE_BARTER_HIDE);
						}
						if(!Chosen->GetTimeout(TO_KARMA_VOTING))
						{
							LMenuCritNodes.push_back(LMENU_NODE_VOTE_UP);
							LMenuCritNodes.push_back(LMENU_NODE_VOTE_DOWN);
						}
					}
					else
					{
						// Self
						LMenuCritNodes.push_back(LMENU_NODE_ROTATE);
						LMenuCritNodes.push_back(LMENU_NODE_LOOK);
						LMenuCritNodes.push_back(LMENU_NODE_BAG);
						LMenuCritNodes.push_back(LMENU_NODE_SKILL);
						LMenuCritNodes.push_back(LMENU_NODE_PICK_ITEM);
					}
				}
			}
			else
			{
				LMenuCritNodes.push_back(LMENU_NODE_LOOK);
				if(LMenuMode!=LMENU_NPC && cr!=Chosen && !Chosen->GetTimeout(TO_KARMA_VOTING))
				{
					LMenuCritNodes.push_back(LMENU_NODE_VOTE_UP);
					LMenuCritNodes.push_back(LMENU_NODE_VOTE_DOWN);
				}
			}

			LMenuCritNodes.push_back(LMENU_NODE_BREAK);
			LMenuCurNodes=&LMenuCritNodes;
		}
		break;
	case LMENU_ITEM:
		{
			if(!TargetSmth.IsItem()) break;
			ItemHex* item=GetItem(TargetSmth.GetId());
			if(!item) break;

			LMenuNodes.clear();

			if(item->IsUsable()) LMenuNodes.push_back(LMENU_NODE_PICK);
			LMenuNodes.push_back(LMENU_NODE_LOOK);
			LMenuNodes.push_back(LMENU_NODE_BAG);
			LMenuNodes.push_back(LMENU_NODE_SKILL);
			LMenuNodes.push_back(LMENU_NODE_BREAK);

			LMenuCurNodes=&LMenuNodes;
		}
		break;
	case LMENU_ITEM_INV:
		{
			Item* cont_item=GetTargetContItem();
			if(!cont_item || !Chosen) break;
			Item* inv_item=Chosen->GetItem(cont_item->GetId());

			LMenuNodes.clear();
			LMenuNodes.push_back(LMENU_NODE_LOOK);

			if(inv_item)
			{
				if(inv_item->IsWeapon() && inv_item->WeapGetMaxAmmoCount() && !inv_item->WeapIsEmpty() && inv_item->WeapGetAmmoCaliber())
					LMenuNodes.push_back(LMENU_NODE_UNLOAD);
				if(inv_item->IsCanUse() || inv_item->IsCanUseOnSmth())
					LMenuNodes.push_back(LMENU_NODE_USE);
				LMenuNodes.push_back(LMENU_NODE_SKILL);
				if(inv_item->ACC_CRITTER.Slot==SLOT_INV && Chosen->IsCanSortItems())
				{
					LMenuNodes.push_back(LMENU_NODE_SORT_UP);
					LMenuNodes.push_back(LMENU_NODE_SORT_DOWN);
				}
				LMenuNodes.push_back(LMENU_NODE_DROP);
			}
			else
			{
				// Items in another containers
			}

			LMenuNodes.push_back(LMENU_NODE_BREAK);
			LMenuCurNodes=&LMenuNodes;
		}
		break;
	case LMENU_GMAP_CRIT:
		{
			if(!TargetSmth.IsCritter()) break;
			CritterCl* cr=GetCritter(TargetSmth.GetId());
			if(!cr || !Chosen) break;
			bool is_water=(GmapRelief?GmapRelief->Get4Bit(GmapGroupX,GmapGroupY)==0xF:false);

			LMenuNodes.clear();
			LMenuNodes.push_back(LMENU_NODE_LOOK);
			LMenuNodes.push_back(LMENU_NODE_SKILL);
			LMenuNodes.push_back(LMENU_NODE_BAG);

			if(cr->GetId()!=Chosen->GetId()) // Another critters
			{
				if(cr->IsPlayer() && cr->IsLife())
				{
					// Kick and give rule
					if(Chosen->IsGmapRule())
					{
						if(!is_water) LMenuNodes.push_back(LMENU_NODE_GMAP_KICK);
						if(!cr->IsOffline()) LMenuNodes.push_back(LMENU_NODE_GMAP_RULE);
					}

					// Barter
					if(!cr->IsOffline())
					{
						LMenuNodes.push_back(LMENU_NODE_BARTER_OPEN);
						LMenuNodes.push_back(LMENU_NODE_BARTER_HIDE);
					}
				}
			}
			else // Self
			{
				// Kick self
				
				if(!Chosen->IsGmapRule() && HexMngr.allCritters.size()>1 && !is_water) LMenuNodes.push_back(LMENU_NODE_GMAP_KICK);
			}

			if(cr->IsPlayer() && cr->GetId()!=Chosen->GetId() && !Chosen->GetTimeout(TO_KARMA_VOTING))
			{
				LMenuNodes.push_back(LMENU_NODE_VOTE_UP);
				LMenuNodes.push_back(LMENU_NODE_VOTE_DOWN);
			}

			LMenuNodes.push_back(LMENU_NODE_BREAK);
			LMenuCurNodes=&LMenuNodes;
		}
		break;
	default:
		break;
	}
}

void FOClient::LMenuDraw()
{
	if(!LMenuCurNodes) return;

	for(int i=0,j=LMenuCurNodes->size();i<j;i++)
	{
		int num_node=(*LMenuCurNodes)[i];

//==================================================================================
#define LMENU_DRAW_CASE(node,pic_up,pic_down) \
	case node:\
		if(i==LMenuCurNode && IsLMenu())\
			SprMngr.DrawSprite(pic_down,LMenuX,LMenuY+LMenuNodeHeight*i);\
		else\
			SprMngr.DrawSprite(pic_up,LMenuX,LMenuY+LMenuNodeHeight*i);\
		break
//==================================================================================

		switch(num_node)
		{
		LMENU_DRAW_CASE(LMENU_NODE_LOOK,LmenuPLookOff,LmenuPLookOn);
		LMENU_DRAW_CASE(LMENU_NODE_TALK,LmenuPTalkOff,LmenuPTalkOn);
		LMENU_DRAW_CASE(LMENU_NODE_BREAK,LmenuPBreakOff,LmenuPBreakOn);
		LMENU_DRAW_CASE(LMENU_NODE_PICK,LmenuPUseOff,LmenuPUseOn);
		LMENU_DRAW_CASE(LMENU_NODE_GMFOLLOW,LmenuPGMFollowOff,LmenuPGMFollowOn);
		LMENU_DRAW_CASE(LMENU_NODE_ROTATE,LmenuPRotateOff,LmenuPRotateOn);
		LMENU_DRAW_CASE(LMENU_NODE_DROP,LmenuPDropOff,LmenuPDropOn);
		LMENU_DRAW_CASE(LMENU_NODE_UNLOAD,LmenuPUnloadOff,LmenuPUnloadOn);
		LMENU_DRAW_CASE(LMENU_NODE_USE,LmenuPUseOff,LmenuPUseOn);
		LMENU_DRAW_CASE(LMENU_NODE_SORT_UP,LmenuPSortUpOff,LmenuPSortUpOn);
		LMENU_DRAW_CASE(LMENU_NODE_SORT_DOWN,LmenuPSortDownOff,LmenuPSortDownOn);
		LMENU_DRAW_CASE(LMENU_NODE_PICK_ITEM,LmenuPPickItemOff,LmenuPPickItemOn);
		LMENU_DRAW_CASE(LMENU_NODE_PUSH,LmenuPPushOff,LmenuPPushOn);
		LMENU_DRAW_CASE(LMENU_NODE_BAG,LmenuPBagOff,LmenuPBagOn);
		LMENU_DRAW_CASE(LMENU_NODE_SKILL,LmenuPSkillOff,LmenuPSkillOn);
		LMENU_DRAW_CASE(LMENU_NODE_BARTER_OPEN,LmenuPBarterOpenOff,LmenuPBarterOpenOn);
		LMENU_DRAW_CASE(LMENU_NODE_BARTER_HIDE,LmenuPBarterHideOff,LmenuPBarterHideOn);
		LMENU_DRAW_CASE(LMENU_NODE_GMAP_KICK,LmenuPGmapKickOff,LmenuPGmapKickOn);
		LMENU_DRAW_CASE(LMENU_NODE_GMAP_RULE,LmenuPGmapRuleOff,LmenuPGmapRuleOn);
		LMENU_DRAW_CASE(LMENU_NODE_VOTE_UP,LmenuPVoteUpOff,LmenuPVoteUpOn);
		LMENU_DRAW_CASE(LMENU_NODE_VOTE_DOWN,LmenuPVoteDownOff,LmenuPVoteDownOn);
		default:
			WriteLog(__FUNCTION__" - Unknown node<%d>.\n",num_node);
			break;
		}
		if(!IsLMenu()) break;
	}
}

void FOClient::LMenuMouseMove()
{
	if(IsLMenu())
	{
		LMenuCurNode=(CurY-LMenuY)/LMenuNodeHeight;
		if(LMenuCurNode<0) LMenuCurNode=0;
		if(LMenuCurNode>LMenuCurNodes->size()-1) LMenuCurNode=LMenuCurNodes->size()-1;
	}
	else
	{
		GameMouseStay=Timer::FastTick();
		LMenuStartTime=Timer::FastTick();
		LMenuStayOff();
	}
}

void FOClient::LMenuMouseUp()
{
	if(!IsLMenu() && !LMenuTryActivated) return;
	LMenuTryActivated=false;
	if(!LMenuCurNodes) LMenuCollect();
	if(!Chosen || !LMenuCurNodes) return;
	if(!IsLMenu()) LMenuCurNode=0;

	ByteVec::iterator it_l=LMenuCurNodes->begin();
	it_l+=LMenuCurNode;

	switch(LMenuMode)
	{
	case LMENU_PLAYER:
		{
			if(!TargetSmth.IsCritter()) break;
			CritterCl* cr=GetCritter(TargetSmth.GetId());
			if(!cr) break;

			switch(*it_l)
			{
			case LMENU_NODE_LOOK:
				AddMess(FOMB_VIEW,FmtCritLook(cr,CRITTER_LOOK_FULL));
				break;
			case LMENU_NODE_GMFOLLOW:
				Net_SendRuleGlobal(GM_CMD_FOLLOW_CRIT,cr->GetId());
				break;
			case LMENU_NODE_USE:
				SetAction(CHOSEN_PICK_CRIT,cr->GetId(),0);
				break;
			case LMENU_NODE_ROTATE:
				SetAction(CHOSEN_DIR,0);
				break;
			case LMENU_NODE_PICK_ITEM:
				TryPickItemOnGround();
				break;
			case LMENU_NODE_PUSH:
				SetAction(CHOSEN_PICK_CRIT,cr->GetId(),1);
				break;
			case LMENU_NODE_BAG:
				UseSelect=TargetSmth;
				ShowScreen(SCREEN__USE);
				break;
			case LMENU_NODE_SKILL:
				SboxUseOn=TargetSmth;
				ShowScreen(SCREEN__SKILLBOX);
				break;
			case LMENU_NODE_BARTER_OPEN:
				Net_SendPlayersBarter(BARTER_TRY,cr->GetId(),false);
				break;
			case LMENU_NODE_BARTER_HIDE:
				Net_SendPlayersBarter(BARTER_TRY,cr->GetId(),true);
				break;
			case LMENU_NODE_VOTE_UP:
				Net_SendKarmaVoting(cr->GetId(),true);
				break;
			case LMENU_NODE_VOTE_DOWN:
				Net_SendKarmaVoting(cr->GetId(),false);
				break;
			case LMENU_NODE_BREAK:
				break;
			default:
				break;
			}
		}
		break;
	case LMENU_NPC:
		{
			if(!TargetSmth.IsCritter()) break;
			CritterCl* cr=GetCritter(TargetSmth.GetId());
			if(!cr) break;

			switch(*it_l)
			{
			case LMENU_NODE_LOOK:
				AddMess(FOMB_VIEW,FmtCritLook(cr,CRITTER_LOOK_FULL));
				break;
			case LMENU_NODE_TALK:
				SetAction(CHOSEN_TALK_NPC,cr->GetId());
				break;
			case LMENU_NODE_GMFOLLOW:
				Net_SendRuleGlobal(GM_CMD_FOLLOW_CRIT,cr->GetId());
				break;
			case LMENU_NODE_USE:
				SetAction(CHOSEN_PICK_CRIT,cr->GetId(),0);
				break;
			case LMENU_NODE_PUSH:
				SetAction(CHOSEN_PICK_CRIT,cr->GetId(),1);
				break;
			case LMENU_NODE_BAG:
				UseSelect=TargetSmth;
				ShowScreen(SCREEN__USE);
				break;
			case LMENU_NODE_SKILL:
				SboxUseOn=TargetSmth;
				ShowScreen(SCREEN__SKILLBOX);
				break;
			case LMENU_NODE_BREAK:
				break;
			default:
				break;
			}
		}
		break;
	case LMENU_ITEM:
		{
			if(!TargetSmth.IsItem()) break;
			ItemHex* item=GetItem(TargetSmth.GetId());
			if(!item) break;

			switch(*it_l)
			{
			case LMENU_NODE_LOOK:
				AddMess(FOMB_VIEW,FmtItemLook(item,ITEM_LOOK_MAP));
				break;
			case LMENU_NODE_PICK:
				SetAction(CHOSEN_PICK_ITEM,item->GetProtoId(),item->HexX,item->HexY);
				break;
			case LMENU_NODE_BAG:
				UseSelect=TargetSmth;
				ShowScreen(SCREEN__USE);
				break;
			case LMENU_NODE_SKILL:
				SboxUseOn=TargetSmth;
				ShowScreen(SCREEN__SKILLBOX);
				break;
			case LMENU_NODE_BREAK:
				break;
			default:
				break;
			}
		}
		break;
	case LMENU_ITEM_INV:
		{
			Item* cont_item=GetTargetContItem();
			if(!cont_item) break;
			Item* inv_item=Chosen->GetItem(cont_item->GetId());

			switch(*it_l)
			{
			case LMENU_NODE_LOOK:
				if(GetActiveScreen()==SCREEN__INVENTORY)
				{
					if(!inv_item) break;
					const char* str=FmtItemLook(inv_item,ITEM_LOOK_INVENTORY);
					if(str)
					{
						InvItemInfo=str;
						InvItemInfoScroll=0;
						InvItemInfoMaxScroll=SprMngr.GetLinesCount(InvWText.W(),0,str)-SprMngr.GetLinesCount(0,InvWText.H(),NULL);
					}
				}
				else if(GetActiveScreen()==SCREEN__BARTER)
				{
					if(BarterIsPlayers)
					{
						const char* str=FmtItemLook(cont_item,ITEM_LOOK_DEFAULT);
						if(str)
						{
							BarterText+=str;
							BarterText+="\n";
							DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,BarterText.c_str());
						}
					}
					else
					{
						const char* str=FmtItemLook(cont_item,ITEM_LOOK_BARTER);
						if(str)
						{
							BarterText=str;
							DlgMainTextCur=0;
							DlgMainTextLinesReal=SprMngr.GetLinesCount(DlgWText.W(),0,BarterText.c_str());
						}
					}
				}
				else if(GetActiveScreen()==SCREEN__PICKUP) AddMess(FOMB_VIEW,FmtItemLook(cont_item,ITEM_LOOK_DEFAULT));
				break;
			case LMENU_NODE_DROP:
				if(!cont_item) break;
				if(!Chosen->IsLife() || !Chosen->IsFree()) break;
				if(cont_item->IsGrouped() && cont_item->GetCount()>1)
					SplitStart(cont_item,0xFF|(TargetSmth.GetParam()<<16));
				else
					AddActionBack(CHOSEN_MOVE_ITEM,cont_item->GetId(),cont_item->GetCount(),0xFF,TargetSmth.GetParam());
				break;
			case LMENU_NODE_UNLOAD:
				if(!inv_item) break;
				SetAction(CHOSEN_USE_ITEM,cont_item->GetId(),0,TARGET_SELF_ITEM,-1,USE_RELOAD);
				break;
			case LMENU_NODE_USE:
				if(!inv_item) break;
				if(inv_item->IsHasTimer()) TimerStart(inv_item->GetId(),ResMngr.GetInvSprId(inv_item->GetPicInv()),inv_item->GetInvColor());
				else SetAction(CHOSEN_USE_ITEM,inv_item->GetId(),0,TARGET_SELF,0,USE_USE);
				break;
			case LMENU_NODE_SKILL:
				if(!inv_item) break;
				SboxUseOn.SetContItem(inv_item->GetId(),0);
				ShowScreen(SCREEN__SKILLBOX);
				break;
			case LMENU_NODE_SORT_UP:
				{
					if(!inv_item) break;
					Item* item=Chosen->GetItemLowSortValue();
					if(!item || inv_item==item) break;
					if(inv_item->GetSortValue()<item->GetSortValue()) break;
					inv_item->Data.SortValue=item->GetSortValue()-1;
					Net_SendSortValueItem(inv_item);
				}
				break;
			case LMENU_NODE_SORT_DOWN:
				{
					if(!inv_item) break;
					Item* item=Chosen->GetItemHighSortValue();
					if(!item || inv_item==item) break;
					if(inv_item->GetSortValue()>item->GetSortValue()) break;
					inv_item->Data.SortValue=item->GetSortValue()+1;
					Net_SendSortValueItem(inv_item);		
				}
				break;
			case LMENU_NODE_BREAK:
				break;
			default:
				break;
			}
		}
		break;
	case LMENU_GMAP_CRIT:
		{
			if(!TargetSmth.IsCritter()) break;
			CritterCl* cr=GetCritter(TargetSmth.GetId());
			if(!cr) break;

			switch(*it_l)
			{
			case LMENU_NODE_LOOK:
				AddMess(FOMB_VIEW,FmtCritLook(cr,CRITTER_LOOK_FULL));
				break;
			case LMENU_NODE_SKILL:
				SboxUseOn=TargetSmth;
				ShowScreen(SCREEN__SKILLBOX);
				break;
			case LMENU_NODE_BAG:
				UseSelect=TargetSmth;
				ShowScreen(SCREEN__USE);
				break;
			case LMENU_NODE_GMAP_KICK:
				Net_SendRuleGlobal(GM_CMD_KICKCRIT,cr->GetId());
				break;
			case LMENU_NODE_GMAP_RULE:
				Net_SendRuleGlobal(GM_CMD_GIVE_RULE,cr->GetId());
				break;
			case LMENU_NODE_BARTER_OPEN:
				Net_SendPlayersBarter(BARTER_TRY,cr->GetId(),false);
				break;
			case LMENU_NODE_BARTER_HIDE:
				Net_SendPlayersBarter(BARTER_TRY,cr->GetId(),true);
				break;
			case LMENU_NODE_VOTE_UP:
				Net_SendKarmaVoting(cr->GetId(),true);
				break;
			case LMENU_NODE_VOTE_DOWN:
				Net_SendKarmaVoting(cr->GetId(),false);
				break;
			case LMENU_NODE_BREAK:
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	}

	LMenuSet(LMENU_OFF);
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::ShowMainScreen(int new_screen)
{
	while(GetActiveScreen()!=SCREEN_NONE) ShowScreen(SCREEN_NONE);
	int prev_main_screen=ScreenModeMain;
	if(ScreenModeMain) RunScreenScript(false,ScreenModeMain,-1,0,0);
	ScreenModeMain=new_screen;
	RunScreenScript(true,new_screen,-1,0,0);

	switch(GetMainScreen())
	{
	case SCREEN_LOGIN:
		SetCurMode(CUR_DEFAULT);
		IntMessTabLevelUp=false;
		LogFocus=IFACE_LOG_NAME;
		ScreenFadeOut();
		break;
	case SCREEN_REGISTRATION:
		SetCurMode(CUR_DEFAULT);
		if(!RegNewCr)
		{
			RegNewCr=new CritterCl();
			RegNewCr->InitForRegistration();
		}
		ZeroMemory(ChaSkillUp,sizeof(ChaSkillUp));
		ChaUnspentSkillPoints=RegNewCr->Params[ST_UNSPENT_SKILL_POINTS];
		ChaX=(MODE_WIDTH-RegWMain.W())/2;
		ChaY=(MODE_HEIGHT-RegWMain.H())/2;
		ChaMouseMove(true);
		ScreenFadeOut();
		break;
	case SCREEN_CREDITS:
		CreditsNextTick=Timer::FastTick();
		CreditsYPos=MODE_HEIGHT;
		CreaditsExt=Keyb::ShiftDwn;
		break;
	case SCREEN_GAME:
		SetCurMode(CUR_DEFAULT);
		if(Singleplayer) SingleplayerData.Pause=false;
		break;
	case SCREEN_GLOBAL_MAP:
		SetCurMode(CUR_DEFAULT);
		GmapMouseMove();
		if(Singleplayer) SingleplayerData.Pause=false;
		break;
	case SCREEN_WAIT:
		SetCurMode(CUR_WAIT);
		if(prev_main_screen!=SCREEN_WAIT)
		{
			ScreenEffects.clear();
			WaitPic=ResMngr.GetRandomSplash();
		}
		break;
	default:
		break;
	}

	MessBoxGenerate();
}

int FOClient::GetActiveScreen(IntVec** screens /* = NULL */)
{
	static IntVec active_screens;
	active_screens.clear();

	if(Script::PrepareContext(ClientFunctions.GetActiveScreens,CALL_FUNC_STR,"Game"))
	{
		asIScriptArray* arr=Script::CreateArray("int[]");
		if(arr)
		{
			Script::SetArgObject(arr);
			if(Script::RunPrepared()) Script::AssignScriptArrayInVector(active_screens,arr);
			arr->Release();
		}
	}

	if(screens) *screens=&active_screens;
	int active=(active_screens.size()?active_screens.back():SCREEN_NONE);
	if(active>=SCREEN_LOGIN && active<=SCREEN_WAIT) active=SCREEN_NONE;
	return active;
}

bool FOClient::IsScreenPresent(int screen)
{
	IntVec* active_screens;
	GetActiveScreen(&active_screens);
	return std::find(active_screens->begin(),active_screens->end(),screen)!=active_screens->end();
}

void FOClient::ShowScreen(int screen, int p0, int p1, int p2)
{
	SmthSelected smth=TargetSmth;
	ConsoleLastKey=0;
	ShowScreenType=0;
	ShowScreenParam=0;
	ShowScreenNeedAnswer=false;
	IfaceHold=IFACE_NONE;
	Timer::StartAccelerator(ACCELERATE_NONE);
	DropScroll();

	if(screen==SCREEN_NONE)
	{
		int s=GetActiveScreen();
		if(p0==-1) SetCurMode(CUR_DEFAULT);
		else if(p0==-2) SetLastCurMode();
		if(s!=SCREEN_NONE) RunScreenScript(false,s,p0,p1,p2);

		if(Singleplayer)
		{
			if(SingleplayerData.Pause && s==SCREEN__MENU_OPTION) SingleplayerData.Pause=false;

			if(s==SCREEN__SAVE_LOAD)
			{
				if(SaveLoadLoginScreen) ScreenFadeOut();
				SaveLoadDataSlots.clear();
			}
			else if(s==SCREEN__SAY && SayType==DIALOGSAY_SAVE) SaveLoadShowDraft();
		}
		return;
	}

	switch(screen)
	{
	case SCREEN__INVENTORY:
		SetCurMode(CUR_HAND);
		CollectContItems();
		InvHoldId=0;
		InvScroll=0;
		InvItemInfo="";
		InvMouseMove();
		break;
	case SCREEN__PICKUP:
		SetCurMode(CUR_HAND);
		CollectContItems();
		PupScroll1=0;
		PupScroll2=0;
		PupMouseMove();
		break;
	case SCREEN__MINI_MAP:
		SetCurMode(CUR_DEFAULT);
	//	LmapHold=LMAP_NONE;
		LmapPrepareMap();
		LmapMouseMove();
		break;
	case SCREEN__CHARACTER:
		SetCurMode(CUR_DEFAULT);
		IntMessTabLevelUp=false;
		ZeroMemory(ChaSkillUp,sizeof(ChaSkillUp));
		if(Chosen) ChaUnspentSkillPoints=Chosen->Params[ST_UNSPENT_SKILL_POINTS];
		ZeroMemory(ChaSwitchScroll,sizeof(ChaSwitchScroll));
		if(!Chosen || (ChaSkilldexPic>=SKILLDEX_PARAM(PERK_BEGIN) && ChaSkilldexPic<=SKILLDEX_PARAM(PERK_END) && !Chosen->IsPerk(ChaSkilldexPic)))
		{
			ChaSkilldexPic=-1;
			ChaName[0]=0;
			ChaDesc[0]=0;
		}
		ChaPrepareSwitch();
		ChaMouseMove(false);
		break;
	case SCREEN__DIALOG:
		SetCurMode(CUR_DEFAULT);
		DlgMouseMove(true);
		break;
	case SCREEN__BARTER:
		SetCurMode(CUR_DEFAULT);
		BarterHoldId=0;
		BarterIsPlayers=false;
		break;
	case SCREEN__PIP_BOY:
		SetCurMode(CUR_DEFAULT);
		PipMode=PIP__NONE;
		PipMouseMove();
		break;
	case SCREEN__FIX_BOY:
		SetCurMode(CUR_DEFAULT);
		FixMode=FIX_MODE_LIST;
		FixCurCraft=-1;
		FixGenerate(FIX_MODE_LIST);
		FixMouseMove();
		break;
	case SCREEN__MENU_OPTION:
		SetCurMode(CUR_DEFAULT);
		break;
	case SCREEN__AIM:
		{
			AimTargetId=0;
			if(!smth.IsCritter()) break;
			CritterCl* cr=GetCritter(smth.GetId());
			if(!cr) break;
			AimTargetId=cr->GetId();
			SetCurMode(CUR_DEFAULT);
			AimPic=AimGetPic(cr,"frm");
			if(!AimPic) AimPic=AimGetPic(cr,"png");
			if(!AimPic) AimPic=AimGetPic(cr,"bmp");
			AimMouseMove();
		}
		break;
	case SCREEN__SAY:
		SetCurMode(CUR_DEFAULT);
		SayType=DIALOGSAY_NONE;
		SayTitle=MsgGame->GetStr(STR_SAY_TITLE);
		SayOnlyNumbers=false;
		break;
	case SCREEN__SPLIT:
		SetCurMode(CUR_DEFAULT);
		break;
	case SCREEN__TIMER:
		SetCurMode(CUR_DEFAULT);
		break;
	case SCREEN__DIALOGBOX:
		SetCurMode(CUR_DEFAULT);
		DlgboxType=DIALOGBOX_NONE;
		DlgboxWait=0;
		StringCopy(DlgboxText,"");
		DlgboxButtonsCount=0;
		DlgboxSelectedButton=0;
		break;
	case SCREEN__ELEVATOR:
		SetCurMode(CUR_DEFAULT);
		break;
	case SCREEN__CHA_NAME:
		SetCurMode(CUR_DEFAULT);
		IfaceHold=IFACE_CHA_NAME_NAME;
		ChaNameX=ChaX+ChaNameWMain[0];
		ChaNameY=ChaY+ChaNameWMain[1];
		break;
	case SCREEN__CHA_AGE:
		SetCurMode(CUR_DEFAULT);
		ChaAgeX=ChaX+ChaAgeWMain[0];
		ChaAgeY=ChaY+ChaAgeWMain[1];
		if(!IsMainScreen(SCREEN_REGISTRATION)) ShowScreen(SCREEN_NONE);
		break;
	case SCREEN__CHA_SEX:
		SetCurMode(CUR_DEFAULT);
		ChaSexX=ChaX+ChaSexWMain[0];
		ChaSexY=ChaY+ChaSexWMain[1];
		if(!IsMainScreen(SCREEN_REGISTRATION)) ShowScreen(SCREEN_NONE);
		break;
	case SCREEN__GM_TOWN:
		{
			SetCurMode(CUR_DEFAULT);
			GmapTownTextPos.clear();
			GmapTownText.clear();

			GmapTownPicId=0;
			GmapTownPicPos[0]=0;
			GmapTownPicPos[1]=0;
			GmapTownPicPos[2]=MODE_WIDTH;
			GmapTownPicPos[3]=MODE_HEIGHT;
			int pic_offsx=0;
			int pic_offsy=0;
			WORD loc_pid=GmapTownLoc.LocPid;

			DWORD spr_id=ResMngr.GetIfaceSprId(Str::GetHash(MsgGM->GetStr(STR_GM_PIC_(loc_pid))));
			SpriteInfo* si=SprMngr.GetSpriteInfo(spr_id);
			if(!si)
			{
				RunScreenScript(true,screen,p0,p1,p2);
				ShowScreen(SCREEN_NONE);
				return;
			}

			GmapTownPicId=spr_id;
			pic_offsx=(MODE_WIDTH-si->Width)/2;
			pic_offsy=(MODE_HEIGHT-si->Height)/2;
			GmapTownPicPos[0]=pic_offsx;
			GmapTownPicPos[1]=pic_offsy;
			GmapTownPicPos[2]=pic_offsx+si->Width;
			GmapTownPicPos[3]=pic_offsy+si->Height;

			DWORD entrance=MsgGM->GetInt(STR_GM_ENTRANCE_COUNT_(loc_pid));
			if(!entrance) break;

			for(int i=0;i<entrance;i++)
			{
				int x=MsgGM->GetInt(STR_GM_ENTRANCE_PICX_(loc_pid,i))+pic_offsx;
				int y=MsgGM->GetInt(STR_GM_ENTRANCE_PICY_(loc_pid,i))+pic_offsy;

				GmapTownTextPos.push_back(INTRECT(x,y,MODE_WIDTH,MODE_HEIGHT));
				GmapTownText.push_back(string(MsgGM->GetStr(STR_GM_ENTRANCE_NAME_(loc_pid,i))));
			}

			if(GmapTownLoc.LocId!=GmapShowEntrancesLocId || Timer::FastTick()>=GmapNextShowEntrancesTick)
			{
				Net_SendRuleGlobal(GM_CMD_ENTRANCES,GmapTownLoc.LocId);
				GmapShowEntrancesLocId=GmapTownLoc.LocId;
				GmapNextShowEntrancesTick=Timer::FastTick()+GM_ENTRANCES_SEND_TIME;
			}
		}
		break;
	case SCREEN__INPUT_BOX:
		SetCurMode(CUR_DEFAULT);
		IboxMode=IBOX_MODE_NONE;
		IboxHolodiskId=0;
		IboxTitleCur=0;
		IboxTextCur=0;
		break;
	case SCREEN__SKILLBOX:
		SetCurMode(CUR_DEFAULT);
		break;
	case SCREEN__USE:
		SetCurMode(CUR_HAND);
		CollectContItems();
		UseHoldId=0;
		UseScroll=0;
		UseMouseMove();
		break;
	case SCREEN__PERK:
		SetCurMode(CUR_DEFAULT);
		PerkScroll=0;
		PerkCurPerk=-1;
		PerkPrepare();
		PerkMouseMove();
		break;
	case SCREEN__TOWN_VIEW:
		SetCurMode(CUR_DEFAULT);
		TViewShowCountours=false;
		TViewType=TOWN_VIEW_FROM_NONE;
		TViewGmapLocId=0;
		TViewGmapLocEntrance=0;
		break;
	case SCREEN__SAVE_LOAD:
		SaveLoadCollect();
		if(SaveLoadLoginScreen) ScreenFadeOut();
		break;
	default:
		break;
	}
	RunScreenScript(true,screen,p0,p1,p2);
}

void FOClient::HideScreen(int screen, int p0, int p1, int p2)
{
	RunScreenScript(false,screen,p0,p1,p2);
}

void FOClient::RunScreenScript(bool show, int screen, int p0, int p1, int p2)
{
	if(Script::PrepareContext(ClientFunctions.ScreenChange,CALL_FUNC_STR,"Game"))
	{
		Script::SetArgBool(show);
		Script::SetArgDword(screen);
		Script::SetArgDword(p0);
		Script::SetArgDword(p1);
		Script::SetArgDword(p2);
		Script::RunPrepared();
	}
}

void FOClient::SetCurMode(int new_cur)
{
//	AddMess(0,Str::Format("SetCurMode Cur %u",CurMode));
//	AddMess(0,Str::Format("SetCurMode New %u",new_cur));
	if(CurModeLast!=CurMode) CurModeLast=CurMode;
	CurMode=new_cur;

	LMenuSet(LMENU_OFF);

	if(CurMode==CUR_USE_WEAPON && IsMainScreen(SCREEN_GAME) && Chosen && Chosen->ItemSlotMain->IsWeapon())
		HexMngr.SetCrittersContour(Sprite::ContourCustom);
	else if(CurModeLast==CUR_USE_WEAPON)
		HexMngr.SetCrittersContour(Sprite::ContourNone);
	if(CurMode==CUR_MOVE) HexMngr.SetCursorVisible(true);
	else if(CurModeLast==CUR_MOVE) HexMngr.SetCursorVisible(false);
}

void FOClient::SetCurCastling(int cur1, int cur2)
{
	if(CurMode==cur1) SetCurMode(cur2);
	else if(CurMode==cur2) SetCurMode(cur1);		
}

void FOClient::SetLastCurMode()
{
//	AddMess(0,Str::Format("SetLastCurMode Cur %u",CurMode));
//	AddMess(0,Str::Format("SetLastCurMode Last %u",CurModeLast));
	if(CurModeLast==CUR_WAIT) return;
	if(CurMode==CurModeLast) return;
	SetCurMode(CurModeLast);
}

void FOClient::SetCurPos(int x, int y)
{
	CurX=x;
	CurY=y;
	if(!GameOpt.FullScreen)
	{
		WINDOWINFO wi;
		wi.cbSize=sizeof(wi);
		GetWindowInfo(Wnd,&wi);
		SetCursorPos(wi.rcClient.left+CurX,wi.rcClient.top+CurY);

		//POINT pp;
		//GetCursorPos(&pp);
		//WriteLog("%d + %d = %d, %d + %d = %d; real %d, %d\n",wi.rcClient.left,CurX,wi.rcClient.left+CurX,wi.rcClient.top,CurY,wi.rcClient.top+CurY,pp.x,pp.y);
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::LmapPrepareMap()
{
	if(!Chosen) return;

	LmapPrepPix.clear();

	int maxpixx=(LmapWMap[2]-LmapWMap[0])/2/LmapZoom;
	int maxpixy=(LmapWMap[3]-LmapWMap[1])/2/LmapZoom;
	int bx=Chosen->GetHexX()-maxpixx;
	int by=Chosen->GetHexY()-maxpixy;
	int ex=Chosen->GetHexX()+maxpixx;
	int ey=Chosen->GetHexY()+maxpixy;

	DWORD vis=Chosen->GetLook();
	DWORD cur_color=0;
	int pix_x=LmapWMap[2]-LmapWMap[0], pix_y=0;

	for(int i1=bx;i1<ex;i1++)
	{
		for(int i2=by;i2<ey;i2++)
		{
			pix_y+=LmapZoom;
			if(i1<0 || i2<0 || i1>=HexMngr.GetMaxHexX() || i2>=HexMngr.GetMaxHexY()) continue;

			bool is_far=false;
			DWORD dist=DistGame(Chosen->GetHexX(),Chosen->GetHexY(),i1,i2);
			if(dist>vis) is_far=true;

			Field& f=HexMngr.GetField(i1,i2);
			if(f.Crit)
			{
				cur_color=(f.Crit==Chosen?0xFF0000FF:(f.Crit->Params[ST_FOLLOW_CRIT]==Chosen->GetId()?0xFFFF00FF:0xFFFF0000));
				LmapPrepPix.push_back(PrepPoint(LmapWMap[0]+pix_x+(LmapZoom-1),LmapWMap[1]+pix_y,cur_color,&LmapX,&LmapY));
				LmapPrepPix.push_back(PrepPoint(LmapWMap[0]+pix_x,LmapWMap[1]+pix_y+((LmapZoom-1)/2),cur_color,&LmapX,&LmapY));
			}
			else if(f.IsExitGrid)
			{
				cur_color=0x3FFF7F00;
			}
			else if(f.IsWall || f.IsScen)
			{
				if(f.IsWallSAI || f.ScrollBlock) continue;
				if(LmapSwitchHi==false && !f.IsWall) continue;
				cur_color=(f.IsWall?0xFF00FF00:0x7F00FF00);
			}
			else
			{
				continue;
			}

			if(is_far) cur_color=COLOR_CHANGE_ALPHA(cur_color,0x22);
			LmapPrepPix.push_back(PrepPoint(LmapWMap[0]+pix_x,LmapWMap[1]+pix_y,cur_color,&LmapX,&LmapY));
			LmapPrepPix.push_back(PrepPoint(LmapWMap[0]+pix_x+(LmapZoom-1),LmapWMap[1]+pix_y+((LmapZoom-1)/2),cur_color,&LmapX,&LmapY));
		}
		pix_x-=LmapZoom;
		pix_y=0;
	}

	LmapPrepareNextTick=Timer::FastTick()+MINIMAP_PREPARE_TICK;
}

void FOClient::LmapDraw()
{
	if(Timer::FastTick()>=LmapPrepareNextTick) LmapPrepareMap();

	SprMngr.DrawSprite(LmapPMain,LmapMain[0]+LmapX,LmapMain[1]+LmapY);
	SprMngr.DrawPoints(LmapPrepPix,D3DPT_LINELIST);
	SprMngr.DrawStr(INTRECT(LmapWMap[0]+LmapX,LmapWMap[1]+LmapY,LmapWMap[0]+LmapX+100,LmapWMap[1]+LmapY+15),Str::Format("Zoom: %d",LmapZoom-1),0);
	if(IfaceHold==IFACE_LMAP_OK) SprMngr.DrawSprite(LmapPBOkDw,LmapBOk[0]+LmapX,LmapBOk[1]+LmapY);
	if(IfaceHold==IFACE_LMAP_SCAN) SprMngr.DrawSprite(LmapPBScanDw,LmapBScan[0]+LmapX,LmapBScan[1]+LmapY);
	if(LmapSwitchHi) SprMngr.DrawSprite(LmapPBLoHiDw,LmapBLoHi[0]+LmapX,LmapBLoHi[1]+LmapY);
}

void FOClient::LmapLMouseDown()
{
	IfaceHold=IFACE_NONE;
	if(IsCurInRect(LmapBOk,LmapX,LmapY)) IfaceHold=IFACE_LMAP_OK;
	else if(IsCurInRect(LmapBScan,LmapX,LmapY)) IfaceHold=IFACE_LMAP_SCAN;
	else if(IsCurInRect(LmapBLoHi,LmapX,LmapY)) IfaceHold=IFACE_LMAP_LOHI;
	else if(IsCurInRect(LmapMain,LmapX,LmapY))
	{
		LmapVectX=CurX-LmapX;
		LmapVectY=CurY-LmapY;
		IfaceHold=IFACE_LMAP_MAIN;
	}
}

void FOClient::LmapMouseMove()
{
	if(IfaceHold==IFACE_LMAP_MAIN)
	{
		LmapX=CurX-LmapVectX;
		LmapY=CurY-LmapVectY;
		if(LmapX<0) LmapX=0;
		if(LmapX+LmapMain[2]>MODE_WIDTH) LmapX=MODE_WIDTH-LmapMain[2];
		if(LmapY<0) LmapY=0;
		if(LmapY+LmapMain[3]>MODE_HEIGHT) LmapY=MODE_HEIGHT-LmapMain[3];
	}
}

void FOClient::LmapLMouseUp()
{
	if(IsCurInRect(LmapBOk,LmapX,LmapY) && IfaceHold==IFACE_LMAP_OK)
	{
		ShowScreen(SCREEN_NONE);
	}
	if(IsCurInRect(LmapBScan,LmapX,LmapY) && IfaceHold==IFACE_LMAP_SCAN)
	{
		LmapZoom*=1.5f;
		if(LmapZoom>13) LmapZoom=2;
		LmapPrepareMap();
	}
	if(IsCurInRect(LmapBLoHi,LmapX,LmapY) && IfaceHold==IFACE_LMAP_LOHI)
	{
		LmapSwitchHi=!LmapSwitchHi;
		LmapPrepareMap();
	}

	IfaceHold=IFACE_NONE;
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

#define GMAP_CHECK_MAPSCR \
	do{\
		if(GmapMapScrX>GmapWMap[0]) GmapMapScrX=GmapWMap[0];\
		if(GmapMapScrY>GmapWMap[1]) GmapMapScrY=GmapWMap[1];\
		if(GmapMapScrX<GmapWMap[2]-GM_MAXX/GmapZoom) GmapMapScrX=GmapWMap[2]-GM_MAXX/GmapZoom;\
		if(GmapMapScrY<GmapWMap[3]-GM_MAXY/GmapZoom) GmapMapScrY=GmapWMap[3]-GM_MAXY/GmapZoom;\
	}while(0)

#define GMAP_LOC_ALPHA   (60)

void FOClient::GmapNullParams()
{
	GmapGroupXf=0;
	GmapGroupYf=0;
	GmapGroupX=0;
	GmapGroupY=0;
	GmapMoveX=0;
	GmapMoveY=0;
	GmapSpeedX=0;
	GmapSpeedY=0;
	GmapWait=false;
	GmapTabsLastScr=0;
	GmapCurHoldBLoc=0;
	GmapLoc.clear();
	SAFEDEL(GmapCar.Car);
	GmapCar.MasterId=0;
	GmapFog.Fill(0);
	GmapIsProc=false;
	ClearCritters();
	GmapTrace.clear();
}

void FOClient::GmapProcess()
{
	if(GmapIsProc==false)
	{
		SetCurMode(CUR_WAIT);
		return;
	}

	if(!GmapWait)
	{
		DWORD dtime_move=Timer::GameTick()-GmapMoveLastTick;
		if(dtime_move>=30)
		{
			int walk_type=GM_WALK_GROUND;
			Item* car=GmapGetCar();
			if(car) walk_type=car->Proto->MiscEx.Car.WalkType;

			float kr=(walk_type==GM_WALK_GROUND?GlobalMapKRelief[GmapRelief?GmapRelief->Get4Bit(GmapGroupX,GmapGroupY):0xF]:1.0f);
			if(walk_type==GM_WALK_GROUND) kr=GlobalMapKRelief[GmapRelief?GmapRelief->Get4Bit(GmapGroupX,GmapGroupY):0xF];
			if(car && kr!=1.0f)
			{
				float n=car->Proto->MiscEx.Car.Negotiability;
				if(n>100 && kr<1.0f) kr+=(1.0f-kr)*(n-100.0f)/100.0f;
				else if(n>100 && kr>1.0f) kr-=(kr-1.0f)*(n-100.0f)/100.0f;
				else if(n<100 && kr<1.0f) kr-=(1.0f-kr)*(100.0f-n)/100.0f;
				else if(n<100 && kr>1.0f) kr+=(kr-1.0f)*(100.0f-n)/100.0f;
			}

			GmapGroupXf+=GmapSpeedX*kr*((float)(dtime_move)/GM_MOVE_PROC_TIME);
			GmapGroupYf+=GmapSpeedY*kr*((float)(dtime_move)/GM_MOVE_PROC_TIME);

			int old_x=GmapGroupX;
			int old_y=GmapGroupY;
			DWORD last_dist=DistSqrt(GmapGroupX,GmapGroupY,GmapMoveX,GmapMoveY);

			GmapGroupX=(int)GmapGroupXf;
			GmapGroupY=(int)GmapGroupYf;

			int old_relief=(GmapRelief?GmapRelief->Get4Bit(old_x,old_y):0xF);
			int relief=(GmapRelief?GmapRelief->Get4Bit(GmapGroupX,GmapGroupY):0xF);
			if(GmapGroupX>=GM_MAXX || GmapGroupY>=GM_MAXY || GmapGroupX<0 || GmapGroupY<0 ||
				(walk_type==GM_WALK_GROUND && old_relief!=0xF && relief==0xF) ||
				(walk_type==GM_WALK_WATER && relief!=0xF))
		//	if(GmapGroupX>=GM_MAXX || GmapGroupY>=GM_MAXY || GmapGroupX<0 || GmapGroupY<0)
			{
				if(GmapGroupX>=GM_MAXX) GmapGroupX=GM_MAXX-1;
				if(GmapGroupX<0) GmapGroupX=0;
				if(GmapGroupY>=GM_MAXY) GmapGroupY=GM_MAXY-1;
				if(GmapGroupY<0) GmapGroupY=0;

				GmapGroupXf=GmapGroupX;
				GmapGroupYf=GmapGroupY;
				GmapSpeedX=0.0f;
				GmapSpeedY=0.0f;
			}

			GmapMapScrX+=(old_x-GmapGroupX)/GmapZoom;
			GmapMapScrY+=(old_y-GmapGroupY)/GmapZoom;

			GMAP_CHECK_MAPSCR;

			DWORD cur_dist=DistSqrt(GmapGroupX,GmapGroupY,GmapMoveX,GmapMoveY);
			if(!cur_dist || cur_dist>last_dist)
			{
				GmapSpeedX=0.0f;
				GmapSpeedY=0.0f;
			}

			GmapMoveLastTick=Timer::GameTick();

			if(GmapSpeedX!=0.0f || GmapSpeedY!=0.0f)
			{
				static DWORD point_tick=0;
				if(Timer::GameTick()>=point_tick)
				{
					if(GmapTrace.empty() || GmapTrace[0].first!=old_x || GmapTrace[0].second!=old_y)
						GmapTrace.push_back(IntPairVecVal(old_x,old_y));
					point_tick=Timer::GameTick()+GM_TRACE_TIME;
				}
			}
			else
			{
				GmapTrace.clear();
			}
		}

		DWORD dtime_proc=Timer::GameTick()-GmapProcLastTick;
		if(dtime_proc>=GM_MOVE_PROC_TIME)
		{
			Item* car=GmapGetCar();
			if(car && (GmapSpeedX || GmapSpeedY))
			{
				int fuel=car->Data.Car.Fuel;
				int wear=car->Data.Car.Deteoration;
				fuel-=car->Proto->MiscEx.Car.FuelConsumption;
				wear+=car->Proto->MiscEx.Car.WearConsumption;
				if(fuel<0) fuel=0;
				if(wear>car->Proto->MiscEx.Car.RunToBreak) wear=car->Proto->MiscEx.Car.RunToBreak;
				car->Data.Car.Fuel=fuel;
				car->Data.Car.Deteoration=wear;
			}

			GmapProcLastTick=Timer::GameTick();
		}
	}

	if(IfaceHold==IFACE_GMAP_TABSCRUP) // Scroll Up
	{
		if(IsCurInRect(GmapBTabsScrUp))
		{
			if(Timer::FastTick()-GmapTabsLastScr>=10)
			{
				if(GmapTabNextX) GmapTabsScrX-=(Timer::FastTick()-GmapTabsLastScr)/5;
				if(GmapTabNextY) GmapTabsScrY-=(Timer::FastTick()-GmapTabsLastScr)/5;
				if(GmapTabsScrX<0) GmapTabsScrX=0;
				if(GmapTabsScrY<0) GmapTabsScrY=0;
				GmapTabsLastScr=Timer::FastTick();
			}
		}
		else
		{
			IfaceHold=IFACE_NONE;
		}
	}

	if(IfaceHold==IFACE_GMAP_TABSCRDW) // Scroll down
	{
		if(IsCurInRect(GmapBTabsScrDn))
		{
			int tabs_count=0;
			for(int i=0,j=GmapLoc.size();i<j;i++)
			{
				GmapLocation& loc=GmapLoc[i];
				if(MsgGM->Count(STR_GM_LABELPIC_(loc.LocPid)) && ResMngr.GetIfaceSprId(Str::GetHash(MsgGM->GetStr(STR_GM_LABELPIC_(loc.LocPid))))) tabs_count++;
			}

			if(Timer::FastTick()-GmapTabsLastScr>=10)
			{
				if(GmapTabNextX)
				{
					GmapTabsScrX+=(Timer::FastTick()-GmapTabsLastScr)/5;
					if(GmapTabsScrX>GmapWTab.W()*tabs_count) GmapTabsScrX=GmapWTab.W()*tabs_count;
				}
				if(GmapTabNextY)
				{
					GmapTabsScrY+=(Timer::FastTick()-GmapTabsLastScr)/5;
					if(GmapTabsScrY>GmapWTab.H()*tabs_count) GmapTabsScrY=GmapWTab.H()*tabs_count;
				}
				GmapTabsLastScr=Timer::FastTick();
			}
		}
		else
		{
			IfaceHold=IFACE_NONE;
		}
	}
}

void FOClient::GmapDraw()
{
	if(!GmapIsProc)
	{
		SprMngr.DrawSprite(GmapWMainPic,0,0);
		return;
	}

	// World map
	int wmx=0,wmy=0;
	for(int py=0;py<GmapTilesY;py++)
	{
		wmx=0;
		for(int px=0;px<GmapTilesX;px++)
		{
			int index=py*GmapTilesX+px;
			if(index>=GmapPic.size()) continue;
			if(!GmapPic[index])
			{
				SprMngr.SurfType=RES_GLOBAL_MAP;
				GmapPic[index]=SprMngr.LoadSprite(Str::Format(GmapTilesPic,index),PT_ART_INTRFACE);
				SprMngr.SurfType=RES_NONE;
			}
			if(!GmapPic[index]) continue;
			SpriteInfo* spr_inf=SprMngr.GetSpriteInfo(GmapPic[index]);
			if(!spr_inf) continue;

			int w=spr_inf->Width;
			int h=spr_inf->Height;

			int mx1=wmx/GmapZoom+GmapMapScrX;
			int my1=wmy/GmapZoom+GmapMapScrY;
			int mx2=mx1+w/GmapZoom;
			int my2=my1+h/GmapZoom;

			int sx1=GmapWMap[0];
			int sy1=GmapWMap[1];
			int sx2=GmapWMap[2];
			int sy2=GmapWMap[3];

			if((sx1>=mx1 && sx1<=mx2 && sy1>=my1 && sy1<=my2)
			|| (sx2>=mx1 && sx2<=mx2 && sy1>=my1 && sy1<=my2)
			|| (sx1>=mx1 && sx1<=mx2 && sy2>=my1 && sy2<=my2)
			|| (sx2>=mx1 && sx2<=mx2 && sy2>=my1 && sy2<=my2)
			|| (mx1>=sx1 && mx1<=sx2 && my1>=sy1 && my1<=sy2)
			|| (mx2>=sx1 && mx2<=sx2 && my1>=sy1 && my1<=sy2)
			|| (mx1>=sx1 && mx1<=sx2 && my2>=sy1 && my2<=sy2)
			|| (mx2>=sx1 && mx2<=sx2 && my2>=sy1 && my2<=sy2))
				SprMngr.DrawSpriteSize(GmapPic[index],
					mx1,my1,w/GmapZoom,h/GmapZoom,true,false);

			wmx+=w;
			if(px==GmapTilesX-1) wmy+=h;
		}
	}

	// Global map fog
	int fog_l=0;
	int fog_r=GameOpt.GlobalMapWidth;
	int fog_t=0;
	int fog_b=GameOpt.GlobalMapHeight;
	GmapFogPix.clear();
	for(int zx=fog_l;zx<=fog_r;zx++)
	{
		for(int zy=fog_t;zy<=fog_b;zy++)
		{
			int val=GmapFog.Get2Bit(zx,zy);
			if(val==GM_FOG_NONE) continue;
			DWORD color=D3DCOLOR_ARGB(0xFF,0,0,0); //GM_FOG_FULL
			if(val==GM_FOG_SELF) color=D3DCOLOR_ARGB(0x7F,0,0,0);
			else if(val==GM_FOG_SELF2) color=D3DCOLOR_ARGB(0x3F,0,0,0);
			float x=float(zx*GM_ZONE_LEN)/GmapZoom+GmapMapScrX;
			float y=float(zy*GM_ZONE_LEN)/GmapZoom+GmapMapScrY;
			SprMngr.PrepareSquare(GmapFogPix,FLTRECT(x,y,x+GM_ZONE_LEN/GmapZoom,y+GM_ZONE_LEN/GmapZoom),color);
		}
	}
	SprMngr.DrawPoints(GmapFogPix,D3DPT_TRIANGLELIST);

	// Locations on map
	for(GmapLocationVecIt it=GmapLoc.begin();it!=GmapLoc.end();++it)
	{
		GmapLocation& loc=(*it);
		int radius=loc.Radius; //MsgGM->GetInt(STR_GM_RADIUS(loc.LocPid));
		if(radius<=0) radius=6;
		int loc_pic_x1=loc.LocWx/GmapZoom+GmapMapScrX-radius/GmapZoom;
		int loc_pic_y1=loc.LocWy/GmapZoom+GmapMapScrY-radius/GmapZoom;
		int loc_pic_x2=loc.LocWx/GmapZoom+GmapMapScrX+radius/GmapZoom;
		int loc_pic_y2=loc.LocWy/GmapZoom+GmapMapScrY+radius/GmapZoom;
		if(loc_pic_x1<=GmapWMap[2] && loc_pic_y1<=GmapWMap[3] && loc_pic_x2>=GmapWMap[0] && loc_pic_y2>=GmapWMap[1])
			SprMngr.DrawSpriteSize(GmapLocPic,loc_pic_x1,loc_pic_y1,loc_pic_x2-loc_pic_x1,loc_pic_y2-loc_pic_y1,true,false,loc.Color?loc.Color:D3DCOLOR_ARGB(GMAP_LOC_ALPHA,0,255,0));
	}

	// On map
	if(GmapWait)
	{
		DWORD pic=((Timer::GameTick()%1000)<500?GmapPLightPic0:GmapPLightPic1);
		SpriteInfo* si=SprMngr.GetSpriteInfo(pic);
		if(si) SprMngr.DrawSprite(pic,GmapGroupX/GmapZoom+GmapMapScrX-si->Width/2,GmapGroupY/GmapZoom+GmapMapScrY-si->Height/2);
	}
	else
	{
		if(GmapSpeedX || GmapSpeedY)
		{
			SpriteInfo* si;
			if(si=SprMngr.GetSpriteInfo(GmapPGr))
				SprMngr.DrawSprite(GmapPGr,GmapGroupX/GmapZoom+GmapMapScrX-si->Width/2,GmapGroupY/GmapZoom+GmapMapScrY-si->Height/2);
			if(si=SprMngr.GetSpriteInfo(GmapPTarg))
				SprMngr.DrawSprite(GmapPTarg,GmapMoveX/GmapZoom+GmapMapScrX-si->Width/2,GmapMoveY/GmapZoom+GmapMapScrY-si->Height/2);
		}
		else
		{
			SpriteInfo* si;
			if(IfaceHold==IFACE_GMAP_TOLOC)
			{
				if(si=SprMngr.GetSpriteInfo(GmapPStayDn))
					SprMngr.DrawSprite(GmapPStayDn,GmapGroupX/GmapZoom+GmapMapScrX-si->Width/2,GmapGroupY/GmapZoom+GmapMapScrY-si->Height/2);
			}
			else
			{
				if(si=SprMngr.GetSpriteInfo(GmapPStay))
					SprMngr.DrawSprite(GmapPStay,GmapGroupX/GmapZoom+GmapMapScrX-si->Width/2,GmapGroupY/GmapZoom+GmapMapScrY-si->Height/2);
			}
		}
	}

	// Trace
	static PointVec gt;
	gt.clear();
	for(IntPairVecIt it=GmapTrace.begin(),end=GmapTrace.end();it!=end;++it)
		gt.push_back(PrepPoint((*it).first/GmapZoom+GmapMapScrX,(*it).second/GmapZoom+GmapMapScrY,0xFFFF0000));
	SprMngr.DrawPoints(gt,D3DPT_POINTLIST);

	// Cut off map
	SprMngr.DrawPoints(GmapMapCutOff,D3DPT_TRIANGLELIST);

	// Tabs pics
	int cur_tabx=GmapWTabs[0]-GmapTabsScrX;
	int cur_taby=GmapWTabs[1]-GmapTabsScrY;

	for(int i=0,j=GmapLoc.size();i<j;i++)
	{
		GmapLocation& loc=GmapLoc[i];

		if(!MsgGM->Count(STR_GM_LABELPIC_(loc.LocPid))) continue;
		DWORD tab_pic=ResMngr.GetIfaceSprId(Str::GetHash(MsgGM->GetStr(STR_GM_LABELPIC_(loc.LocPid))));
		if(!tab_pic) continue;

		SprMngr.DrawSprite(GmapPWTab,cur_tabx,cur_taby);
		SprMngr.DrawSprite(tab_pic,GmapWTabLoc[0]+cur_tabx,GmapWTabLoc[1]+cur_taby);

		if(IfaceHold==IFACE_GMAP_TABBTN && GmapCurHoldBLoc==i) //Button down
			SprMngr.DrawSprite(GmapPBTabLoc,GmapBTabLoc[0]+cur_tabx,GmapBTabLoc[1]+cur_taby);

		cur_tabx+=GmapTabNextX;
		cur_taby+=GmapTabNextY;
		if(cur_tabx>GmapWTabs[2] || cur_taby>GmapWTabs[3]) break;
	}

	// Empty tabs
	while(true)
	{
		if(cur_tabx>GmapWTabs[2] || cur_taby>GmapWTabs[3]) break;
		SprMngr.DrawSprite(GmapPWBlankTab,cur_tabx,cur_taby);
		cur_tabx+=GmapTabNextX;
		cur_taby+=GmapTabNextY;
	}

	// Main pic
	SprMngr.DrawSprite(GmapWMainPic,0,0);

	switch(IfaceHold)
	{
	case IFACE_GMAP_TOWN: SprMngr.DrawSprite(GmapPBTownDw,GmapBTown[0],GmapBTown[1]); break;
	case IFACE_GMAP_TABSCRUP: SprMngr.DrawSprite(GmapPTabScrUpDw,GmapBTabsScrUp[0],GmapBTabsScrUp[1]); break;
	case IFACE_GMAP_TABSCRDW: SprMngr.DrawSprite(GmapPTabScrDwDw,GmapBTabsScrDn[0],GmapBTabsScrDn[1]); break;
	case IFACE_GMAP_INV: SprMngr.DrawSprite(GmapBInvPicDown,GmapBInv[0],GmapBInv[1]); break;
	case IFACE_GMAP_MENU: SprMngr.DrawSprite(GmapBMenuPicDown,GmapBMenu[0],GmapBMenu[1]); break;
	case IFACE_GMAP_CHA: SprMngr.DrawSprite(GmapBChaPicDown,GmapBCha[0],GmapBCha[1]); break;
	case IFACE_GMAP_PIP: SprMngr.DrawSprite(GmapBPipPicDown,GmapBPip[0],GmapBPip[1]); break;
	case IFACE_GMAP_FIX: SprMngr.DrawSprite(GmapBFixPicDown,GmapBFix[0],GmapBFix[1]); break;
	default: break;
	}

	// Car
	Item* car=GmapGetCar();
	if(car)
	{
		SprMngr.DrawSpriteSize(ResMngr.GetSprId(car->GetPicMap(),car->Proto->Dir),GmapWCar.L,GmapWCar.T,GmapWCar.W(),GmapWCar.H(),false,true);
		SprMngr.DrawStr(GmapWCar,FmtItemLook(car,ITEM_LOOK_WM_CAR),FT_CENTERX|FT_BOTTOM,COLOR_TEXT,FONT_DEFAULT);
	}

	// Day time
	int gh=GameOpt.Hour+11;
	if(gh>=24) gh-=24;
	AnimRun(GmapWDayTimeAnim,ANIMRUN_SET_FRM(gh));
	SprMngr.DrawSprite(AnimGetCurSpr(GmapWDayTimeAnim),GmapWDayTime[0],GmapWDayTime[1]);

	// Lock
	if(Chosen && !Chosen->IsGmapRule()) SprMngr.DrawStr(GmapWLock,MsgGame->GetStr(STR_GMAP_LOCKED),FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);

	// Critters
	int pos=0;
	for(CritMapIt it=HexMngr.allCritters.begin();it!=HexMngr.allCritters.end();it++,pos++)
	{
		CritterCl* cr=(*it).second;
		SprMngr.DrawStr(INTRECT(GmapWName,GmapWNameStepX*pos,GmapWNameStepY*pos),cr->GetName(),FT_NOBREAK|FT_CENTERY,cr->IsGmapRule()?COLOR_TEXT_DGREEN:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(GmapWName,GmapWNameStepX*pos,GmapWNameStepY*pos),cr->IsOffline()?"offline":"online",FT_NOBREAK|FT_CENTERR,cr->IsOffline()?COLOR_TEXT_DDRED:COLOR_TEXT_DDGREEN,FONT_SPECIAL);
	}

	// Map coord
	if(GetActiveScreen()==SCREEN_NONE && IsCurInRect(GmapWMap) && !IsLMenu())
	{
		int cx=(CurX-GmapMapScrX)*GmapZoom;
		int cy=(CurY-GmapMapScrY)*GmapZoom;
		if(GmapFog.Get2Bit(GM_ZONE(cx),GM_ZONE(cy))!=GM_FOG_FULL)
		{
			GmapLocation* cur_loc=NULL;
			for(GmapLocationVecIt it=GmapLoc.begin();it!=GmapLoc.end();++it)
			{
				GmapLocation& loc=(*it);
				DWORD radius=loc.Radius; //MsgGM->GetInt(STR_GM_RADIUS(loc.LocPid));
				if(!radius || !MsgGM->Count(STR_GM_NAME_(loc.LocPid)) || !MsgGM->Count(STR_GM_INFO_(loc.LocPid))) continue;
				if(DistSqrt(loc.LocWx,loc.LocWy,cx,cy)<=radius)
				{
					cur_loc=&loc;
					break;
				}
			}

			SpriteInfo* si=SprMngr.GetSpriteInfo(CurPDef);
			if(Chosen && (Chosen->IsPerk(PE_PATHFINDER)!=0 || Chosen->GetSkill(SK_OUTDOORSMAN)>99))
			{
				SprMngr.DrawStr(INTRECT(CurX+si->Width,CurY+si->Height,CurX+si->Width+200,CurY+si->Height+500),cur_loc?
					FmtGameText(STR_GMAP_CUR_LOC_INFO,cx,cy,GM_ZONE(cx),GM_ZONE(cy),MsgGM->GetStr(STR_GM_NAME_(cur_loc->LocPid)),MsgGM->GetStr(STR_GM_INFO_(cur_loc->LocPid))):
					FmtGameText(STR_GMAP_CUR_INFO,cx,cy,GM_ZONE(cx),GM_ZONE(cy)),0);
			}
			else if(cur_loc)
			{
				SprMngr.DrawStr(INTRECT(CurX+si->Width,CurY+si->Height,CurX+si->Width+200,CurY+si->Height+500),
					FmtGameText(STR_GMAP_LOC_INFO,MsgGM->GetStr(STR_GM_NAME_(cur_loc->LocPid)),MsgGM->GetStr(STR_GM_INFO_(cur_loc->LocPid))),0);
			}
		}
	}

	// Time
	SprMngr.DrawStr(INTRECT(GmapWTime),Str::Format("%02d",GameOpt.Day),0,COLOR_IFACE,FONT_NUM); // Day
	char mval='0'+GameOpt.Month-1+0x30; // Month
	SprMngr.DrawStr(INTRECT(GmapWTime,26,1),Str::Format("%c",mval),0,COLOR_IFACE,FONT_NUM); // Month
	SprMngr.DrawStr(INTRECT(GmapWTime,62,0),Str::Format("%04d",GameOpt.Year),0,COLOR_IFACE,FONT_NUM); // Year
	SprMngr.DrawStr(INTRECT(GmapWTime,107,0),Str::Format("%02d%02d",GameOpt.Hour,GameOpt.Minute),0,COLOR_IFACE,FONT_NUM); // Hour,Minute
}

void FOClient::GmapTownDraw()
{
	if(!GmapIsProc)
	{
		ShowScreen(SCREEN_NONE);
		return;
	}

	WORD loc_pid=GmapTownLoc.LocPid;

	if(DistSqrt(GmapTownLoc.LocWx,GmapTownLoc.LocWy,GmapGroupX,GmapGroupY)>GmapTownLoc.Radius)
	{
		ShowScreen(SCREEN_NONE);
		return;
	}

	SpriteInfo* si=SprMngr.GetSpriteInfo(GmapTownPicId);
	if(si) SprMngr.DrawSprite(GmapTownPicId,(MODE_WIDTH-si->Width)/2,(MODE_HEIGHT-si->Height)/2);

	if(!Chosen || !Chosen->IsGmapRule()) return;

	for(int i=0;i<GmapTownTextPos.size();i++)
	{
		if(GmapTownLoc.LocId!=GmapShowEntrancesLocId || !GmapShowEntrances[i]) continue;

		// Enter to entrance
		if(Chosen && Chosen->IsGmapRule())
		{
			DWORD pic=GmapPTownInPic;
			if(IfaceHold==IFACE_GMAP_TOWN_BUT && GmapTownCurButton==i) pic=GmapPTownInPicDn;
			SprMngr.DrawSprite(pic,GmapTownTextPos[i][0]+GmapPTownInOffsX,GmapTownTextPos[i][1]+GmapPTownInOffsY);
		}

		// View entrance
		DWORD pic=GmapPTownViewPic;
		if(IfaceHold==IFACE_GMAP_VIEW_BUT && GmapTownCurButton==i) pic=GmapPTownViewPicDn;
		SprMngr.DrawSprite(pic,GmapTownTextPos[i][0]+GmapPTownViewOffsX,GmapTownTextPos[i][1]+GmapPTownViewOffsY);
	}

	for(int i=0;i<GmapTownTextPos.size();i++)
	{
		if(GmapTownLoc.LocId!=GmapShowEntrancesLocId || !GmapShowEntrances[i]) continue;
		SprMngr.DrawStr(GmapTownTextPos[i],GmapTownText[i].c_str(),FT_COLORIZE);
	}
}

void FOClient::GmapLMouseDown()
{
	IfaceHold=IFACE_NONE;

	if(GetActiveScreen()==SCREEN__GM_TOWN)
	{
		for(int i=0;i<GmapTownTextPos.size();i++)
		{
			if(GmapTownLoc.LocId!=GmapShowEntrancesLocId || !GmapShowEntrances[i]) continue;
			INTRECT& r=GmapTownTextPos[i];

			// Enter to entrance
			if(Chosen && Chosen->IsGmapRule() && IsCurInRect(r,GmapPTownInOffsX,GmapPTownInOffsY) && SprMngr.IsPixNoTransp(GmapPTownInPicMask,CurX-r[0]-GmapPTownInOffsX,CurY-r[1]-GmapPTownInOffsY,false))
			{
				IfaceHold=IFACE_GMAP_TOWN_BUT;
				GmapTownCurButton=i;
				break;
			}

			// View entrance
			if(IsCurInRect(r,GmapPTownViewOffsX,GmapPTownViewOffsY) && SprMngr.IsPixNoTransp(GmapPTownViewPicMask,CurX-r[0]-GmapPTownViewOffsX,CurY-r[1]-GmapPTownViewOffsY,false))
			{
				IfaceHold=IFACE_GMAP_VIEW_BUT;
				GmapTownCurButton=i;
				break;
			}
		}

		return;
	}

	if(IsCurInRect(GmapWMap))
	{
		if(!GmapSpeedX && !GmapSpeedY)
		{
			SpriteInfo* si=SprMngr.GetSpriteInfo(GmapPStayMask);
			if(si)
			{
				int x=GmapGroupX/GmapZoom+GmapMapScrX-si->Width/2;
				int y=GmapGroupY/GmapZoom+GmapMapScrY-si->Height/2;

				if(CurX>=x && CurX<=x+si->Width && CurY>=y && CurY<=y+si->Height && SprMngr.IsPixNoTransp(GmapPStayMask,CurX-x,CurY-y,false))
				{
					IfaceHold=IFACE_GMAP_TOLOC;
					return;
				}
			}
		}

		IfaceHold=IFACE_GMAP_MAP;
	}
	else if(IsCurInRect(GmapBInv)) IfaceHold=IFACE_GMAP_INV;
	else if(IsCurInRect(GmapBMenu)) IfaceHold=IFACE_GMAP_MENU;
	else if(IsCurInRect(GmapBCha)) IfaceHold=IFACE_GMAP_CHA;
	else if(IsCurInRect(GmapBPip)) IfaceHold=IFACE_GMAP_PIP;
	else if(IsCurInRect(GmapBFix)) IfaceHold=IFACE_GMAP_FIX;
	else if(IsCurInRect(GmapBTabsScrUp))
	{
		IfaceHold=IFACE_GMAP_TABSCRUP;
		GmapTabsLastScr=Timer::FastTick();
	}
	else if(IsCurInRect(GmapBTabsScrDn))
	{
		IfaceHold=IFACE_GMAP_TABSCRDW;
		GmapTabsLastScr=Timer::FastTick();
	}
	else
	{
		int pos=0;
		for(CritMapIt it=HexMngr.allCritters.begin();it!=HexMngr.allCritters.end();it++,pos++)
		{
			CritterCl* cr=(*it).second;
			if(!IsCurInRect(INTRECT(GmapWName,GmapWNameStepX*pos,GmapWNameStepY*pos))) continue;
			LMenuTryActivate();
			break;
		}
	}

	if(!Chosen || !Chosen->IsGmapRule()) return;

	if(IsCurInRect(GmapBTown)) IfaceHold=IFACE_GMAP_TOWN;
	else if(IsCurInRect(GmapWTabs))
	{
		int tab_x=(CurX-(GmapWTabs[0]-GmapTabsScrX))%GmapWTab.W();
		int tab_y=(CurY-(GmapWTabs[1]-GmapTabsScrY))%GmapWTab.H();

		if(tab_x>=GmapBTabLoc[0] && tab_y>=GmapBTabLoc[1] && tab_x<=GmapBTabLoc[2] && tab_y<=GmapBTabLoc[3])
		{
			if(GmapTabNextX) GmapCurHoldBLoc=(CurX-(GmapWTabs[0]-GmapTabsScrX))/GmapWTab.W();
			else if(GmapTabNextY) GmapCurHoldBLoc=(CurY-(GmapWTabs[1]-GmapTabsScrY))/GmapWTab.H();
			else GmapCurHoldBLoc=-1;

			int cur_city=0;
			for(int i=0,j=GmapLoc.size();i<j;i++)
			{
				GmapLocation& loc=GmapLoc[i];

				// Skip na
				if(!MsgGM->Count(STR_GM_LABELPIC_(loc.LocPid))) continue;
				DWORD tab_pic=ResMngr.GetIfaceSprId(Str::GetHash(MsgGM->GetStr(STR_GM_LABELPIC_(loc.LocPid))));
				if(!tab_pic) continue;

				// Get
				if(GmapCurHoldBLoc==cur_city)
				{
					GmapCurHoldBLoc=i;
					IfaceHold=IFACE_GMAP_TABBTN;
					break;
				}

				cur_city++;
			}
		}
	}
}

void FOClient::GmapLMouseUp()
{
	if(GetActiveScreen()==SCREEN__GM_TOWN)
	{
		if(!IsCurInRect(GmapTownPicPos))
		{
			ShowScreen(SCREEN_NONE);
		}
		else if(IfaceHold==IFACE_GMAP_TOWN_BUT && GmapTownCurButton>=0 && GmapTownCurButton<GmapTownTextPos.size()
			&& IsCurInRect(GmapTownTextPos[GmapTownCurButton],GmapPTownInOffsX,GmapPTownInOffsY) && SprMngr.IsPixNoTransp(GmapPTownInPicMask,CurX-GmapTownTextPos[GmapTownCurButton][0]-GmapPTownInOffsX,CurY-GmapTownTextPos[GmapTownCurButton][1]-GmapPTownInOffsY,false))
		{
			Net_SendRuleGlobal(GM_CMD_TOLOCAL,GmapTownLoc.LocId,GmapTownCurButton);
		}
		else if(IfaceHold==IFACE_GMAP_VIEW_BUT && GmapTownCurButton>=0 && GmapTownCurButton<GmapTownTextPos.size()
			&& IsCurInRect(GmapTownTextPos[GmapTownCurButton],GmapPTownViewOffsX,GmapPTownViewOffsY)  && SprMngr.IsPixNoTransp(GmapPTownViewPicMask,CurX-GmapTownTextPos[GmapTownCurButton][0]-GmapPTownViewOffsX,CurY-GmapTownTextPos[GmapTownCurButton][1]-GmapPTownViewOffsY,false))
		{
			Net_SendRuleGlobal(GM_CMD_VIEW_MAP,GmapTownLoc.LocId,GmapTownCurButton);
		}

		IfaceHold=IFACE_NONE;
		return;
	}

	if(IsCurInRect(GmapWMap))
	{
		if(IfaceHold==IFACE_GMAP_TOLOC)
		{
			SpriteInfo* si=SprMngr.GetSpriteInfo(GmapPStayMask);
			if(si)
			{
				int x=GmapGroupX/GmapZoom+GmapMapScrX-si->Width/2;
				int y=GmapGroupY/GmapZoom+GmapMapScrY-si->Height/2;

				if(CurX>=x && CurX<=x+si->Width && CurY>=y && CurY<=y+si->Height && SprMngr.IsPixNoTransp(GmapPStayMask,CurX-x,CurY-y,false))
				{
					DWORD loc_id=0;
					for(GmapLocationVecIt it=GmapLoc.begin();it!=GmapLoc.end();++it)
					{
						GmapLocation& loc=(*it);
						DWORD radius=loc.Radius;
						if(!radius) radius=6;

						if(DistSqrt(loc.LocWx,loc.LocWy,GmapGroupX,GmapGroupY)<=radius)
						{
							loc_id=loc.LocId;
							break;
						}
					}

					Net_SendRuleGlobal(GM_CMD_TOLOCAL,loc_id,0);
				}
			}
		}
		else if(IfaceHold==IFACE_GMAP_MAP)
		{
			Net_SendRuleGlobal(GM_CMD_SETMOVE,(CurX-GmapMapScrX)*GmapZoom,(CurY-GmapMapScrY)*GmapZoom);
		}
	}
	else if(IfaceHold==IFACE_GMAP_TOWN && IsCurInRect(GmapBTown))
	{
		for(GmapLocationVecIt it=GmapLoc.begin();it!=GmapLoc.end();++it)
		{
			GmapLocation& loc=(*it);
			DWORD radius=loc.Radius;
			if(!radius) radius=6;

			if(DistSqrt(loc.LocWx,loc.LocWy,GmapGroupX,GmapGroupY)<=radius)
			{
				GmapTownLoc=loc;
				ShowScreen(SCREEN__GM_TOWN);
				break;
			}
		}
	}
	else if(IfaceHold==IFACE_GMAP_TABBTN && IsCurInRect(GmapWTabs))
	{
		if(GmapCurHoldBLoc<GmapLoc.size())
		{
			GmapLocation& loc=GmapLoc[GmapCurHoldBLoc];
			Net_SendRuleGlobal(GM_CMD_SETMOVE,loc.LocWx,loc.LocWy);
		}
	}
	else if(IfaceHold==IFACE_GMAP_INV && IsCurInRect(GmapBInv)) ShowScreen(SCREEN__INVENTORY);
	else if(IfaceHold==IFACE_GMAP_MENU && IsCurInRect(GmapBMenu)) ShowScreen(SCREEN__MENU_OPTION);
	else if(IfaceHold==IFACE_GMAP_CHA && IsCurInRect(GmapBCha))
	{
		ShowScreen(SCREEN__CHARACTER);
		if(Chosen && Chosen->Params[ST_UNSPENT_PERKS]) ShowScreen(SCREEN__PERK);
	}
	else if(IfaceHold==IFACE_GMAP_PIP && IsCurInRect(GmapBPip)) ShowScreen(SCREEN__PIP_BOY);
	else if(IfaceHold==IFACE_GMAP_FIX && IsCurInRect(GmapBFix)) ShowScreen(SCREEN__FIX_BOY);

	IfaceHold=IFACE_NONE;
}

void FOClient::GmapRMouseDown()
{
	if(IsCurInRect(GmapWMap))
	{
		IfaceHold=IFACE_GMAP_MOVE_MAP;
		GmapVectX=CurX-GmapMapScrX;
		GmapVectY=CurY-GmapMapScrY;
	}
}

void FOClient::GmapRMouseUp()
{
	IfaceHold=IFACE_NONE;
}

void FOClient::GmapMouseMove()
{
	if(IfaceHold==IFACE_GMAP_MOVE_MAP)
	{
		if(CurX<GmapWMap[0]) CurX=GmapWMap[0];
		if(CurY<GmapWMap[1]) CurY=GmapWMap[1];
		if(CurX>GmapWMap[2]) CurX=GmapWMap[2];
		if(CurY>GmapWMap[3]) CurY=GmapWMap[3];

		SetCurPos(CurX,CurY);

		GmapMapScrX=CurX-GmapVectX;
		GmapMapScrY=CurY-GmapVectY;

		GMAP_CHECK_MAPSCR;
	}
}

void FOClient::GmapKeyDown(BYTE dik)
{
	if(ConsoleEdit) return;
	if(Keyb::CtrlDwn || Keyb::ShiftDwn || Keyb::AltDwn) return;

	switch(dik)
	{
	case DIK_HOME:
		GmapMapScrX=(GmapWMap[2]-GmapWMap[0])/2+GmapWMap[0]-GmapGroupX/GmapZoom;
		GmapMapScrY=(GmapWMap[3]-GmapWMap[1])/2+GmapWMap[1]-GmapGroupY/GmapZoom;
		GMAP_CHECK_MAPSCR;
		break;
	case DIK_C: ShowScreen(SCREEN__CHARACTER); if(Chosen && Chosen->Params[ST_UNSPENT_PERKS]) ShowScreen(SCREEN__PERK); break;
	case DIK_I: ShowScreen(SCREEN__INVENTORY); break;
	case DIK_P: ShowScreen(SCREEN__PIP_BOY); break;
	case DIK_F: ShowScreen(SCREEN__FIX_BOY); break;
	case DIK_O: ShowScreen(SCREEN__MENU_OPTION); break;
	case DIK_SLASH: AddMess(FOMB_GAME,Str::Format("Time: %02d.%02d.%d %02d:%02d:%02d x%u",GameOpt.Day,GameOpt.Month,GameOpt.Year,GameOpt.Hour,GameOpt.Minute,GameOpt.Second,GameOpt.TimeMultiplier)); break;
	case DIK_EQUALS:
	case DIK_ADD: GameOpt.Light+=5; if(GameOpt.Light>50) GameOpt.Light=50; SetDayTime(true); break;
	case DIK_MINUS:
	case DIK_SUBTRACT: GameOpt.Light-=5; if(GameOpt.Light<0) GameOpt.Light=0; SetDayTime(true); break;
	default: break;
	}
}

void FOClient::GmapChangeZoom(float offs)
{
	if(!IsCurInRect(GmapWMap)) return;
	if(GmapZoom+offs>2.0f) return;
	if(GmapZoom+offs<0.2f) return;

	float scr_x=(float)(GmapWMap.CX()-GmapMapScrX)*GmapZoom;
	float scr_y=(float)(GmapWMap.CY()-GmapMapScrY)*GmapZoom;

	GmapZoom+=offs;

	GmapMapScrX=(float)GmapWMap.CX()-scr_x/GmapZoom;
	GmapMapScrY=(float)GmapWMap.CY()-scr_y/GmapZoom;

	GMAP_CHECK_MAPSCR;

	if(GmapMapScrX>GmapWMap.L || GmapMapScrY>GmapWMap.T ||
	   GmapMapScrX<GmapWMap.R-GM_MAXX/GmapZoom || GmapMapScrY<GmapWMap.B-GM_MAXY/GmapZoom) GmapChangeZoom(-offs);
}

Item* FOClient::GmapGetCar()
{
	return GmapCar.Car;
}

void FOClient::GmapFreeResources()
{
	for(int i=0,j=GmapPic.size();i<j;i++) GmapPic[i]=0;
	SprMngr.FreeSurfaces(RES_GLOBAL_MAP);
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::SboxDraw()
{
	SprMngr.DrawSprite(SboxPMain,SboxWMain[0]+SboxX,SboxWMain[1]+SboxY);

	if(IfaceHold==IFACE_SBOX_CANCEL) SprMngr.DrawSprite(SboxPBCancelDn,SboxBCancel[0]+SboxX,SboxBCancel[1]+SboxY);
	if(IfaceHold==IFACE_SBOX_SNEAK) SprMngr.DrawSprite(SboxPBSneakDn,SboxBSneak[0]+SboxX,SboxBSneak[1]+SboxY);
	if(IfaceHold==IFACE_SBOX_LOCKPICK) SprMngr.DrawSprite(SboxPBLockPickDn,SboxBLockpick[0]+SboxX,SboxBLockpick[1]+SboxY);
	if(IfaceHold==IFACE_SBOX_STEAL) SprMngr.DrawSprite(SboxPBStealDn,SboxBSteal[0]+SboxX,SboxBSteal[1]+SboxY);
	if(IfaceHold==IFACE_SBOX_TRAP) SprMngr.DrawSprite(SboxPBTrapsDn,SboxBTrap[0]+SboxX,SboxBTrap[1]+SboxY);
	if(IfaceHold==IFACE_SBOX_FIRSTAID) SprMngr.DrawSprite(SboxPBFirstaidDn,SboxBFirstAid[0]+SboxX,SboxBFirstAid[1]+SboxY);
	if(IfaceHold==IFACE_SBOX_DOCTOR) SprMngr.DrawSprite(SboxPBDoctorDn,SboxBDoctor[0]+SboxX,SboxBDoctor[1]+SboxY);
	if(IfaceHold==IFACE_SBOX_SCIENCE) SprMngr.DrawSprite(SboxPBScienceDn,SboxBScience[0]+SboxX,SboxBScience[1]+SboxY);
	if(IfaceHold==IFACE_SBOX_REPAIR) SprMngr.DrawSprite(SboxPBRepairDn,SboxBRepair[0]+SboxX,SboxBRepair[1]+SboxY);

	SprMngr.Flush();

	// Cancel, title
	SprMngr.DrawStr(INTRECT(SboxWMainText,SboxX,SboxY),MsgGame->GetStr(STR_SKILLDEX_NAME),FT_CENTERX|FT_CENTERY|FT_NOBREAK,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(SboxBCancelText,SboxX,SboxY),MsgGame->GetStr(STR_SKILLDEX_CANCEL),FT_CENTERY|FT_NOBREAK,COLOR_TEXT_SAND,FONT_FAT);

	// Skills
#define SBOX_DRAW_SKILL(comp,skill) \
	do{SprMngr.DrawStr(INTRECT(SboxB##comp,SboxX,SboxY-(IfaceHold==skill?1:0)),MsgGame->GetStr(STR_PARAM_NAME_SHORT_(skill)),FT_CENTERX|FT_CENTERY|FT_NOBREAK,COLOR_TEXT_SAND,FONT_FAT);\
	int sk_val=(Chosen?Chosen->GetSkill(skill):0); if(sk_val<0) sk_val=-sk_val; sk_val=CLAMP(sk_val,0,MAX_SKILL_VAL); char str[16]; sprintf(str,"%03d",sk_val); if(Chosen && Chosen->GetSkill(skill)<0) Str::ChangeValue(str,0x10);\
	SprMngr.DrawStr(INTRECT(SboxT##comp,SboxX,SboxY),str,0,COLOR_IFACE,FONT_BIG_NUM);}while(0)

	SBOX_DRAW_SKILL(Sneak,SK_SNEAK);
	SBOX_DRAW_SKILL(Lockpick,SK_LOCKPICK);
	SBOX_DRAW_SKILL(Steal,SK_STEAL);
	SBOX_DRAW_SKILL(Trap,SK_TRAPS);
	SBOX_DRAW_SKILL(FirstAid,SK_FIRST_AID);
	SBOX_DRAW_SKILL(Doctor,SK_DOCTOR);
	SBOX_DRAW_SKILL(Science,SK_SCIENCE);
	SBOX_DRAW_SKILL(Repair,SK_REPAIR);
}

void FOClient::SboxLMouseDown()
{
	IfaceHold=IFACE_NONE;
	if(IsCurInRect(SboxBCancel,SboxX,SboxY)) IfaceHold=IFACE_SBOX_CANCEL;
	else if(IsCurInRect(SboxBSneak,SboxX,SboxY)) IfaceHold=IFACE_SBOX_SNEAK;
	else if(IsCurInRect(SboxBLockpick,SboxX,SboxY)) IfaceHold=IFACE_SBOX_LOCKPICK;
	else if(IsCurInRect(SboxBSteal,SboxX,SboxY)) IfaceHold=IFACE_SBOX_STEAL;
	else if(IsCurInRect(SboxBTrap,SboxX,SboxY)) IfaceHold=IFACE_SBOX_TRAP;
	else if(IsCurInRect(SboxBFirstAid,SboxX,SboxY)) IfaceHold=IFACE_SBOX_FIRSTAID;
	else if(IsCurInRect(SboxBDoctor,SboxX,SboxY)) IfaceHold=IFACE_SBOX_DOCTOR;
	else if(IsCurInRect(SboxBScience,SboxX,SboxY)) IfaceHold=IFACE_SBOX_SCIENCE;
	else if(IsCurInRect(SboxBRepair,SboxX,SboxY)) IfaceHold=IFACE_SBOX_REPAIR;
	else if(IsCurInRect(SboxWMain,SboxX,SboxY))
	{
		SboxVectX=CurX-SboxX;
		SboxVectY=CurY-SboxY;
		IfaceHold=IFACE_SBOX_MAIN;
	}
}

void FOClient::SboxLMouseUp()
{
	if(!Chosen || (IfaceHold==IFACE_SBOX_CANCEL && IsCurInRect(SboxBCancel,SboxX,SboxY)))
	{
		ShowScreen(SCREEN_NONE);
	}
	else
	{
		WORD cur_skill;
		if(IfaceHold==IFACE_SBOX_SNEAK && IsCurInRect(SboxBSneak,SboxX,SboxY)) cur_skill=SK_SNEAK;
		else if(IfaceHold==IFACE_SBOX_LOCKPICK && IsCurInRect(SboxBLockpick,SboxX,SboxY)) cur_skill=SK_LOCKPICK;
		else if(IfaceHold==IFACE_SBOX_STEAL && IsCurInRect(SboxBSteal,SboxX,SboxY)) cur_skill=SK_STEAL;
		else if(IfaceHold==IFACE_SBOX_TRAP && IsCurInRect(SboxBTrap,SboxX,SboxY)) cur_skill=SK_TRAPS;
		else if(IfaceHold==IFACE_SBOX_FIRSTAID && IsCurInRect(SboxBFirstAid,SboxX,SboxY)) cur_skill=SK_FIRST_AID;
		else if(IfaceHold==IFACE_SBOX_DOCTOR && IsCurInRect(SboxBDoctor,SboxX,SboxY)) cur_skill=SK_DOCTOR;
		else if(IfaceHold==IFACE_SBOX_SCIENCE && IsCurInRect(SboxBScience,SboxX,SboxY)) cur_skill=SK_SCIENCE;
		else if(IfaceHold==IFACE_SBOX_REPAIR && IsCurInRect(SboxBRepair,SboxX,SboxY)) cur_skill=SK_REPAIR;
		else
		{
			IfaceHold=IFACE_NONE;
			return;
		}

		if(ShowScreenType)
		{
			if(ShowScreenNeedAnswer) Net_SendScreenAnswer(cur_skill,"");
			ShowScreen(SCREEN_NONE);
			return;
		}
		else
		{
			if(cur_skill==SK_SNEAK)
			{
				if(!SboxUseOn.IsSmth() || (SboxUseOn.IsCritter() && SboxUseOn.GetId()==Chosen->GetId()))
				{
					SetAction(CHOSEN_USE_SKL_ON_CRITTER,SK_SNEAK);
					ShowScreen(SCREEN_NONE);
				}
				IfaceHold=IFACE_NONE;
				return;
			}

			CurSkill=cur_skill;
			if(SboxUseOn.IsSmth())
			{
				if(SboxUseOn.IsCritter())
				{
					CritterCl* cr=GetCritter(SboxUseOn.GetId());
					if(cr) SetAction(CHOSEN_USE_SKL_ON_CRITTER,CurSkill,cr->GetId(),Chosen->GetFullRate());
				}
				else if(SboxUseOn.IsItem())
				{
					ItemHex* item=GetItem(SboxUseOn.GetId());
					if(item)
					{
						if(item->IsScenOrGrid())
							SetAction(CHOSEN_USE_SKL_ON_SCEN,CurSkill,item->GetProtoId(),item->GetHexX(),item->GetHexY());
						else
							SetAction(CHOSEN_USE_SKL_ON_ITEM,false,CurSkill,item->GetId());
					}
				}
				else if(SboxUseOn.IsContItem())
				{
					Item* item=Chosen->GetItem(SboxUseOn.GetId());
					if(item) SetAction(CHOSEN_USE_SKL_ON_ITEM,true,CurSkill,item->GetId());
				}

				SboxUseOn.Clear();
				ShowScreen(SCREEN_NONE);
			}
			else
			{
				ShowScreen(SCREEN_NONE);
				SetCurMode(CUR_USE_SKILL);
			}
		}
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::SboxMouseMove()
{
	if(IfaceHold==IFACE_SBOX_MAIN)
	{
		SboxX=CurX-SboxVectX;
		SboxY=CurY-SboxVectY;

		if(SboxX<0) SboxX=0;
		if(SboxX+SboxWMain[2]>MODE_WIDTH) SboxX=MODE_WIDTH-SboxWMain[2];
		if(SboxY<0) SboxY=0;
		if(SboxY+SboxWMain[3]>MODE_HEIGHT) SboxY=MODE_HEIGHT-SboxWMain[3];
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::MoptDraw()
{
	SprMngr.DrawSprite(Singleplayer?MoptSingleplayerMainPic:MoptMainPic,MoptMain[0],MoptMain[1]);

	if(Singleplayer)
	{
		if(IfaceHold==IFACE_MOPT_SAVEGAME) SprMngr.DrawSprite(MoptSaveGamePicDown,MoptSaveGame[0],MoptSaveGame[1]);
		else if(IfaceHold==IFACE_MOPT_LOADGAME) SprMngr.DrawSprite(MoptLoadGamePicDown,MoptLoadGame[0],MoptLoadGame[1]);
		SprMngr.DrawStr(INTRECT(MoptSaveGame,0,IfaceHold==IFACE_MOPT_SAVEGAME?-1:0),MsgGame->GetStr(STR_MENUOPT_SAVEGAME),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
		SprMngr.DrawStr(INTRECT(MoptLoadGame,0,IfaceHold==IFACE_MOPT_LOADGAME?-1:0),MsgGame->GetStr(STR_MENUOPT_LOADGAME),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	}

	if(IfaceHold==IFACE_MOPT_OPTIONS) SprMngr.DrawSprite(MoptOptionsPicDown,MoptOptions[0],MoptOptions[1]);
	else if(IfaceHold==IFACE_MOPT_EXIT) SprMngr.DrawSprite(MoptExitPicDown,MoptExit[0],MoptExit[1]);
	else if(IfaceHold==IFACE_MOPT_RESUME) SprMngr.DrawSprite(MoptResumePicDown,MoptResume[0],MoptResume[1]);
	SprMngr.DrawStr(INTRECT(MoptOptions,0,IfaceHold==IFACE_MOPT_OPTIONS?-1:0),MsgGame->GetStr(STR_MENUOPT_OPTIONS),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(MoptResume,0,IfaceHold==IFACE_MOPT_RESUME?-1:0),MsgGame->GetStr(STR_MENUOPT_RESUME),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(MoptExit,0,IfaceHold==IFACE_MOPT_EXIT?-1:0),MsgGame->GetStr(STR_MENUOPT_EXIT),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
}

void FOClient::MoptLMouseDown()
{
	IfaceHold=IFACE_NONE;

	if(Singleplayer && IsCurInRect(MoptSaveGame,0,0)) IfaceHold=IFACE_MOPT_SAVEGAME;
	else if(Singleplayer && IsCurInRect(MoptLoadGame,0,0)) IfaceHold=IFACE_MOPT_LOADGAME;
	else if(IsCurInRect(MoptOptions,0,0)) IfaceHold=IFACE_MOPT_OPTIONS;
	else if(IsCurInRect(MoptExit,0,0)) IfaceHold=IFACE_MOPT_EXIT;
	else if(IsCurInRect(MoptResume,0,0)) IfaceHold=IFACE_MOPT_RESUME;
}

void FOClient::MoptLMouseUp()
{
	if(IfaceHold==IFACE_MOPT_SAVEGAME && Singleplayer && IsCurInRect(MoptSaveGame,0,0))
	{
		SaveLoadLoginScreen=false;
		SaveLoadSave=true;
		ShowScreen(SCREEN__SAVE_LOAD);
	}
	else if(IfaceHold==IFACE_MOPT_LOADGAME && Singleplayer && IsCurInRect(MoptLoadGame,0,0))
	{
		SaveLoadLoginScreen=false;
		SaveLoadSave=false;
		ShowScreen(SCREEN__SAVE_LOAD);
	}
	else if(IfaceHold==IFACE_MOPT_OPTIONS && IsCurInRect(MoptOptions,0,0)) AddMess(FOMB_GAME,MsgGame->GetStr(STR_OPTIONS_NOT_AVIABLE));
	else if(IfaceHold==IFACE_MOPT_EXIT && IsCurInRect(MoptExit,0,0)) NetDisconnect();
	else if(IfaceHold==IFACE_MOPT_RESUME && IsCurInRect(MoptResume,0,0)) ShowScreen(SCREEN_NONE);

	IfaceHold=IFACE_NONE;
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::CreditsDraw()
{
	SprMngr.DrawStr(INTRECT(0,CreditsYPos,MODE_WIDTH,MODE_HEIGHT+50),
		MsgGame->GetStr(CreaditsExt?STR_GAME_CREDITS_EXT:STR_GAME_CREDITS),FT_CENTERX|FT_COLORIZE,COLOR_TEXT,FONT_BIG);

	if(Timer::FastTick()>=CreditsNextTick)
	{
		CreditsYPos--;
		DWORD wait=MsgGame->GetInt(STR_GAME_CREDITS_SPEED);
		wait=CLAMP(wait,10,1000);
		CreditsNextTick=Timer::FastTick()+wait;
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================
const int ShowStats[]={ST_ARMOR_CLASS,ST_ACTION_POINTS,ST_CARRY_WEIGHT,ST_MELEE_DAMAGE,ST_NORMAL_RESIST,
	ST_POISON_RESISTANCE,ST_RADIATION_RESISTANCE,ST_SEQUENCE,ST_HEALING_RATE,ST_CRITICAL_CHANCE};
const int ShowStatsCnt=sizeof(ShowStats)/sizeof(ShowStats[0]);

void FOClient::ChaPrepareSwitch()
{
	ChaSwitchText[CHA_SWITCH_PERKS].clear();
	ChaSwitchText[CHA_SWITCH_KARMA].clear();
	ChaSwitchText[CHA_SWITCH_KILLS].clear();

	if(!Chosen) return;

	SwitchElementVec& perks=ChaSwitchText[CHA_SWITCH_PERKS];
	SwitchElementVec& karma=ChaSwitchText[CHA_SWITCH_KARMA];
	SwitchElementVec& kills=ChaSwitchText[CHA_SWITCH_KILLS];

	// Perks
	// Traits
	for(int i=TRAIT_BEGIN;i<=TRAIT_END;i++)
	{
		if(!Chosen->IsPerk(i)) continue;
		if(perks.empty()) perks.push_back(SwitchElement(STR_TRAITS_NAME,STR_TRAITS_DESC,SKILLDEX_TRAITS,FT_CENTERX));
		perks.push_back(SwitchElement(STR_PARAM_NAME_(i),STR_PARAM_DESC_(i),SKILLDEX_PARAM(i),0));
	}

	// Perks
	bool is_add=false;
	for(int i=PERK_BEGIN;i<=PERK_END;i++)
	{
		if(!Chosen->IsPerk(i)) continue;

		// Title
		if(!is_add)
		{
			perks.push_back(SwitchElement(STR_PERKS_NAME,STR_PERKS_DESC,SKILLDEX_PERKS,FT_CENTERX));
			is_add=true;
		}

		// Perk info
		perks.push_back(SwitchElement(STR_PARAM_NAME_(i),STR_PARAM_DESC_(i),SKILLDEX_PARAM(i),0));

		// Perk count
		if(Chosen->GetParam(i)>1) StringCopy(perks[perks.size()-1].Addon,Str::Format(" (%u)",Chosen->GetParam(i)));
	}

	// Karma
	// General karma
	karma.push_back(SwitchElement(STR_KARMA_GEN_GEN_NAME,STR_KARMA_GEN_GEN_DESC,SKILLDEX_KARMA,0));
	int karma_gen_count=MsgGame->GetInt(STR_KARMA_GEN_COUNT);
	for(int i=0;i<karma_gen_count;i++)
	{
		int val=MsgGame->GetInt(STR_KARMA_GEN_VAL_(i));
		if(Chosen->GetParam(ST_KARMA)<val) continue;
		string karma_info=MsgGame->GetStr(STR_KARMA_GEN_NAME_(i));
		StringCopy(karma[karma.size()-1].Addon,FmtGameText(STR_KARMA_GEN_GEN_NAME2,Chosen->GetParam(ST_KARMA),karma_info.c_str()));
		karma[karma.size()-1].PictureId=MsgGame->GetInt(STR_KARMA_GEN_SKILLDEX_(i));
		break;
	}

	// Karma perks
	for(int i=KARMA_BEGIN;i<=KARMA_END;i++)
	{
		if(!Chosen->IsPerk(i)) continue;

		// Karma perks info
		karma.push_back(SwitchElement(STR_PARAM_NAME_(i),STR_PARAM_DESC_(i),SKILLDEX_PARAM(i),0));
	}

	// Town reputation
	is_add=false;
	for(int i=REPUTATION_BEGIN;i<=REPUTATION_END;i++)
	{
		int val=Chosen->Params[i];
		if(val==0x80000000) continue;

		// Title
		if(!is_add)
		{
			karma.push_back(SwitchElement(STR_TOWNREP_TITLE_NAME,STR_TOWNREP_TITLE_DESC,SKILLDEX_REPUTATION,FT_CENTERX));
			is_add=true;
		}

		// Reputation
		karma.push_back(SwitchElement(STR_PARAM_NAME_(i),STR_TOWNREP_RATIO_DESC_(val),SKILLDEX_REPUTATION_RATIO(val),0));

		// Perk count
		StringCopy(karma[karma.size()-1].Addon,Str::Format(": %s %d",MsgGame->GetStr(STR_TOWNREP_RATIO_NAME_(val)),val));
	}

	// Addiction
	is_add=false;
	for(int i=ADDICTION_BEGIN;i<=ADDICTION_END;i++)
	{
		if(!Chosen->IsPerk(i)) continue;

		// Addiction title
		if(!is_add)
		{
			karma.push_back(SwitchElement(STR_ADDICT_TITLE_NAME,STR_ADDICT_TITLE_DESC,SKILLDEX_DRUG_ADDICT,FT_CENTERX));
			is_add=true;
		}

		// Addiction info
		karma.push_back(SwitchElement(STR_PARAM_NAME_(i),STR_PARAM_DESC_(i),SKILLDEX_PARAM(i),0));
	}

	// Kills
	for(int i=KILL_BEGIN;i<=KILL_END;i++)
	{
		if(!Chosen->GetParam(i)) continue;

		kills.push_back(SwitchElement(STR_PARAM_NAME_(i),STR_PARAM_DESC_(i),SKILLDEX_PARAM(i),0));
		StringCopy(kills[kills.size()-1].Addon,Str::DWtoA(Chosen->GetParam(i)));
	}
}

#define CHA_PARAM(index) (is_reg?cr->Params[index]:cr->GetParam(index))
void FOClient::ChaDraw(bool is_reg)
{
	CritterCl* cr=(is_reg?RegNewCr:Chosen);
	if(!cr) return;

	if(is_reg)
		SprMngr.DrawSprite(RegPMain,RegWMain[0]+ChaX,RegWMain[1]+ChaY);
	else
		SprMngr.DrawSprite(ChaPMain,ChaWMain[0]+ChaX,ChaWMain[1]+ChaY);

	switch(IfaceHold)
	{
	case IFACE_CHA_PRINT: SprMngr.DrawSprite(ChaPBPrintDn,ChaBPrint[0]+ChaX,ChaBPrint[1]+ChaY); break;
	case IFACE_CHA_OK: SprMngr.DrawSprite(ChaPBOkDn,ChaBOk[0]+ChaX,ChaBOk[1]+ChaY); break;
	case IFACE_CHA_CANCEL: SprMngr.DrawSprite(ChaPBCancelDn,ChaBCancel[0]+ChaX,ChaBCancel[1]+ChaY); break;
	case IFACE_CHA_NAME: SprMngr.DrawSprite(ChaPBNameDn,ChaBName[0]+ChaX,ChaBName[1]+ChaY); break;
	case IFACE_CHA_AGE: SprMngr.DrawSprite(ChaPBAgeDn,ChaBAge[0]+ChaX,ChaBAge[1]+ChaY); break;
	case IFACE_CHA_SEX: SprMngr.DrawSprite(ChaPBSexDn,ChaBSex[0]+ChaX,ChaBSex[1]+ChaY); break;
	default: break;
	}

	if(ChaSkilldexPic>=0)
	{
		DWORD spr_id=ResMngr.GetSkDxSprId(Str::GetHash(FONames::GetPictureName(ChaSkilldexPic)));
		if(spr_id) SprMngr.DrawSprite(spr_id,ChaWPic[0]+ChaX,ChaWPic[1]+ChaY);
		else ChaSkilldexPic=-1;
	}

	// Traits button
	if(is_reg)
	{
		if(IfaceHold==IFACE_REG_TRAIT_L && IsCurInRect(RegBTraitL,RegTraitNum*RegTraitNextX+ChaX,RegTraitNum*RegTraitNextY+ChaY))
			SprMngr.DrawSprite(RegPBTraitDn,RegBTraitL[0]+RegTraitNum*RegTraitNextX+ChaX,RegBTraitL[1]+RegTraitNum*RegTraitNextY+ChaY);

		if(IfaceHold==IFACE_REG_TRAIT_R && IsCurInRect(RegBTraitR,RegTraitNum*RegTraitNextX+ChaX,RegTraitNum*RegTraitNextY+ChaY))
			SprMngr.DrawSprite(RegPBTraitDn,RegBTraitR[0]+RegTraitNum*RegTraitNextX+ChaX,RegBTraitR[1]+RegTraitNum*RegTraitNextY+ChaY);
	}

	// Switch
	if(!is_reg)
	{
		switch(ChaCurSwitch)
		{
		case CHA_SWITCH_PERKS: SprMngr.DrawSprite(ChaPBSwitchPerks,ChaBSwitch[0]+ChaX,ChaBSwitch[1]+ChaY); break;
		case CHA_SWITCH_KARMA: SprMngr.DrawSprite(ChaPBSwitchKarma,ChaBSwitch[0]+ChaX,ChaBSwitch[1]+ChaY); break;
		case CHA_SWITCH_KILLS: SprMngr.DrawSprite(ChaPBSwitchKills,ChaBSwitch[0]+ChaX,ChaBSwitch[1]+ChaY); break;
		default: break;
		}

		if(IfaceHold==IFACE_CHA_SW_SCRUP) SprMngr.DrawSprite(ChaPBSwitchScrUpDn,ChaBSwitchScrUp[0]+ChaX,ChaBSwitchScrUp[1]+ChaY);
		else SprMngr.DrawSprite(ChaPBSwitchScrUpUp,ChaBSwitchScrUp[0]+ChaX,ChaBSwitchScrUp[1]+ChaY);
		if(IfaceHold==IFACE_CHA_SW_SCRDN) SprMngr.DrawSprite(ChaPBSwitchScrDnDn,ChaBSwitchScrDn[0]+ChaX,ChaBSwitchScrDn[1]+ChaY);
		else SprMngr.DrawSprite(ChaPBSwitchScrDnUp,ChaBSwitchScrDn[0]+ChaX,ChaBSwitchScrDn[1]+ChaY);
	}

	// Print, Ok, Cancel button texts
	SprMngr.DrawStr(INTRECT(ChaBPrintText,ChaX,ChaY),MsgGame->GetStr(STR_CHA_PRINT),FT_NOBREAK|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(ChaBOkText,ChaX,ChaY),MsgGame->GetStr(STR_CHA_OK),FT_NOBREAK|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(ChaBCancelText,ChaX,ChaY),MsgGame->GetStr(STR_CHA_CANCEL),FT_NOBREAK|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);

	// Switch
	if(!is_reg)
	{
		int sw=(ChaBSwitch[2]-ChaBSwitch[0])/3;
		int sh=ChaBSwitch[3]-ChaBSwitch[1];
		SprMngr.DrawStr(INTRECT(INTRECT(ChaBSwitch[0],ChaBSwitch[1],ChaBSwitch[0]+sw,ChaBSwitch[3]),ChaX,ChaY-(ChaCurSwitch==CHA_SWITCH_PERKS?2:0)),MsgGame->GetStr(STR_SWITCH_PERKS_NAME),FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
		SprMngr.DrawStr(INTRECT(INTRECT(ChaBSwitch[0]+sw,ChaBSwitch[1],ChaBSwitch[0]+sw+sw,ChaBSwitch[3]),ChaX,ChaY-(ChaCurSwitch==CHA_SWITCH_KARMA?2:0)),MsgGame->GetStr(STR_SWITCH_KARMA_NAME),FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
		SprMngr.DrawStr(INTRECT(INTRECT(ChaBSwitch[0]+sw+sw,ChaBSwitch[1],ChaBSwitch[2],ChaBSwitch[3]),ChaX,ChaY-(ChaCurSwitch==CHA_SWITCH_KILLS?2:0)),MsgGame->GetStr(STR_SWITCH_KILLS_NAME),FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);

		SwitchElementVec& text=ChaSwitchText[ChaCurSwitch];
		int scroll=ChaSwitchScroll[ChaCurSwitch];
		int end_line=(ChaTSwitch[3]-ChaTSwitch[1])/11+scroll;

		// Perks, Karma
		if(ChaCurSwitch==CHA_SWITCH_PERKS || ChaCurSwitch==CHA_SWITCH_KARMA)
		{
			for(int i=0;i<text.size();i++)
			{
				if(i>=end_line) break;
				if(i<scroll) continue;
				SwitchElement& e=text[i];
				SprMngr.DrawStr(INTRECT(ChaTSwitch,ChaX,ChaY+(i-scroll)*11),Str::Format("%s%s",MsgGame->GetStr(e.NameStrNum),e.Addon),e.DrawFlags);
			}
		}
		// Kills
		else if(ChaCurSwitch==CHA_SWITCH_KILLS)
		{
			for(int i=0;i<text.size();i++)
			{
				if(i>=end_line) break;
				if(i<scroll) continue;

				SwitchElement& e=text[i];
				SprMngr.DrawStr(INTRECT(ChaTSwitch,ChaX,ChaY+(i-scroll)*11),MsgGame->GetStr(e.NameStrNum),e.DrawFlags);
				SprMngr.DrawStr(INTRECT(INTRECT(ChaTSwitch[2]-35,ChaTSwitch[1],ChaTSwitch[2],ChaTSwitch[3]),ChaX,ChaY+(i-scroll)*11),e.Addon,0);
			}
		}
	}

	// Special
	for(int i=ST_STRENGTH;i<=ST_LUCK;++i)
	{
		// Text
		SprMngr.DrawStr(INTRECT(ChaWSpecialText,ChaX+ChaWSpecialNextX*i,ChaY+ChaWSpecialNextY*i),MsgGame->GetStr(STR_INV_SHORT_SPECIAL_(i)),FT_NOBREAK|FT_CENTERX,COLOR_TEXT_SAND,FONT_BIG);

		// Value
		int val=CHA_PARAM(i);
		val=CLAMP(val,0,99);
		char str[32];
		sprintf_s(str,"%02d",val);
		if(val<1 || val>10) Str::ChangeValue(str,0x10);
		SprMngr.DrawStr(INTRECT(ChaWSpecialValue,ChaX+ChaWSpecialNextX*i,ChaY+ChaWSpecialNextY*i),str,FT_NOBREAK,COLOR_IFACE,FONT_BIG_NUM);

		// Str level
		if(is_reg)
			SprMngr.DrawStr(INTRECT(ChaWSpecialLevel,ChaX+ChaWSpecialNextX*i,ChaY+ChaWSpecialNextY*i),MsgGame->GetStr(STR_STAT_LEVEL_ABB_(val)),FT_NOBREAK|FT_CENTERY);
		else
			SprMngr.DrawStr(INTRECT(ChaWSpecialLevel,ChaX+ChaWSpecialNextX*i,ChaY+ChaWSpecialNextY*i),MsgGame->GetStr(STR_STAT_LEVEL_(val)),FT_NOBREAK|FT_CENTERY);
	}

	// Unspent
	if(is_reg)
	{
		int unspent=40;
		for(int i=ST_STRENGTH;i<=ST_LUCK;i++) unspent-=cr->ParamsReg[i];
		SprMngr.DrawStr(INTRECT(RegWUnspentSpecial,ChaX,ChaY),Str::Format("%02d",unspent),FT_NOBREAK,COLOR_IFACE,FONT_BIG_NUM);
		SprMngr.DrawStr(INTRECT(RegWUnspentSpecialText,ChaX,ChaY),MsgGame->GetStr(STR_REG_SPECIAL_SUM),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	}

	// Skill
	SprMngr.DrawStr(INTRECT(ChaWSkillText,ChaX,ChaY),MsgGame->GetStr(STR_CHA_SKILLS),FT_NOBREAK|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);

	for(int i=SKILL_BEGIN;i<=SKILL_END;i++)
	{
		int offs=i-SKILL_BEGIN;
		// Name	
		SprMngr.DrawStr(INTRECT(ChaWSkillName,ChaX+ChaWSkillNextX*offs,ChaY+ChaWSkillNextY*offs),MsgGame->GetStr(STR_PARAM_NAME_(i)),FT_NOBREAK,cr->IsTagSkill(i)?0xFFAAAAAA:COLOR_TEXT);
		// Value
		SprMngr.DrawStr(INTRECT(ChaWSkillValue,ChaX+ChaWSkillNextX*offs,ChaY+ChaWSkillNextY*offs),Str::Format("%d%%",CLAMP(CHA_PARAM(i)+(is_reg?0:ChaSkillUp[offs]),-MAX_SKILL_VAL,MAX_SKILL_VAL)),FT_NOBREAK,cr->IsTagSkill(i)?0xFFAAAAAA:COLOR_TEXT);
	}

	if(is_reg)
	{
		SprMngr.DrawStr(INTRECT(ChaWUnspentSPText,ChaX,ChaY),MsgGame->GetStr(STR_REG_UNSPENT_TAGS),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
		SprMngr.DrawStr(INTRECT(ChaWUnspentSP,ChaX,ChaY),Str::Format("%02d",(cr->Params[TAG_SKILL1]==0?1:0)+(cr->Params[TAG_SKILL2]==0?1:0)+(cr->Params[TAG_SKILL3]==0?1:0)),FT_NOBREAK,COLOR_IFACE,FONT_BIG_NUM);
	}
	else
	{
		SprMngr.DrawStr(INTRECT(ChaWUnspentSPText,ChaX,ChaY),MsgGame->GetStr(STR_CHA_UNSPENT_SP),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
		SprMngr.DrawStr(INTRECT(ChaWUnspentSP,ChaX,ChaY),Str::Format("%02d",ChaUnspentSkillPoints),FT_NOBREAK,COLOR_IFACE,FONT_BIG_NUM);
	}

	// Tips
	SprMngr.DrawStr(INTRECT(ChaWName,ChaX,ChaY),ChaName,FT_COLORIZE,COLOR_TEXT_BLACK,FONT_THIN);
	SprMngr.DrawStr(INTRECT(ChaWDesc,ChaX,ChaY),ChaDesc,FT_COLORIZE,COLOR_TEXT_BLACK);

	// Level
	if(!is_reg)
	{
		SprMngr.DrawStr(INTRECT(ChaWLevel,ChaX,ChaY),FmtGameText(STR_CHA_LEVEL,cr->GetParam(ST_LEVEL)),FT_COLORIZE);
		SprMngr.DrawStr(INTRECT(ChaWExp,ChaX,ChaY),FmtGameText(STR_CHA_EXPERIENCE,cr->GetParam(ST_EXPERIENCE)),FT_COLORIZE);
		SprMngr.DrawStr(INTRECT(ChaWNextLevel,ChaX,ChaY),FmtGameText(STR_CHA_NEXT_LEVEL,NextLevel(cr->GetParam(ST_LEVEL))),FT_COLORIZE);
	}

	// Name
	SprMngr.DrawStr(INTRECT(ChaBName,ChaX,ChaY-(IfaceHold==IFACE_CHA_NAME?1:0)),cr->GetName(),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);

	// Age
	SprMngr.DrawStr(INTRECT(ChaBAge,ChaX,ChaY-(IfaceHold==IFACE_CHA_AGE?1:0)),Str::Format("%02d",(is_reg?cr->ParamsReg[ST_AGE]:cr->GetParam(ST_AGE))),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);

	// Sex
	if((is_reg?cr->ParamsReg[ST_GENDER]:cr->GetParam(ST_GENDER))==GENDER_MALE)
		SprMngr.DrawStr(INTRECT(ChaBSex,ChaX,ChaY-(IfaceHold==IFACE_CHA_SEX?1:0)),MsgGame->GetStr(STR_MALE_NAME),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	else
		SprMngr.DrawStr(INTRECT(ChaBSex,ChaX,ChaY-(IfaceHold==IFACE_CHA_SEX?1:0)),MsgGame->GetStr(STR_FEMALE_NAME),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);

	// Damage
	// Life
	if(is_reg)
		SprMngr.DrawStr(INTRECT(ChaWDmgLife,ChaX,ChaY),FmtGameText(STR_DMG_LIFE,cr->Params[ST_MAX_LIFE],cr->Params[ST_MAX_LIFE]),FT_NOBREAK);
	else
		SprMngr.DrawStr(INTRECT(ChaWDmgLife,ChaX,ChaY),FmtGameText(STR_DMG_LIFE,cr->GetParam(ST_CURRENT_HP),cr->GetParam(ST_MAX_LIFE)),FT_NOBREAK);

	// Body damages
	for(int i=DAMAGE_BEGIN;i<=DAMAGE_END;++i)
	{
		int offs=i-DAMAGE_BEGIN;
		DWORD color;
		if(i==DAMAGE_RADIATED) color=(CHA_PARAM(ST_RADIATION_LEVEL)?COLOR_TEXT:COLOR_TEXT_DARK);
		else if(i==DAMAGE_POISONED) color=(CHA_PARAM(ST_POISONING_LEVEL)?COLOR_TEXT:COLOR_TEXT_DARK);
		else color=(CHA_PARAM(i)?COLOR_TEXT:COLOR_TEXT_DARK);
		SprMngr.DrawStr(INTRECT(ChaWDmg,ChaX+ChaWDmgNextX*offs,ChaY+ChaWDmgNextY*offs),MsgGame->GetStr(STR_PARAM_NAME_(i)),FT_NOBREAK,color);
	}

	// Secondary stats
	for(int i=0;i<ShowStatsCnt;++i)
	{
		// Name
		SprMngr.DrawStr(INTRECT(ChaWStatsName,ChaX+ChaWStatsNextX*i,ChaY+ChaWStatsNextY*i),MsgGame->GetStr(STR_PARAM_NAME_SHORT_(ShowStats[i])),FT_NOBREAK,COLOR_TEXT);
		// Value
		int val=CHA_PARAM(ShowStats[i]);
		const char* str;
		switch(ShowStats[i])
		{
		case ST_CARRY_WEIGHT: str=Str::Format("%d",val/1000); break;
		case ST_NORMAL_RESIST:
		case ST_CRITICAL_CHANCE: str=Str::Format("%d%%",val); break;
		default: str=Str::Format("%d",val); break;
		}
		SprMngr.DrawStr(INTRECT(ChaWStatsValue,ChaX+ChaWStatsNextX*i,ChaY+ChaWStatsNextY*i),str,FT_NOBREAK,COLOR_TEXT);
	}

	// Traits text
	if(is_reg)
	{
		// Left
		for(int i=TRAIT_BEGIN,k=0;i<TRAIT_BEGIN+TRAIT_COUNT/2;++i,++k)
		{
			DWORD col=(i==TRAIT_SEX_APPEAL?COLOR_TEXT_RED:COLOR_TEXT);
			SprMngr.DrawStr(INTRECT(RegWTraitL,RegTraitNextX*k+ChaX,RegTraitNextY*k+ChaY),MsgGame->GetStr(STR_PARAM_NAME_(i)),FT_NOBREAK,CHA_PARAM(i)?0xFFAAAAAA:col);
		}
		// Right
		for(int i=TRAIT_BEGIN+TRAIT_COUNT/2,k=0;i<=TRAIT_END;++i,++k)
		{
			DWORD col=(i==TRAIT_SEX_APPEAL?COLOR_TEXT_RED:COLOR_TEXT);
			SprMngr.DrawStr(INTRECT(RegWTraitR,RegTraitNextX*k+ChaX,RegTraitNextY*k+ChaY),MsgGame->GetStr(STR_PARAM_NAME_(i)),FT_NOBREAK,CHA_PARAM(i)?0xFFAAAAAA:col);
		}
	}

	// Slider
	if(!is_reg)
	{
		SprMngr.DrawSprite(ChaPWSlider,ChaCurSkill*ChaWSkillNextX+ChaWSliderX+ChaX,ChaCurSkill*ChaWSkillNextY+ChaWSliderY+ChaY);

		switch(IfaceHold)
		{
		case IFACE_CHA_PLUS: SprMngr.DrawSprite(ChaPBSliderPlusDn,ChaBSliderPlus[0]+ChaCurSkill*ChaWSkillNextX+ChaX,ChaBSliderPlus[1]+ChaCurSkill*ChaWSkillNextY+ChaY); break;
		case IFACE_CHA_MINUS: SprMngr.DrawSprite(ChaPBSliderMinusDn,ChaBSliderMinus[0]+ChaCurSkill*ChaWSkillNextX+ChaX,ChaBSliderMinus[1]+ChaCurSkill*ChaWSkillNextY+ChaY); break;
		default: break;
		}
	}
	else
	{
		switch(IfaceHold)
		{
		case IFACE_REG_SPEC_PL: SprMngr.DrawSprite(RegPBSpecialPlusDn,RegBSpecialPlus[0]+RegCurSpecial*RegBSpecialNextX+ChaX,RegBSpecialPlus[1]+RegCurSpecial*RegBSpecialNextY+ChaY); break;
		case IFACE_REG_SPEC_MN: SprMngr.DrawSprite(RegPBSpecialMinusDn,RegBSpecialMinus[0]+RegCurSpecial*RegBSpecialNextX+ChaX,RegBSpecialMinus[1]+RegCurSpecial*RegBSpecialNextY+ChaY); break;
		case IFACE_REG_TAGSKILL: SprMngr.DrawSprite(RegPBTagSkillDn,RegBTagSkill[0]+(RegCurTagSkill-SKILL_BEGIN)*RegBTagSkillNextX+ChaX,RegBTagSkill[1]+(RegCurTagSkill-SKILL_BEGIN)*RegBTagSkillNextY+ChaY); break;
		default: break;
		}
	}
}

void FOClient::ChaLMouseDown(bool is_reg)
{
	IfaceHold=IFACE_NONE;

	CritterCl* cr=(is_reg?RegNewCr:Chosen);
	if(!cr) return;

	// Special
	for(int i=ST_STRENGTH;i<=ST_LUCK;++i)
	{
		int offs=i-ST_STRENGTH;
		if(is_reg)
		{
			if(IsCurInRect(RegBSpecialPlus,RegBSpecialNextX*offs+ChaX,RegBSpecialNextY*offs+ChaY)) IfaceHold=IFACE_REG_SPEC_PL;
			if(IsCurInRect(RegBSpecialMinus,RegBSpecialNextX*offs+ChaX,RegBSpecialNextY*offs+ChaY)) IfaceHold=IFACE_REG_SPEC_MN;
			if(IfaceHold)
			{
				RegCurSpecial=i;
				goto label_DrawSpecial;
			}
		}

		if(IsCurInRect(ChaWSpecialValue,ChaX+ChaWSpecialNextX*offs,ChaY+ChaWSpecialNextY*offs) ||
			IsCurInRect(ChaWSpecialLevel,ChaX+ChaWSpecialNextX*offs,ChaY+ChaWSpecialNextY*offs) ||
			IsCurInRect(ChaWSpecialText,ChaX+ChaWSpecialNextX*offs,ChaY+ChaWSpecialNextY*offs))
		{
label_DrawSpecial:
			ChaSkilldexPic=SKILLDEX_PARAM(i);
			StringCopy(ChaName,MsgGame->GetStr(STR_PARAM_NAME_(i)));
			StringCopy(ChaDesc,MsgGame->GetStr(STR_PARAM_DESC_(i)));
			return;
		}
	}

	// Skills
	for(int i=SKILL_BEGIN;i<=SKILL_END;i++)
	{
		int offs=i-SKILL_BEGIN;
		if(is_reg && IsCurInRect(RegBTagSkill,offs*RegBTagSkillNextX+ChaX,offs*RegBTagSkillNextY+ChaY))
		{
			RegCurTagSkill=i;
			IfaceHold=IFACE_REG_TAGSKILL;
			goto label_DrawSkill;
		}

		if(IsCurInRect(ChaWSkillName,ChaX+ChaWSkillNextX*offs,ChaY+ChaWSkillNextY*offs) || IsCurInRect(ChaWSkillValue,ChaX+ChaWSkillNextX*offs,ChaY+ChaWSkillNextY*offs))
		{
label_DrawSkill:
			ChaSkilldexPic=SKILLDEX_PARAM(i);
			StringCopy(ChaName,MsgGame->GetStr(STR_PARAM_NAME_(i)));
			StringCopy(ChaDesc,MsgGame->GetStr(STR_PARAM_DESC_(i)));
			ChaCurSkill=offs;
			return;
		}
	}

	// Damage
	// Life
	if(IsCurInRect(ChaWDmgLife,ChaX,ChaY))
	{
		ChaSkilldexPic=SKILLDEX_PARAM(ST_MAX_LIFE);
		StringCopy(ChaName,MsgGame->GetStr(STR_PARAM_NAME_(ST_MAX_LIFE)));
		StringCopy(ChaDesc,MsgGame->GetStr(STR_PARAM_DESC_(ST_MAX_LIFE)));
		return;
	}

	// Body damages
	for(int i=DAMAGE_BEGIN;i<=DAMAGE_END;++i)
	{
		int offs=i-DAMAGE_BEGIN;
		if(IsCurInRect(ChaWDmg,ChaX+ChaWDmgNextX*offs,ChaY+ChaWDmgNextY*offs))
		{
			ChaSkilldexPic=SKILLDEX_PARAM(i);
			StringCopy(ChaName,MsgGame->GetStr(STR_PARAM_NAME_(i)));
			StringCopy(ChaDesc,MsgGame->GetStr(STR_PARAM_DESC_(i)));
			return;
		}
	}

	// Sec. Stats
	for(int i=0;i<ShowStatsCnt;++i)
	{
		if( IsCurInRect(ChaWStatsName,ChaX+ChaWStatsNextX*i,ChaY+ChaWStatsNextY*i) ||
			IsCurInRect(ChaWStatsValue,ChaX+ChaWStatsNextX*i,ChaY+ChaWStatsNextY*i))
		{
			ChaSkilldexPic=SKILLDEX_PARAM(ShowStats[i]);
			StringCopy(ChaName,MsgGame->GetStr(STR_PARAM_NAME_(ShowStats[i])));
			StringCopy(ChaDesc,MsgGame->GetStr(STR_PARAM_DESC_(ShowStats[i])));
			return;
		}
	}

	// Traits
	if(is_reg)
	{
		int i,k;
		// Left button
		for(i=TRAIT_BEGIN,k=0;i<TRAIT_BEGIN+TRAIT_COUNT/2;++i,++k)
			if(IsCurInRect(RegBTraitL,RegTraitNextX*k+ChaX,RegTraitNextY*k+ChaY)) { IfaceHold=IFACE_REG_TRAIT_L; goto label_DrawTrait; }
		// Right button
		for(i=TRAIT_BEGIN+TRAIT_COUNT/2,k=0;i<=TRAIT_END;++i,++k)
			if(IsCurInRect(RegBTraitR,RegTraitNextX*k+ChaX,RegTraitNextY*k+ChaY)) { IfaceHold=IFACE_REG_TRAIT_R; goto label_DrawTrait; }
		// Left text
		for(i=TRAIT_BEGIN,k=0;i<TRAIT_BEGIN+TRAIT_COUNT/2;++i,++k)
			if(IsCurInRect(RegWTraitL,RegTraitNextX*k+ChaX,RegTraitNextY*k+ChaY)) goto label_DrawTrait;
		// Right text
		for(i=TRAIT_BEGIN+TRAIT_COUNT/2,k=0;i<=TRAIT_END;++i,++k)
			if(IsCurInRect(RegWTraitR,RegTraitNextX*k+ChaX,RegTraitNextY*k+ChaY)) goto label_DrawTrait;

		if(0)
		{
label_DrawTrait:
			RegTraitNum=k;
			ChaSkilldexPic=SKILLDEX_PARAM(i);
			StringCopy(ChaName,MsgGame->GetStr(STR_PARAM_NAME_(i)));
			StringCopy(ChaDesc,MsgGame->GetStr(STR_PARAM_DESC_(i)));
			return;
		}
	}

	// Switch
	if(!is_reg)
	{
		// 3 Buttons
		if(IsCurInRect(ChaBSwitch,ChaX,ChaY) && SprMngr.IsPixNoTransp(ChaPBSwitchMask,CurX-ChaX-ChaBSwitch[0],CurY-ChaY-ChaBSwitch[1],false))
		{
			int sw=(CurX-ChaX-ChaBSwitch[0])/((ChaBSwitch[2]-ChaBSwitch[0])/3);
			switch(sw)
			{
			case 0:
				ChaCurSwitch=CHA_SWITCH_PERKS;
				StringCopy(ChaName,MsgGame->GetStr(STR_SWITCH_PERKS_NAME));
				StringCopy(ChaDesc,MsgGame->GetStr(STR_PERKS_DESC));
				ChaSkilldexPic=SKILLDEX_PERKS;
				break;
			case 1:
				ChaCurSwitch=CHA_SWITCH_KARMA;
				StringCopy(ChaName,MsgGame->GetStr(STR_SWITCH_KARMA_NAME));
				StringCopy(ChaDesc,MsgGame->GetStr(STR_KARMA_DESC));
				ChaSkilldexPic=SKILLDEX_KARMA;
				break;
			case 2:
				ChaCurSwitch=CHA_SWITCH_KILLS;
				StringCopy(ChaName,MsgGame->GetStr(STR_SWITCH_KILLS_NAME));
				StringCopy(ChaDesc,MsgGame->GetStr(STR_KILLS_DESC));
				ChaSkilldexPic=SKILLDEX_KILLS;
				break;
			default: break;
			}
			return;
		}

		if(IsCurInRect(ChaTSwitch,ChaX,ChaY))
		{
			SwitchElementVec& text=ChaSwitchText[ChaCurSwitch];
			int scroll=ChaSwitchScroll[ChaCurSwitch];
			int cur_line=scroll+(CurY-ChaTSwitch[1]-ChaY)/11;
			if(cur_line<text.size())
			{
				SwitchElement& e=text[cur_line];
				ChaSkilldexPic=e.PictureId;
				StringCopy(ChaName,MsgGame->GetStr(e.NameStrNum));
				if(ChaCurSwitch==CHA_SWITCH_PERKS) StringAppend(ChaName,e.Addon);
				StringCopy(ChaDesc,MsgGame->GetStr(e.DescStrNum));
				return;
			}
		}
	}

	// Holds
	if(!is_reg && IsCurInRect(ChaWLevel,ChaX,ChaY))
	{
		ChaSkilldexPic=SKILLDEX_PARAM(ST_LEVEL);
		StringCopy(ChaName,MsgGame->GetStr(STR_PARAM_NAME_(ST_LEVEL)));
		StringCopy(ChaDesc,MsgGame->GetStr(STR_PARAM_DESC_(ST_LEVEL)));
	}
	else if(!is_reg && IsCurInRect(ChaWExp,ChaX,ChaY))
	{
		ChaSkilldexPic=SKILLDEX_PARAM(ST_EXPERIENCE);
		StringCopy(ChaName,MsgGame->GetStr(STR_PARAM_NAME_(ST_EXPERIENCE)));
		StringCopy(ChaDesc,MsgGame->GetStr(STR_PARAM_DESC_(ST_EXPERIENCE)));
	}
	else if(!is_reg && IsCurInRect(ChaWNextLevel,ChaX,ChaY))
	{
		ChaSkilldexPic=SKILLDEX_NEXT_LEVEL;
		StringCopy(ChaName,MsgGame->GetStr(STR_NEXT_LEVEL_NAME));
		StringCopy(ChaDesc,MsgGame->GetStr(STR_NEXT_LEVEL_DESC));
	}
	else if(IsCurInRect(ChaBPrint,ChaX,ChaY)) IfaceHold=IFACE_CHA_PRINT;
	else if(IsCurInRect(ChaBOk,ChaX,ChaY)) IfaceHold=IFACE_CHA_OK;
	else if(IsCurInRect(ChaBCancel,ChaX,ChaY)) IfaceHold=IFACE_CHA_CANCEL;
	else if(!is_reg && IsCurInRect(ChaBSwitchScrUp,ChaX,ChaY))
	{
		Timer::StartAccelerator(ACCELERATE_CHA_SW_SCRUP);
		IfaceHold=IFACE_CHA_SW_SCRUP;
	}
	else if(!is_reg && IsCurInRect(ChaBSwitchScrDn,ChaX,ChaY))
	{
		Timer::StartAccelerator(ACCELERATE_CHA_SW_SCRDOWN);
		IfaceHold=IFACE_CHA_SW_SCRDN;
	}
	else if(!is_reg && IsCurInRect(ChaBSliderPlus,ChaCurSkill*ChaWSkillNextX+ChaX,ChaCurSkill*ChaWSkillNextY+ChaY))
	{
		Timer::StartAccelerator(ACCELERATE_CHA_PLUS);
		IfaceHold=IFACE_CHA_PLUS;
	}
	else if(!is_reg && IsCurInRect(ChaBSliderMinus,ChaCurSkill*ChaWSkillNextX+ChaX,ChaCurSkill*ChaWSkillNextY+ChaY))
	{
		Timer::StartAccelerator(ACCELERATE_CHA_MINUS);
		IfaceHold=IFACE_CHA_MINUS;
	}
	else if(is_reg && IsCurInRect(ChaBName,ChaX,ChaY)) IfaceHold=IFACE_CHA_NAME;
	else if(is_reg && IsCurInRect(ChaBAge,ChaX,ChaY)) IfaceHold=IFACE_CHA_AGE;
	else if(is_reg && IsCurInRect(ChaBSex,ChaX,ChaY)) IfaceHold=IFACE_CHA_SEX;
	else if(!is_reg && IsCurInRect(ChaWMain,ChaX,ChaY))
	{
		ChaVectX=CurX-ChaX;
		ChaVectY=CurY-ChaY;
		IfaceHold=IFACE_CHA_MAIN;
	}
}

void FOClient::ChaLMouseUp(bool is_reg)
{
	CritterCl* cr=(is_reg?RegNewCr:Chosen);
	if(!cr) return;

	switch(IfaceHold)
	{
	case IFACE_CHA_PRINT:
		if(!IsCurInRect(ChaBPrint,ChaX,ChaY)) break;
		break;
	case IFACE_CHA_OK:
		if(!IsCurInRect(ChaBOk,ChaX,ChaY)) break;
		if(is_reg)
		{
			if(RegCheckData(RegNewCr))
			{
				NetState=STATE_INIT_NET;
				InitNetReason=INIT_NET_REASON_REG;
				SetCurMode(CUR_WAIT);
			}
		}
		else
		{
			if(ChaUnspentSkillPoints<cr->Params[ST_UNSPENT_SKILL_POINTS]) Net_SendLevelUp(0xFFFF);
			ShowScreen(SCREEN_NONE);
		}
		break;
	case IFACE_CHA_CANCEL:
		{
			if(!IsCurInRect(ChaBCancel,ChaX,ChaY)) break;
			if(is_reg) ShowMainScreen(SCREEN_LOGIN);
			else ShowScreen(SCREEN_NONE);
		}
		break;
	case IFACE_CHA_SW_SCRUP:
		{
			if(is_reg) break;
			if(!IsCurInRect(ChaBSwitchScrUp,ChaX,ChaY)) break;
			if(ChaSwitchScroll[ChaCurSwitch]>0) ChaSwitchScroll[ChaCurSwitch]--;
		}
		break;
	case IFACE_CHA_SW_SCRDN:
		{
			if(is_reg) break;
			if(!IsCurInRect(ChaBSwitchScrDn,ChaX,ChaY)) break;
			int max_lines=(ChaTSwitch[3]-ChaTSwitch[1])/11;
			SwitchElementVec& text=ChaSwitchText[ChaCurSwitch];
			int& scroll=ChaSwitchScroll[ChaCurSwitch];
			if(scroll+max_lines<text.size()) scroll++;
		}
		break;
	case IFACE_CHA_PLUS:
		{
			if(is_reg) break;
			if(!IsCurInRect(ChaBSliderPlus,ChaCurSkill*ChaWSkillNextX+ChaX,ChaCurSkill*ChaWSkillNextY+ChaY)) break;
			if(!ChaUnspentSkillPoints) break;

			int skill_val=cr->GetSkill(ChaCurSkill+SKILL_BEGIN)+ChaSkillUp[ChaCurSkill];
			if(skill_val>=MAX_SKILL_VAL) break;
			int need_sp;

			if(skill_val<=100) need_sp=1;
			else if(skill_val>100 && skill_val<=125) need_sp=2;
			else if(skill_val>125 && skill_val<=150) need_sp=3;
			else if(skill_val>150 && skill_val<=175) need_sp=4;
			else if(skill_val>175 && skill_val<=200) need_sp=5;
			else need_sp=6;

			if(ChaUnspentSkillPoints<need_sp) break;

			ChaUnspentSkillPoints-=need_sp;
			ChaSkillUp[ChaCurSkill]++;
			if(cr->IsTagSkill(ChaCurSkill+SKILL_BEGIN)) ChaSkillUp[ChaCurSkill]++;
		}
		break;
	case IFACE_CHA_MINUS:
		{
			if(is_reg) break;
			if(!IsCurInRect(ChaBSliderMinus,ChaCurSkill*ChaWSkillNextX+ChaX,ChaCurSkill*ChaWSkillNextY+ChaY)) break;
			if(!ChaSkillUp[ChaCurSkill]) break;

			ChaSkillUp[ChaCurSkill]--;
			if(cr->IsTagSkill(ChaCurSkill+SKILL_BEGIN)) ChaSkillUp[ChaCurSkill]--;
			int skill_val=cr->GetSkill(ChaCurSkill+SKILL_BEGIN)+ChaSkillUp[ChaCurSkill];

			if(skill_val<=100) ChaUnspentSkillPoints+=1;
			else if(skill_val>100 && skill_val<=125) ChaUnspentSkillPoints+=2;
			else if(skill_val>125 && skill_val<=150) ChaUnspentSkillPoints+=3;
			else if(skill_val>150 && skill_val<=175) ChaUnspentSkillPoints+=4;
			else if(skill_val>175 && skill_val<=200) ChaUnspentSkillPoints+=5;
			else ChaUnspentSkillPoints+=6;
		}
		break;
	case IFACE_REG_SPEC_PL:
		{
			if(!is_reg) break;
			if(!IsCurInRect(RegBSpecialPlus,RegCurSpecial*RegBSpecialNextX+ChaX,RegCurSpecial*RegBSpecialNextY+ChaY)) break;
			int cur_count=cr->Params[RegCurSpecial];
			int unspent=40;
			for(int i=ST_STRENGTH;i<=ST_LUCK;i++) unspent-=cr->ParamsReg[i];
			if(unspent<=0 || cur_count>=10) break;
			cr->ParamsReg[RegCurSpecial]++;
			cr->GenParams();
		}
		break;
	case IFACE_REG_SPEC_MN:
		{
			if(!is_reg) break;
			if(!IsCurInRect(RegBSpecialMinus,RegCurSpecial*RegBSpecialNextX+ChaX,RegCurSpecial*RegBSpecialNextY+ChaY)) break;
			int cur_count=cr->Params[RegCurSpecial];
			if(cur_count<=1) break;
			cr->ParamsReg[RegCurSpecial]--;
			cr->GenParams();
		}
		break;
	case IFACE_REG_TAGSKILL:
		{
			if(!is_reg) break;
			int offs=RegCurTagSkill-SKILL_BEGIN;
			if(!IsCurInRect(RegBTagSkill,offs*RegBTagSkillNextX+ChaX,offs*RegBTagSkillNextY+ChaY)) break;

			if(cr->ParamsReg[TAG_SKILL1]==RegCurTagSkill) cr->ParamsReg[TAG_SKILL1]=0;
			else if(cr->ParamsReg[TAG_SKILL2]==RegCurTagSkill) cr->ParamsReg[TAG_SKILL2]=0;
			else if(cr->ParamsReg[TAG_SKILL3]==RegCurTagSkill) cr->ParamsReg[TAG_SKILL3]=0;
			else if(!cr->ParamsReg[TAG_SKILL1]) cr->ParamsReg[TAG_SKILL1]=RegCurTagSkill;
			else if(!cr->ParamsReg[TAG_SKILL2]) cr->ParamsReg[TAG_SKILL2]=RegCurTagSkill;
			else if(!cr->ParamsReg[TAG_SKILL3]) cr->ParamsReg[TAG_SKILL3]=RegCurTagSkill;

			cr->GenParams();
		}
		break;
	case IFACE_REG_TRAIT_L:
	case IFACE_REG_TRAIT_R:
		{
			if(!is_reg) break;
			if(IfaceHold==IFACE_REG_TRAIT_L && !IsCurInRect(RegBTraitL,RegTraitNum*RegTraitNextX+ChaX,RegTraitNum*RegTraitNextY+ChaY)) break;
			if(IfaceHold==IFACE_REG_TRAIT_R && !IsCurInRect(RegBTraitR,RegTraitNum*RegTraitNextX+ChaX,RegTraitNum*RegTraitNextY+ChaY)) break;

			int trait=RegTraitNum+TRAIT_BEGIN;
			if(IfaceHold==IFACE_REG_TRAIT_R) trait+=TRAIT_COUNT/2;

			if(cr->Params[trait])
			{
				cr->ParamsReg[trait]=0;
			}
			else
			{
				int j=0;
				for(int i=TRAIT_BEGIN;i<=TRAIT_END;++i) if(cr->Params[i]) j++;
				if(j<2) cr->ParamsReg[trait]=1;
			}

			cr->GenParams();
		}
		break;
	case IFACE_CHA_NAME:
		if(!is_reg || !IsCurInRect(ChaBName,ChaX,ChaY)) break;
		ShowScreen(SCREEN__CHA_NAME);
		return;
	case IFACE_CHA_AGE:
		if(!is_reg || !IsCurInRect(ChaBAge,ChaX,ChaY)) break;
		ShowScreen(SCREEN__CHA_AGE);
		return;
	case IFACE_CHA_SEX:
		if(!is_reg || !IsCurInRect(ChaBSex,ChaX,ChaY)) break;
		ShowScreen(SCREEN__CHA_SEX);
		return;
	default:
		break;
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::ChaMouseMove(bool is_reg)
{
	if(IfaceHold!=IFACE_CHA_MAIN) return;

	ChaX=CurX-ChaVectX;
	ChaY=CurY-ChaVectY;

	if(ChaX<0) ChaX=0;
	if(ChaX+ChaWMain[2]>MODE_WIDTH) ChaX=MODE_WIDTH-ChaWMain[2];
	if(ChaY<0) ChaY=0;
	if(ChaY+ChaWMain[3]>MODE_HEIGHT) ChaY=MODE_HEIGHT-ChaWMain[3];
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::ChaNameDraw()
{
	bool is_reg=IsMainScreen(SCREEN_REGISTRATION);
	CritterCl* cr=(is_reg?RegNewCr:Chosen);
	if(!cr)
	{
		ShowScreen(SCREEN_NONE);
		return;
	}

	SprMngr.DrawSprite(Singleplayer?ChaNameSingleplayerMainPic:ChaNameMainPic,ChaNameX,ChaNameY);

	SprMngr.DrawStr(INTRECT(ChaNameWNameText,ChaNameX,ChaNameY),MsgGame->GetStr(STR_CHA_NAME_NAME),FT_NOBREAK|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(ChaNameWName,ChaNameX,ChaNameY),cr->GetName(),FT_NOBREAK|FT_CENTERY,IfaceHold==IFACE_CHA_NAME_NAME?COLOR_TEXT_LGREEN:COLOR_TEXT_DGREEN);

	if(!Singleplayer)
	{
		SprMngr.DrawStr(INTRECT(ChaNameWPassText,ChaNameX,ChaNameY),MsgGame->GetStr(STR_CHA_NAME_PASS),FT_NOBREAK|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
		SprMngr.DrawStr(INTRECT(ChaNameWPass,ChaNameX,ChaNameY),cr->GetPass(),FT_NOBREAK|FT_CENTERY,IfaceHold==IFACE_CHA_NAME_PASS?COLOR_TEXT_LGREEN:COLOR_TEXT_DGREEN);
	}
}

void FOClient::ChaNameLMouseDown()
{
	IfaceHold=IFACE_NONE;

	if(!IsCurInRectNoTransp(Singleplayer?ChaNameSingleplayerMainPic:ChaNameMainPic,ChaNameWMain,ChaX,ChaY))
	{
		ShowScreen(SCREEN_NONE);
		return;
	}

	if(!IsMainScreen(SCREEN_REGISTRATION)) return;

	if(IsCurInRect(ChaNameWName,ChaNameX,ChaNameY)) IfaceHold=IFACE_CHA_NAME_NAME;
	else if(!Singleplayer && IsCurInRect(ChaNameWPass,ChaNameX,ChaNameY)) IfaceHold=IFACE_CHA_NAME_PASS;
}

void FOClient::ChaNameKeyDown(BYTE dik)
{
	if(!IsMainScreen(SCREEN_REGISTRATION) || !RegNewCr) return;

	if(dik==DIK_TAB && !Singleplayer)
	{
		switch(IfaceHold)
		{
		case IFACE_CHA_NAME_NAME: IfaceHold=IFACE_CHA_NAME_PASS; return;
		case IFACE_CHA_NAME_PASS: IfaceHold=IFACE_CHA_NAME_NAME; return;
		default: IfaceHold=IFACE_CHA_NAME_NAME; return;
		}
	}

	if(dik==DIK_RETURN || dik==DIK_NUMPADENTER)
	{
		ShowScreen(SCREEN_NONE);
		return;
	}

	switch(IfaceHold)
	{
	case IFACE_CHA_NAME_NAME: Keyb::GetChar(dik,RegNewCr->Name,NULL,min(GameOpt.MaxNameLength,MAX_NAME),KIF_NO_SPEC_SYMBOLS); break;
	case IFACE_CHA_NAME_PASS: Keyb::GetChar(dik,RegNewCr->Pass,NULL,min(GameOpt.MaxNameLength,MAX_NAME),KIF_NO_SPEC_SYMBOLS); break;
	default: break;
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::ChaAgeDraw()
{
	SprMngr.DrawSprite(ChaAgePic,ChaAgeX,ChaAgeY);

	switch(IfaceHold)
	{
	case IFACE_CHA_AGE_UP: SprMngr.DrawSprite(ChaAgeBUpDn,ChaAgeBUp[0]+ChaAgeX,ChaAgeBUp[1]+ChaAgeY); break;
	case IFACE_CHA_AGE_DOWN: SprMngr.DrawSprite(ChaAgeBDownDn,ChaAgeBDown[0]+ChaAgeX,ChaAgeBDown[1]+ChaAgeY); break;
	default: break;
	}

	if(!IsMainScreen(SCREEN_REGISTRATION) || !RegNewCr) return;
	char str[16];
	sprintf(str,"%02d",RegNewCr->ParamsReg[ST_AGE]);
	SprMngr.DrawStr(INTRECT(ChaAgeWAge,ChaAgeX,ChaAgeY),str,FT_NOBREAK,COLOR_IFACE,FONT_BIG_NUM);
}

void FOClient::ChaAgeLMouseDown()
{
	IfaceHold=IFACE_NONE;
	
	if(!IsCurInRect(ChaAgeWMain,ChaX,ChaY))
	{
		ShowScreen(SCREEN_NONE);
		return;
	}

	if(IsCurInRect(ChaAgeBUp,ChaAgeX,ChaAgeY))
	{
		Timer::StartAccelerator(ACCELERATE_CHA_AGE_UP);
		IfaceHold=IFACE_CHA_AGE_UP;
	}
	else if(IsCurInRect(ChaAgeBDown,ChaAgeX,ChaAgeY))
	{
		Timer::StartAccelerator(ACCELERATE_CHA_AGE_DOWN);
		IfaceHold=IFACE_CHA_AGE_DOWN;
	}
}

void FOClient::ChaAgeLMouseUp()
{
	switch(IfaceHold)
	{
	case IFACE_CHA_AGE_UP:
		if(!IsCurInRect(ChaAgeBUp,ChaAgeX,ChaAgeY)) break;
		if(!RegNewCr) break;
		if(RegNewCr->ParamsReg[ST_AGE]>=AGE_MAX) RegNewCr->ParamsReg[ST_AGE]=AGE_MIN;
		else RegNewCr->ParamsReg[ST_AGE]++;
		RegNewCr->GenParams();
	case IFACE_CHA_AGE_DOWN:
		if(!IsCurInRect(ChaAgeBDown,ChaAgeX,ChaAgeY)) break;
		if(!RegNewCr) break;
		if(RegNewCr->ParamsReg[ST_AGE]<=AGE_MIN) RegNewCr->ParamsReg[ST_AGE]=AGE_MAX;
		else RegNewCr->ParamsReg[ST_AGE]--;
		RegNewCr->GenParams();
	default:
		break;
	}

	IfaceHold=IFACE_NONE;
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::ChaSexDraw()
{
	SprMngr.DrawSprite(ChaSexPic,ChaSexX,ChaSexY);

	if(!RegNewCr) return;

	if(RegNewCr->ParamsReg[ST_GENDER]==GENDER_MALE || IfaceHold==IFACE_CHA_SEX_MALE)
		SprMngr.DrawSprite(ChaSexBMaleDn,ChaSexBMale[0]+ChaSexX,ChaSexBMale[1]+ChaSexY);
	if(RegNewCr->ParamsReg[ST_GENDER]==GENDER_FEMALE  || IfaceHold==IFACE_CHA_SEX_FEMALE)
		SprMngr.DrawSprite(ChaSexBFemaleDn,ChaSexBFemale[0]+ChaSexX,ChaSexBFemale[1]+ChaSexY);
}

void FOClient::ChaSexLMouseDown()
{
	IfaceHold=IFACE_NONE;
	
	if(!IsCurInRect(ChaSexWMain,ChaX,ChaY))
	{
		ShowScreen(SCREEN_NONE);
		return;
	}

	if(IsCurInRect(ChaSexBMale,ChaSexX,ChaSexY)) IfaceHold=IFACE_CHA_SEX_MALE;
	if(IsCurInRect(ChaSexBFemale,ChaSexX,ChaSexY)) IfaceHold=IFACE_CHA_SEX_FEMALE;
}

void FOClient::ChaSexLMouseUp()
{
	switch(IfaceHold)
	{
	case IFACE_CHA_SEX_MALE:
		if(!IsCurInRect(ChaSexBMale,ChaSexX,ChaSexY)) break;
		if(RegNewCr)
		{
			RegNewCr->ParamsReg[ST_GENDER]=GENDER_MALE;
			RegNewCr->GenParams();
		}
	case IFACE_CHA_SEX_FEMALE:
		if(!IsCurInRect(ChaSexBFemale,ChaSexX,ChaSexY)) break;
		if(RegNewCr)
		{
			RegNewCr->ParamsReg[ST_GENDER]=GENDER_FEMALE;
			RegNewCr->GenParams();
		}
	default: break;
	}

	IfaceHold=IFACE_NONE;
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::PerkPrepare()
{
	PerkCollection.clear();
	if(!Chosen) return;

	for(int i=PERK_BEGIN;i<=PERK_END;i++)
	{
		if(Script::PrepareContext(ClientFunctions.PerkCheck,CALL_FUNC_STR,"Perk"))
		{
			Script::SetArgObject(Chosen);
			Script::SetArgDword(i-(GameOpt.AbsoluteOffsets?0:PERK_BEGIN));
			if(Script::RunPrepared() && Script::GetReturnedBool()) PerkCollection.push_back(i);
		}
	}
}

void FOClient::PerkDraw()
{
	SprMngr.DrawSprite(PerkPMain,PerkWMain[0]+PerkX,PerkWMain[1]+PerkY);

	switch(IfaceHold)
	{
	case IFACE_PERK_SCRUP: SprMngr.DrawSprite(PerkPBScrUpDn,PerkBScrUp[0]+PerkX,PerkBScrUp[1]+PerkY); break;
	case IFACE_PERK_SCRDN: SprMngr.DrawSprite(PerkPBScrDnDn,PerkBScrDn[0]+PerkX,PerkBScrDn[1]+PerkY); break;
	case IFACE_PERK_OK: SprMngr.DrawSprite(PerkPBOkDn,PerkBOk[0]+PerkX,PerkBOk[1]+PerkY); break;
	case IFACE_PERK_CANCEL: SprMngr.DrawSprite(PerkPBCancelDn,PerkBCancel[0]+PerkX,PerkBCancel[1]+PerkY); break;
	default: break;
	}

	if(PerkCurPerk>=0)
	{
		const char* name=FONames::GetPictureName(SKILLDEX_PARAM(PerkCurPerk));
		if(name)
		{
			DWORD spr_id=ResMngr.GetSkDxSprId(Str::GetHash(name));
			if(spr_id) SprMngr.DrawSprite(spr_id,PerkWPic[0]+PerkX,PerkWPic[1]+PerkY);
		}
	}

	SprMngr.DrawStr(INTRECT(PerkBOkText,PerkX,PerkY),MsgGame->GetStr(STR_PERK_TAKE),FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(PerkBCancelText,PerkX,PerkY),MsgGame->GetStr(STR_PERK_CANCEL),FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);

	for(int i=PerkScroll,j=(int)PerkCollection.size(),k=0;i<j;i++,k++)
	{
		if(PerkNextX && PerkNextX*(k+1)>=PerkWPerks[2]-PerkWPerks[0]) break;
		if(PerkNextY && PerkNextY*(k+1)>=PerkWPerks[3]-PerkWPerks[1]) break;

		DWORD col=COLOR_TEXT;
		if(PerkCollection[i]==PerkCurPerk) col=COLOR_TEXT_DGREEN;
		SprMngr.DrawStr(INTRECT(PerkWPerks,PerkX+PerkNextX*k,PerkY+PerkNextY*k),MsgGame->GetStr(STR_PARAM_NAME_(PerkCollection[i])),0,col);
	}

	if(PerkCurPerk>=0) SprMngr.DrawStr(INTRECT(PerkWText,PerkX,PerkY),MsgGame->GetStr(STR_PARAM_DESC_(PerkCurPerk)),0,COLOR_TEXT_BLACK);
}

void FOClient::PerkLMouseDown()
{
	IfaceHold=IFACE_NONE;
	if(!IsCurInRect(PerkWMain,PerkX,PerkY)) return;

	if(IsCurInRect(PerkWPerks,PerkX,PerkY))
	{
		IfaceHold=IFACE_PERK_PERKS;
		int cur_perk=-1;

		if(PerkNextX) cur_perk=PerkScroll+(CurX-PerkWPerks[0]-PerkX)/PerkNextX;
		if(PerkNextY) cur_perk=PerkScroll+(CurY-PerkWPerks[1]-PerkY)/PerkNextY;

		if(cur_perk>=0 && cur_perk<PerkCollection.size()) PerkCurPerk=PerkCollection[cur_perk];
		else PerkCurPerk=-1;
	}
	else if(IsCurInRect(PerkBScrUp,PerkX,PerkY))
	{
		Timer::StartAccelerator(ACCELERATE_PERK_SCRUP);
		IfaceHold=IFACE_PERK_SCRUP;
	}
	else if(IsCurInRect(PerkBScrDn,PerkX,PerkY))
	{
		Timer::StartAccelerator(ACCELERATE_PERK_SCRDOWN);
		IfaceHold=IFACE_PERK_SCRDN;
	}
	else if(IsCurInRect(PerkBOk,PerkX,PerkY)) IfaceHold=IFACE_PERK_OK;
	else if(IsCurInRect(PerkBCancel,PerkX,PerkY)) IfaceHold=IFACE_PERK_CANCEL;
	else
	{
		PerkVectX=CurX-PerkX;
		PerkVectY=CurY-PerkY;
		IfaceHold=IFACE_PERK_MAIN;
	}
}

void FOClient::PerkLMouseUp()
{
	switch(IfaceHold)
	{
	case IFACE_PERK_SCRUP:
		if(!IsCurInRect(PerkBScrUp,PerkX,PerkY)) break;
		if(PerkScroll>0) PerkScroll--;
		break;
	case IFACE_PERK_SCRDN:
		if(!IsCurInRect(PerkBScrDn,PerkX,PerkY)) break;
		if(PerkScroll<(int)PerkCollection.size()-1) PerkScroll++;
		break;
	case IFACE_PERK_OK:
		if(!IsCurInRect(PerkBOk,PerkX,PerkY)) break;
		ShowScreen(SCREEN_NONE);
		if(PerkCurPerk>=0) Net_SendLevelUp(PerkCurPerk);
		break;
	case IFACE_PERK_CANCEL:
		if(!IsCurInRect(PerkBCancel,PerkX,PerkY)) break;
		ShowScreen(SCREEN_NONE);
		break;
	default:
		break;
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::PerkMouseMove()
{
	if(IfaceHold==IFACE_PERK_MAIN)
	{
		PerkX=CurX-PerkVectX;
		PerkY=CurY-PerkVectY;

		if(PerkX<0) PerkX=0;
		if(PerkX+PerkWMain[2]>MODE_WIDTH) PerkX=MODE_WIDTH-PerkWMain[2];
		if(PerkY<0) PerkY=0;
		if(PerkY+PerkWMain[3]>MODE_HEIGHT) PerkY=MODE_HEIGHT-PerkWMain[3];
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::TViewDraw()
{
	SprMngr.DrawSprite(TViewWMainPic,TViewWMain[0]+TViewX,TViewWMain[1]+TViewY);

	if(IfaceHold==IFACE_TOWN_VIEW_BACK) SprMngr.DrawSprite(TViewBBackPicDn,TViewBBack[0]+TViewX,TViewBBack[1]+TViewY);
	if(IfaceHold==IFACE_TOWN_VIEW_ENTER) SprMngr.DrawSprite(TViewBEnterPicDn,TViewBEnter[0]+TViewX,TViewBEnter[1]+TViewY);
	if(IfaceHold==IFACE_TOWN_VIEW_CONTOUR || TViewShowCountours) SprMngr.DrawSprite(TViewBContoursPicDn,TViewBContours[0]+TViewX,TViewBContours[1]+TViewY);

	SprMngr.DrawStr(INTRECT(TViewBBack,TViewX,IfaceHold==IFACE_TOWN_VIEW_BACK?TViewY-1:TViewY),MsgGame->GetStr(STR_TOWN_VIEW_BACK),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(TViewBEnter,TViewX,IfaceHold==IFACE_TOWN_VIEW_ENTER?TViewY-1:TViewY),MsgGame->GetStr(STR_TOWN_VIEW_ENTER),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(TViewBContours,TViewX,IfaceHold==IFACE_TOWN_VIEW_CONTOUR || TViewShowCountours?TViewY-1:TViewY),MsgGame->GetStr(STR_TOWN_VIEW_CONTOURS),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
}

void FOClient::TViewLMouseDown()
{
	IfaceHold=IFACE_NONE;

	if(IsCurInRect(TViewBBack,TViewX,TViewY)) IfaceHold=IFACE_TOWN_VIEW_BACK;
	else if(IsCurInRect(TViewBEnter,TViewX,TViewY)) IfaceHold=IFACE_TOWN_VIEW_ENTER;
	else if(IsCurInRect(TViewBContours,TViewX,TViewY)) IfaceHold=IFACE_TOWN_VIEW_CONTOUR;
	else if(IsCurInRect(TViewWMain,TViewX,TViewY))
	{
		TViewVectX=CurX-TViewX;
		TViewVectY=CurY-TViewY;
		IfaceHold=IFACE_TOWN_VIEW_MAIN;
	}
}

void FOClient::TViewLMouseUp()
{
	if(IfaceHold==IFACE_TOWN_VIEW_BACK && IsCurInRect(TViewBBack,TViewX,TViewY))
	{
		// Back
		Net_SendRefereshMe();
	}
	else if(IfaceHold==IFACE_TOWN_VIEW_ENTER && IsCurInRect(TViewBEnter,TViewX,TViewY))
	{
		// Enter to map
		if(TViewType==TOWN_VIEW_FROM_GLOBAL) Net_SendRuleGlobal(GM_CMD_TOLOCAL,TViewGmapLocId,TViewGmapLocEntrance);
	}
	else if(IfaceHold==IFACE_TOWN_VIEW_CONTOUR && IsCurInRect(TViewBContours,TViewX,TViewY))
	{
		if(!TViewShowCountours)
		{
			// Show contours
			HexMngr.SetCrittersContour(Sprite::ContourCustom);
		}
		else
		{
			// Hide contours
			HexMngr.SetCrittersContour(Sprite::ContourNone);
		}
		TViewShowCountours=!TViewShowCountours;
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::TViewMouseMove()
{
	if(IfaceHold==IFACE_TOWN_VIEW_MAIN)
	{
		TViewX=CurX-TViewVectX;
		TViewY=CurY-TViewVectY;

		if(TViewX<0) TViewX=0;
		if(TViewX+TViewWMain[2]>MODE_WIDTH) TViewX=MODE_WIDTH-TViewWMain[2];
		if(TViewY<0) TViewY=0;
		if(TViewY+TViewWMain[3]>MODE_HEIGHT) TViewY=MODE_HEIGHT-TViewWMain[3];
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::PipDraw()
{
	SprMngr.DrawSprite(PipPMain,PipWMain[0]+PipX,PipWMain[1]+PipY);
	if(!Chosen) return;

	switch(IfaceHold)
	{
	case IFACE_PIP_STATUS: SprMngr.DrawSprite(PipPBStatusDn,PipBStatus[0]+PipX,PipBStatus[1]+PipY); break;
	//case IFACE_PIP_GAMES: SprMngr.DrawSprite(PipPBGamesDn,PipBGames[0]+PipX,PipBGames[1]+PipY); break;
	case IFACE_PIP_AUTOMAPS: SprMngr.DrawSprite(PipPBAutomapsDn,PipBAutomaps[0]+PipX,PipBAutomaps[1]+PipY); break;
	case IFACE_PIP_ARCHIVES: SprMngr.DrawSprite(PipPBArchivesDn,PipBArchives[0]+PipX,PipBArchives[1]+PipY); break;
	case IFACE_PIP_CLOSE:	SprMngr.DrawSprite(PipPBCloseDn,PipBClose[0]+PipX,PipBClose[1]+PipY); break;
	default: break;
	}

	int scr=-(int)PipScroll[PipMode];
	INTRECT& r=PipWMonitor;
	int ml=SprMngr.GetLinesCount(0,r.H(),NULL,FONT_DEFAULT);
	int h=r.H()/ml;
#define PIP_DRAW_TEXT(text,flags,color) do{if(scr>=0 && scr<ml) SprMngr.DrawStr(INTRECT(r[0],r[1]+scr*h,r[2],r[1]+scr*h+h,PipX,PipY),text,flags,color);}while(0)
#define PIP_DRAW_TEXTR(text,flags,color) do{if(scr>=0 && scr<ml) SprMngr.DrawStr(INTRECT(r[2]-r.W()/3,r[1]+scr*h,r[2],r[1]+scr*h+h,PipX,PipY),text,flags,color);}while(0)

	switch(PipMode)
	{
	case PIP__NONE:
		{
			SpriteInfo* si=SprMngr.GetSpriteInfo(PipPWMonitor);
			SprMngr.DrawSprite(PipPWMonitor,PipWMonitor[0]+(PipWMonitor[2]-PipWMonitor[0]-si->Width)/2+PipX,PipWMonitor[1]+(PipWMonitor[3]-PipWMonitor[1]-si->Height)/2+PipY);
		}
		break;
	case PIP__STATUS:
		{
			// Status
			PIP_DRAW_TEXT(FmtGameText(STR_PIP_STATUS),FT_CENTERX,COLOR_TEXT_DGREEN); scr++;
			PIP_DRAW_TEXT(FmtGameText(STR_PIP_REPLICATION_MONEY),0,COLOR_TEXT);
			PIP_DRAW_TEXTR(FmtGameText(STR_PIP_REPLICATION_MONEY_VAL,Chosen->GetParam(ST_REPLICATION_MONEY)),0,COLOR_TEXT); scr++;
			PIP_DRAW_TEXT(FmtGameText(STR_PIP_REPLICATION_COST),0,COLOR_TEXT);
			PIP_DRAW_TEXTR(FmtGameText(STR_PIP_REPLICATION_COST_VAL,Chosen->GetParam(ST_REPLICATION_COST)),0,COLOR_TEXT); scr++;
			PIP_DRAW_TEXT(FmtGameText(STR_PIP_REPLICATION_COUNT),0,COLOR_TEXT);
			PIP_DRAW_TEXTR(FmtGameText(STR_PIP_REPLICATION_COUNT_VAL,Chosen->GetParam(ST_REPLICATION_COUNT)),0,COLOR_TEXT); scr++;

			// Timeouts
			scr++;
			PIP_DRAW_TEXT(FmtGameText(STR_PIP_TIMEOUTS),FT_CENTERX,COLOR_TEXT_DGREEN); scr++;
			for(int j=TIMEOUT_END;j>=TIMEOUT_BEGIN;j--)
			{
				if(!MsgGame->Count(STR_PARAM_NAME_(j))) continue;
				DWORD val=Chosen->GetTimeout(j);

				if(j==TO_REMOVE_FROM_GAME)
				{
					DWORD to_battle=Chosen->GetTimeout(TO_BATTLE);
					if(val<to_battle) val=to_battle;
				}

				val/=(GameOpt.TimeMultiplier?GameOpt.TimeMultiplier:1); // Convert to seconds
				if(j==TO_REMOVE_FROM_GAME && val<CLIENT_KICK_TIME/1000) val=CLIENT_KICK_TIME/1000;
				if(j==TO_REMOVE_FROM_GAME && NoLogOut) val=1000000;

				DWORD str_num=STR_TIMEOUT_SECONDS;
				if(val>300)
				{
					val/=60;
					str_num=STR_TIMEOUT_MINUTES;
				}
				PIP_DRAW_TEXT(FmtGameText(STR_PARAM_NAME_(j)),0,COLOR_TEXT);
				if(val>1440) PIP_DRAW_TEXTR(FmtGameText(str_num,"?"),0,COLOR_TEXT);
				else PIP_DRAW_TEXTR(FmtGameText(str_num,Str::Format("%u",val)),0,COLOR_TEXT);
				scr++;
			}

			// Quests
			scr++;
			if(scr>=0 && scr<ml) SprMngr.DrawStr(INTRECT(PipWMonitor[0],PipWMonitor[1]+scr*h,PipWMonitor[2],PipWMonitor[1]+scr*h+h,PipX,PipY),FmtGameText(STR_PIP_QUESTS),FT_CENTERX,COLOR_TEXT_DGREEN); scr++;
			QuestTabMap* tabs=QuestMngr.GetTabs();
			for(QuestTabMapIt it=tabs->begin(),end=tabs->end();it!=end;++it)
			{
				PIP_DRAW_TEXT((*it).first.c_str(),FT_NOBREAK|FT_COLORIZE,COLOR_TEXT);
				scr++;
			}

			// Scores title
			scr++;
			PIP_DRAW_TEXT(FmtGameText(STR_PIP_SCORES),FT_CENTERX,COLOR_TEXT_DGREEN);
		}
		break;
	case PIP__STATUS_QUESTS:
		{
			QuestTab* tab=QuestMngr.GetTab(QuestNumTab);
			if(!tab) break;
			SprMngr.DrawStr(INTRECT(PipWMonitor,PipX,PipY),tab->GetText(),FT_COLORIZE|FT_SKIPLINES(PipScroll[PipMode]));
		}
		break;
	case PIP__STATUS_SCORES:
		{
			bool is_first_title=true;
			char last_title[256];
			for(int i=0;i<SCORES_MAX;i++)
			{				
				if(MsgGame->Count(STR_SCORES_TITLE_(i))) StringCopy(last_title,MsgGame->GetStr(STR_SCORES_TITLE_(i)));
				char* name=&BestScores[i][0];
				if(!name[0]) continue;
				if(last_title[0])
				{
					if(!is_first_title) scr++;
					PIP_DRAW_TEXT(last_title,FT_CENTERX,COLOR_TEXT_DGREEN); scr+=2;
					last_title[0]='\0';
					is_first_title=false;
				}
				PIP_DRAW_TEXT(MsgGame->GetStr(STR_SCORES_NAME_(i)),FT_CENTERX|FT_COLORIZE,COLOR_TEXT); scr++;
				PIP_DRAW_TEXT(name,FT_CENTERX,COLOR_TEXT); scr++;
			}
		}
		break;
//	case PIP__GAMES:
//		break;
	case PIP__AUTOMAPS:
		{
			PIP_DRAW_TEXT(FmtGameText(STR_PIP_MAPS),FT_CENTERX,COLOR_TEXT_DGREEN);
			scr+=2;
			for(size_t i=0,j=Automaps.size();i<j;i++)
			{
				Automap& amap=Automaps[i];
				PIP_DRAW_TEXT(amap.LocName.c_str(),FT_CENTERX,COLOR_TEXT); scr++;

				for(size_t k=0,l=amap.MapNames.size();k<l;k++)
				{
					PIP_DRAW_TEXT(amap.MapNames[k].c_str(),FT_CENTERX,COLOR_TEXT_GREEN); scr++;
				}
				scr++;
			}
		}
		break;
	case PIP__AUTOMAPS_LOC:
		{
			SprMngr.DrawStr(INTRECT(PipWMonitor,PipX,PipY),MsgGM->GetStr(STR_GM_INFO_(AutomapSelected.LocPid)),FT_COLORIZE|FT_SKIPLINES(PipScroll[PipMode])|FT_ALIGN);
		}
		break;
	case PIP__AUTOMAPS_MAP:
		{
			WORD map_pid=AutomapSelected.MapPids[AutomapSelected.CurMap];
			const char* map_name=AutomapSelected.MapNames[AutomapSelected.CurMap].c_str();

			scr=0;
			PIP_DRAW_TEXT(map_name,FT_CENTERX,COLOR_TEXT_GREEN);
			scr+=2;

			// Draw already builded minimap
			if(map_pid==AutomapCurMapPid)
			{
				FLTRECT stencil;
				stencil(PipWMonitor.L+PipX,PipWMonitor.T+PipY,PipWMonitor.R+PipX,PipWMonitor.B+PipY);
				FLTPOINT offset;
				offset(AutomapScrX,AutomapScrY);
				SprMngr.DrawPoints(AutomapPoints,D3DPT_LINELIST,&AutomapZoom,&stencil,&offset);
				break;
			}

			// Check wait of data
			if(AutomapWaitPids.count(map_pid))
			{
				PIP_DRAW_TEXT(MsgGame->GetStr(STR_AUTOMAP_LOADING),FT_CENTERX,COLOR_TEXT);
				break;
			}

			// Try load map
			WORD maxhx,maxhy;
			ItemVec items;
			if(!HexMngr.GetMapData(map_pid,items,maxhx,maxhy))
			{
				// Check for already received
				if(AutomapReceivedPids.count(map_pid))
				{
					PIP_DRAW_TEXT(MsgGame->GetStr(STR_AUTOMAP_LOADING_ERROR),FT_CENTERX,COLOR_TEXT);
					break;
				}

				Net_SendGiveMap(true,map_pid,AutomapSelected.LocId,0,0,0);
				AutomapWaitPids.insert(map_pid);
				break;
			}

			// Build minimap
			AutomapPoints.clear();
			AutomapScrX=PipX-maxhx*2/2+PipWMonitor.W()/2;
			AutomapScrY=PipY-maxhy*2/2+PipWMonitor.H()/2;
			for(ItemVecIt it=items.begin(),end=items.end();it!=end;++it)
			{
				Item& item=*it;
				WORD pid=item.GetProtoId();
				if(pid==SP_SCEN_IBLOCK || pid==SP_MISC_SCRBLOCK) continue;

				DWORD color;
				if(pid==SP_GRID_EXITGRID) color=0x3FFF7F00;
				else if(item.Proto->IsWall()) color=0xFF00FF00;
				else if(item.Proto->IsScen()) color=0x7F00FF00;
				else if(item.Proto->IsGrid()) color=0x7F00FF00;
				else continue;

				int x=PipWMonitor.L+maxhx*2-item.ACC_HEX.HexX*2;
				int y=PipWMonitor.T+item.ACC_HEX.HexY*2;

				AutomapPoints.push_back(PrepPoint(x,y,color));
				AutomapPoints.push_back(PrepPoint(x+1,y+1,color));
			}
			AutomapCurMapPid=map_pid;
			AutomapZoom=1.0f;
		}
		break;
	case PIP__ARCHIVES:
		{
			PIP_DRAW_TEXT(FmtGameText(STR_PIP_INFO),FT_CENTERX,COLOR_TEXT_DGREEN); scr++;
			for(int i=0;i<MAX_HOLO_INFO;i++)
			{
				DWORD num=HoloInfo[i];
				if(!num)
				{
					scr++;
					continue;
				}
				PIP_DRAW_TEXT(GetHoloText(STR_HOLO_INFO_NAME_(num)),0,COLOR_TEXT); scr++;
			}
		}
		break;
	case PIP__ARCHIVES_INFO:
		{
			SprMngr.DrawStr(INTRECT(PipWMonitor,PipX,PipY),GetHoloText(STR_HOLO_INFO_DESC_(PipInfoNum)),FT_COLORIZE|FT_SKIPLINES(PipScroll[PipMode])|FT_ALIGN);
		}
		break;
	default:
		break;
	}

	// Time
	SprMngr.DrawStr(INTRECT(PipWTime,PipX,PipY),Str::Format("%02d",GameOpt.Day),0,COLOR_IFACE,FONT_NUM); //Day
	char mval='0'+GameOpt.Month-1+0x30; //Month
	SprMngr.DrawStr(INTRECT(PipWTime,PipX+26,PipY+1),Str::Format("%c",mval),0,COLOR_IFACE,FONT_NUM); //Month
	SprMngr.DrawStr(INTRECT(PipWTime,PipX+63,PipY),Str::Format("%04d",GameOpt.Year),0,COLOR_IFACE,FONT_NUM); //Year
	SprMngr.DrawStr(INTRECT(PipWTime,PipX+135,PipY),Str::Format("%02d%02d",GameOpt.Hour,GameOpt.Minute),0,COLOR_IFACE,FONT_NUM); //Hour,Minute
}

void FOClient::PipLMouseDown()
{
	IfaceHold=IFACE_NONE;
	if(!Chosen) return;

	INTRECT& r=PipWMonitor;
	int ml=SprMngr.GetLinesCount(0,r.H(),NULL,FONT_DEFAULT);
	int h=r.H()/ml;
	int scr=-(int)PipScroll[PipMode];

	if(IsCurInRect(PipBStatus,PipX,PipY)) IfaceHold=IFACE_PIP_STATUS;
	//else if(IsCurInRect(PipBGames,PipX,PipY)) PipHold=IFACE_PIP_GAMES;
	else if(IsCurInRect(PipBAutomaps,PipX,PipY)) IfaceHold=IFACE_PIP_AUTOMAPS;
	else if(IsCurInRect(PipBArchives,PipX,PipY)) IfaceHold=IFACE_PIP_ARCHIVES;
	else if(IsCurInRect(PipBClose,PipX,PipY)) IfaceHold=IFACE_PIP_CLOSE;
	else if(IsCurInRect(PipWMonitor,PipX,PipY))
	{
		switch(PipMode)
		{
		case PIP__STATUS:
			{
				scr+=8;
				for(int j=TIMEOUT_END;j>=TIMEOUT_BEGIN;j--) if(MsgGame->Count(STR_PARAM_NAME_(j))) scr++;
				int scr_=scr;

				QuestTabMap* tabs=QuestMngr.GetTabs();
				for(QuestTabMapIt it=tabs->begin(),end=tabs->end();it!=end;++it)
				{
					if(scr>=0 && scr<ml && IsCurInRect(INTRECT(r[0],r[1]+scr*h,r[2],r[1]+scr*h+h),PipX,PipY))
					{
						QuestNumTab=scr-scr_+PipScroll[PipMode];
						PipMode=PIP__STATUS_QUESTS;
						PipScroll[PipMode]=0;
						break;
					}
					scr++;
				}
				if(PipMode==PIP__STATUS)
				{
					scr++;
					if(scr>=0 && scr<ml && IsCurInRect(INTRECT(r[0],r[1]+scr*h,r[2],r[1]+scr*h+h),PipX,PipY))
					{
						PipMode=PIP__STATUS_SCORES;
						PipScroll[PipMode]=0;
						if(Timer::FastTick()>=ScoresNextUploadTick)
						{
							Net_SendGetScores();
							ScoresNextUploadTick=Timer::FastTick()+SCORES_SEND_TIME;
						}
						break;
					}
				}
			}
			break;
//		case PIP__GAMES:
//			PipMode=PIP__STATUS;
//			break;
		case PIP__AUTOMAPS:
			{
				scr+=2;
				for(size_t i=0,j=Automaps.size();i<j;i++)
				{
					Automap& amap=Automaps[i];

					if(scr>=0 && scr<ml && IsCurInRect(INTRECT(r[0],r[1]+scr*h,r[2],r[1]+scr*h+h),PipX,PipY))
					{
						PipMode=PIP__AUTOMAPS_LOC;
						AutomapSelected=amap;
						PipScroll[PipMode]=0;
						break;
					}
					scr++;

					for(size_t k=0,l=amap.MapNames.size();k<l;k++)
					{
						if(scr>=0 && scr<ml && IsCurInRect(INTRECT(r[0],r[1]+scr*h,r[2],r[1]+scr*h+h),PipX,PipY))
						{
							PipMode=PIP__AUTOMAPS_MAP;
							AutomapSelected=amap;
							AutomapSelected.CurMap=k;
							PipScroll[PipMode]=0;
							AutomapCurMapPid=0;
							i=j;
							break;
						}
						scr++;
					}
					scr++;
				}
			}
			break;
		case PIP__AUTOMAPS_MAP:
			{
				PipVectX=CurX-AutomapScrX;
				PipVectY=CurY-AutomapScrY;
				IfaceHold=IFACE_PIP_AUTOMAPS_SCR;
			}
			break;
		case PIP__ARCHIVES:
			{
				scr+=1;
				for(int i=0;i<MAX_HOLO_INFO;i++)
				{
					if(scr>=0 && scr<ml && IsCurInRect(INTRECT(r[0],r[1]+scr*h,r[2],r[1]+scr*h+h),PipX,PipY))
					{
						PipInfoNum=HoloInfo[scr-1+PipScroll[PipMode]];
						if(!PipInfoNum) break;
						PipMode=PIP__ARCHIVES_INFO;
						PipScroll[PipMode]=0;
						break;
					}
					scr++;
				}
			}
			break;
		default:
			break;
		}
	}
	else if(IsCurInRect(PipWMain,PipX,PipY))
	{
		PipVectX=CurX-PipX;
		PipVectY=CurY-PipY;
		IfaceHold=IFACE_PIP_MAIN;
	}
}

void FOClient::PipLMouseUp()
{
	switch(IfaceHold)
	{
	case IFACE_PIP_STATUS:
		if(!IsCurInRect(PipBStatus,PipX,PipY)) break;
		PipMode=PIP__STATUS;
		break;
//	case IFACE_PIP_GAMES:
//		if(!IsCurInRect(PipBGames,PipX,PipY)) break;
//		PipMode=PIP__GAMES;
//		break;
	case IFACE_PIP_AUTOMAPS:
		if(!IsCurInRect(PipBAutomaps,PipX,PipY)) break;
		PipMode=PIP__AUTOMAPS;
		break;
	case IFACE_PIP_ARCHIVES:
		if(!IsCurInRect(PipBArchives,PipX,PipY)) break;
		PipMode=PIP__ARCHIVES;
		break;
	case IFACE_PIP_CLOSE:
		if(!IsCurInRect(PipBClose,PipX,PipY)) break;
		ShowScreen(SCREEN_NONE);
		break;
	default:
		break;
	}
	
	IfaceHold=IFACE_NONE;
}

void FOClient::PipRMouseDown()
{
	if(IsCurInRect(PipWMonitor,PipX,PipY))
	{
		switch(PipMode)
		{
		case PIP__STATUS_QUESTS:
			PipMode=PIP__STATUS;
			break;
		case PIP__STATUS_SCORES:
			PipMode=PIP__STATUS;
			break;
		case PIP__AUTOMAPS_LOC:
			PipMode=PIP__AUTOMAPS;
			break;
		case PIP__AUTOMAPS_MAP:
			PipMode=PIP__AUTOMAPS;
			break;
		case PIP__ARCHIVES_INFO:
			PipMode=PIP__ARCHIVES;
			break;
		default:
			break;
		}
	}
}

void FOClient::PipMouseMove()
{
	if(IfaceHold==IFACE_PIP_MAIN)
	{
		AutomapScrX-=(float)PipX;
		AutomapScrY-=(float)PipY;
		PipX=CurX-PipVectX;
		PipY=CurY-PipVectY;

		if(PipX<0) PipX=0;
		if(PipX+PipWMain[2]>MODE_WIDTH) PipX=MODE_WIDTH-PipWMain[2];
		if(PipY<0) PipY=0;
		if(PipY+PipWMain[3]>MODE_HEIGHT) PipY=MODE_HEIGHT-PipWMain[3];
		AutomapScrX+=(float)PipX;
		AutomapScrY+=(float)PipY;
	}
	else if(IfaceHold==IFACE_PIP_AUTOMAPS_SCR)
	{
		AutomapScrX=CurX-PipVectX;
		AutomapScrY=CurY-PipVectY;
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::AimDraw()
{
	SprMngr.DrawSprite(AimPWMain,AimWMain[0]+AimX,AimWMain[1]+AimY);

	if(AimPic) SprMngr.DrawSprite(AimPic,AimWMain[0]+AimPicX+AimX,AimWMain[1]+AimPicY+AimY);
	if(IfaceHold==IFACE_AIM_CANCEL) SprMngr.DrawSprite(AimPBCancelDn,AimBCancel[0]+AimX,AimBCancel[1]+AimY);
	
	SprMngr.Flush();

	CritterCl* cr=GetCritter(AimTargetId);
	if(!Chosen || !cr) return;

	if(GameOpt.ApCostAimArms==GameOpt.ApCostAimTorso && GameOpt.ApCostAimTorso==GameOpt.ApCostAimLegs && GameOpt.ApCostAimLegs==GameOpt.ApCostAimGroin && GameOpt.ApCostAimGroin==GameOpt.ApCostAimEyes && GameOpt.ApCostAimEyes==GameOpt.ApCostAimHead)
	{
		SprMngr.DrawStr(INTRECT(AimWHeadT,AimX,AimY),Str::Format("%s",MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_HEAD-1)),FT_COLORIZE|FT_NOBREAK,IfaceHold==IFACE_AIM_HEAD?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWLArmT,AimX,AimY),Str::Format("%s",MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_LEFT_ARM-1)),FT_COLORIZE|FT_NOBREAK|FT_CENTERR,IfaceHold==IFACE_AIM_LARM?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWRArmT,AimX,AimY),Str::Format("%s",MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_RIGHT_ARM-1)),FT_COLORIZE|FT_NOBREAK,IfaceHold==IFACE_AIM_RARM?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWTorsoT,AimX,AimY),Str::Format("%s",MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_TORSO-1)),FT_COLORIZE|FT_NOBREAK|FT_CENTERR,IfaceHold==IFACE_AIM_TORSO?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWRLegT,AimX,AimY),Str::Format("%s",MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_RIGHT_LEG-1)),FT_COLORIZE|FT_NOBREAK,IfaceHold==IFACE_AIM_RLEG?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWLLegT,AimX,AimY),Str::Format("%s",MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_LEFT_LEG-1)),FT_COLORIZE|FT_NOBREAK|FT_CENTERR,IfaceHold==IFACE_AIM_LLEG?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWEyesT,AimX,AimY),Str::Format("%s",MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_EYES-1)),FT_COLORIZE|FT_NOBREAK,IfaceHold==IFACE_AIM_EYES?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWGroinT,AimX,AimY),Str::Format("%s",MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_GROIN-1)),FT_COLORIZE|FT_NOBREAK|FT_CENTERR,IfaceHold==IFACE_AIM_GROIN?COLOR_TEXT_RED:COLOR_TEXT);
	}
	else
	{
		SprMngr.DrawStr(INTRECT(AimWHeadT,AimX,AimY),Str::Format("%s (%u)",MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_HEAD-1),GameOpt.ApCostAimHead),FT_COLORIZE|FT_NOBREAK,IfaceHold==IFACE_AIM_HEAD?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWLArmT,AimX,AimY),Str::Format("(%u) %s",GameOpt.ApCostAimArms,MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_LEFT_ARM-1)),FT_COLORIZE|FT_NOBREAK|FT_CENTERR,IfaceHold==IFACE_AIM_LARM?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWRArmT,AimX,AimY),Str::Format("%s (%u)",MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_RIGHT_ARM-1),GameOpt.ApCostAimArms),FT_COLORIZE|FT_NOBREAK,IfaceHold==IFACE_AIM_RARM?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWTorsoT,AimX,AimY),Str::Format("(%u) %s",GameOpt.ApCostAimTorso,MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_TORSO-1)),FT_COLORIZE|FT_NOBREAK|FT_CENTERR,IfaceHold==IFACE_AIM_TORSO?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWRLegT,AimX,AimY),Str::Format("%s (%u)",MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_RIGHT_LEG-1),GameOpt.ApCostAimLegs),FT_COLORIZE|FT_NOBREAK,IfaceHold==IFACE_AIM_RLEG?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWLLegT,AimX,AimY),Str::Format("(%u) %s",GameOpt.ApCostAimLegs,MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_LEFT_LEG-1)),FT_COLORIZE|FT_NOBREAK|FT_CENTERR,IfaceHold==IFACE_AIM_LLEG?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWEyesT,AimX,AimY),Str::Format("%s (%u)",MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_EYES-1),GameOpt.ApCostAimEyes),FT_COLORIZE|FT_NOBREAK,IfaceHold==IFACE_AIM_EYES?COLOR_TEXT_RED:COLOR_TEXT);
		SprMngr.DrawStr(INTRECT(AimWGroinT,AimX,AimY),Str::Format("(%u) %s",GameOpt.ApCostAimGroin,MsgCombat->GetStr(1000+cr->GetCrTypeAlias()*10+HIT_LOCATION_GROIN-1)),FT_COLORIZE|FT_NOBREAK|FT_CENTERR,IfaceHold==IFACE_AIM_GROIN?COLOR_TEXT_RED:COLOR_TEXT);
	}

	bool zero=!HexMngr.TraceBullet(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY(),Chosen->GetAttackDist(),0.0f,cr,false,NULL,0,NULL,NULL,NULL,true);
	SprMngr.DrawStr(INTRECT(AimWHeadP,AimX,AimY),Str::ItoA(zero?0:ScriptGetHitProc(cr,HIT_LOCATION_HEAD)),FT_COLORIZE|FT_NOBREAK|FT_CENTERX);
	SprMngr.DrawStr(INTRECT(AimWLArmP,AimX,AimY),Str::ItoA(zero?0:ScriptGetHitProc(cr,HIT_LOCATION_LEFT_ARM)),FT_COLORIZE|FT_NOBREAK|FT_CENTERX);
	SprMngr.DrawStr(INTRECT(AimWRArmP,AimX,AimY),Str::ItoA(zero?0:ScriptGetHitProc(cr,HIT_LOCATION_RIGHT_ARM)),FT_COLORIZE|FT_NOBREAK|FT_CENTERX);
	SprMngr.DrawStr(INTRECT(AimWTorsoP,AimX,AimY),Str::ItoA(zero?0:ScriptGetHitProc(cr,HIT_LOCATION_TORSO)),FT_COLORIZE|FT_NOBREAK|FT_CENTERX);
	SprMngr.DrawStr(INTRECT(AimWRLegP,AimX,AimY),Str::ItoA(zero?0:ScriptGetHitProc(cr,HIT_LOCATION_RIGHT_LEG)),FT_COLORIZE|FT_NOBREAK|FT_CENTERX);
	SprMngr.DrawStr(INTRECT(AimWLLegP,AimX,AimY),Str::ItoA(zero?0:ScriptGetHitProc(cr,HIT_LOCATION_LEFT_LEG)),FT_COLORIZE|FT_NOBREAK|FT_CENTERX);
	SprMngr.DrawStr(INTRECT(AimWEyesP,AimX,AimY),Str::ItoA(zero?0:ScriptGetHitProc(cr,HIT_LOCATION_EYES)),FT_COLORIZE|FT_NOBREAK|FT_CENTERX);
	SprMngr.DrawStr(INTRECT(AimWGroinP,AimX,AimY),Str::ItoA(zero?0:ScriptGetHitProc(cr,HIT_LOCATION_GROIN)),FT_COLORIZE|FT_NOBREAK|FT_CENTERX);
}

void FOClient::AimLMouseDown()
{
	IfaceHold=IFACE_NONE;
	
	if(IsCurInRect(AimBCancel,AimX,AimY)) IfaceHold=IFACE_AIM_CANCEL;
	else if(IsCurInRect(AimWHeadT,AimX,AimY)) IfaceHold=IFACE_AIM_HEAD;
	else if(IsCurInRect(AimWLArmT,AimX,AimY)) IfaceHold=IFACE_AIM_LARM;
	else if(IsCurInRect(AimWRArmT,AimX,AimY)) IfaceHold=IFACE_AIM_RARM;
	else if(IsCurInRect(AimWTorsoT,AimX,AimY)) IfaceHold=IFACE_AIM_TORSO;
	else if(IsCurInRect(AimWRLegT,AimX,AimY)) IfaceHold=IFACE_AIM_RLEG;
	else if(IsCurInRect(AimWLLegT,AimX,AimY)) IfaceHold=IFACE_AIM_LLEG;
	else if(IsCurInRect(AimWEyesT,AimX,AimY)) IfaceHold=IFACE_AIM_EYES;
	else if(IsCurInRect(AimWGroinT,AimX,AimY)) IfaceHold=IFACE_AIM_GROIN;
	else if(IsCurInRect(AimWMain,AimX,AimY))
	{
		AimVectX=CurX-AimX;
		AimVectY=CurY-AimY;
		IfaceHold=IFACE_AIM_MAIN;
	}
}

void FOClient::AimLMouseUp()
{
	if(!Chosen) return;

	switch(IfaceHold)
	{
	case IFACE_AIM_CANCEL:
		if(!IsCurInRect(AimBCancel,AimX,AimY)) break;
		ShowScreen(SCREEN_NONE);
		break;
	case IFACE_AIM_HEAD:
		if(!IsCurInRect(AimWHeadT,AimX,AimY)) break;
		Chosen->SetAim(HIT_LOCATION_HEAD);
		goto Label_Attack;
		break;
	case IFACE_AIM_LARM:
		if(!IsCurInRect(AimWLArmT,AimX,AimY)) break;
		Chosen->SetAim(HIT_LOCATION_LEFT_ARM);
		goto Label_Attack;
		break;
	case IFACE_AIM_RARM:
		if(!IsCurInRect(AimWRArmT,AimX,AimY)) break;
		Chosen->SetAim(HIT_LOCATION_RIGHT_ARM);
		goto Label_Attack;
		break;
	case IFACE_AIM_TORSO:
		if(!IsCurInRect(AimWTorsoT,AimX,AimY)) break;
		Chosen->SetAim(HIT_LOCATION_TORSO);
		goto Label_Attack;
		break;
	case IFACE_AIM_RLEG:
		if(!IsCurInRect(AimWRLegT,AimX,AimY)) break;
		Chosen->SetAim(HIT_LOCATION_RIGHT_LEG);
		goto Label_Attack;
		break;
	case IFACE_AIM_LLEG:
		if(!IsCurInRect(AimWLLegT,AimX,AimY)) break;
		Chosen->SetAim(HIT_LOCATION_LEFT_LEG);
		goto Label_Attack;
		break;
	case IFACE_AIM_EYES:
		if(!IsCurInRect(AimWEyesT,AimX,AimY)) break;
		Chosen->SetAim(HIT_LOCATION_EYES);
		goto Label_Attack;
		break;
	case IFACE_AIM_GROIN:
		if(!IsCurInRect(AimWGroinT,AimX,AimY)) break;
		Chosen->SetAim(HIT_LOCATION_GROIN);
		goto Label_Attack;
		break;
	default:
		break;
	}

	IfaceHold=IFACE_NONE;
	return;

Label_Attack:
	CritterCl* cr=GetCritter(AimTargetId);
	if(cr) SetAction(CHOSEN_USE_ITEM,Chosen->ItemSlotMain->GetId(),Chosen->ItemSlotMain->GetProtoId(),TARGET_CRITTER,AimTargetId,Chosen->GetFullRate());

	IfaceHold=IFACE_NONE;
	if(!Keyb::ShiftDwn)
	{
		ShowScreen(SCREEN_NONE);
		SetCurMode(CUR_USE_WEAPON);
	}
}

void FOClient::AimMouseMove()
{
	if(IfaceHold==IFACE_AIM_MAIN)
	{
		AimX=CurX-AimVectX;
		AimY=CurY-AimVectY;

		if(AimX<0) AimX=0;
		if(AimX+AimWMain[2]>MODE_WIDTH) AimX=MODE_WIDTH-AimWMain[2];
		if(AimY<0) AimY=0;
		if(AimY+AimWMain[3]>MODE_HEIGHT) AimY=MODE_HEIGHT-AimWMain[3];
	}
}

DWORD FOClient::AimGetPic(CritterCl* cr, const char* ext)
{
	char aim_name[MAX_FOPATH];
	char aim_name_alias[MAX_FOPATH];
	sprintf(aim_name,"%sna.%s",CritType::GetName(cr->GetCrType()),ext);
	sprintf(aim_name_alias,"%sna.%s",CritType::GetName(cr->GetCrTypeAlias()),ext);

	StrDwordMapIt it=AimLoadedPics.find(string(aim_name));
	if(it!=AimLoadedPics.end()) return (*it).second;
	it=AimLoadedPics.find(string(aim_name_alias));
	if(it!=AimLoadedPics.end()) return (*it).second;

	DWORD aim_pic=SprMngr.LoadSprite(aim_name,PT_ART_CRITTERS);
	if(!aim_pic) aim_pic=SprMngr.LoadSprite(aim_name_alias,PT_ART_CRITTERS);

	AimLoadedPics.insert(StrDwordMapVal(string(aim_name),aim_pic));
	return aim_pic;
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::PupDraw() 
{
	SprMngr.DrawSprite(PupPMain,PupX,PupY);

	if(!Chosen) return;

	// Info window
	if(PupTransferType==TRANSFER_HEX_CONT_UP || PupTransferType==TRANSFER_HEX_CONT_DOWN || PupTransferType==TRANSFER_FAR_CONT)
	{
		ProtoItem* proto_item=ItemMngr.GetProtoItem(PupContPid);
		if(proto_item)
		{
			DWORD spr_id=ResMngr.GetSprId(proto_item->PicMapHash,proto_item->Dir);
			if(spr_id) SprMngr.DrawSpriteSize(spr_id,PupWInfo[0]+PupX,PupWInfo[1]+PupY,PupWInfo.W(),PupWInfo.H(),false,true);
		}
	}
	else if(PupTransferType==TRANSFER_CRIT_STEAL || PupTransferType==TRANSFER_CRIT_LOOT || PupTransferType==TRANSFER_FAR_CRIT)
	{
		CritterCl* cr=HexMngr.GetCritter(PupContId);
		if(cr) cr->DrawStay(INTRECT(PupWInfo,PupX,PupY));
	}

	// Button Ok
	if(IfaceHold==IFACE_PUP_OK) SprMngr.DrawSprite(PupPBOkOn,PupBOk[0]+PupX,PupBOk[1]+PupY);

	// Button Take All
	if(IfaceHold==IFACE_PUP_TAKEALL) SprMngr.DrawSprite(PupPTakeAllOn,PupBTakeAll[0]+PupX,PupBTakeAll[1]+PupY);

	// Scrolling Buttons Window 1
	if(PupScroll1<=0)
		SprMngr.DrawSprite(PupPBScrUpOff1,PupBScrUp1[0]+PupX,PupBScrUp1[1]+PupY);
	else if(IfaceHold==IFACE_PUP_SCRUP1)
		SprMngr.DrawSprite(PupPBScrUpOn1,PupBScrUp1[0]+PupX,PupBScrUp1[1]+PupY);

	int count_items=Chosen->GetItemsCountInv();
	if(PupScroll1>=count_items-(PupWCont1[3]-PupWCont1[1])/PupHeightItem1)
		SprMngr.DrawSprite(PupPBScrDwOff1,PupBScrDw1[0]+PupX,PupBScrDw1[1]+PupY);
	else if(IfaceHold==IFACE_PUP_SCRDOWN1)
		SprMngr.DrawSprite(PupPBScrDwOn1,PupBScrDw1[0]+PupX,PupBScrDw1[1]+PupY);

	// Scrolling Buttons Window 2
	if(PupScroll2<=0)
		SprMngr.DrawSprite(PupPBScrUpOff2,PupBScrUp2[0]+PupX,PupBScrUp2[1]+PupY);
	else if(IfaceHold==IFACE_PUP_SCRUP2)
		SprMngr.DrawSprite(PupPBScrUpOn2,PupBScrUp2[0]+PupX,PupBScrUp2[1]+PupY);

	count_items=PupCont2.size();
	if(PupScroll2>=count_items-(PupWCont2[3]-PupWCont2[1])/PupHeightItem2)
		SprMngr.DrawSprite(PupPBScrDwOff2,PupBScrDw2[0]+PupX,PupBScrDw2[1]+PupY);
	else if(IfaceHold==IFACE_PUP_SCRDOWN2)
		SprMngr.DrawSprite(PupPBScrDwOn2,PupBScrDw2[0]+PupX,PupBScrDw2[1]+PupY);

	// Scrolling critters
	if(PupGetLootCrits().size()>1)
	{
		if(IfaceHold==IFACE_PUP_SCRCR_L)
			SprMngr.DrawSprite(PupBNextCritLeftPicDown,PupBNextCritLeft[0]+PupX,PupBNextCritLeft[1]+PupY);
		else
			SprMngr.DrawSprite(PupBNextCritLeftPicUp,PupBNextCritLeft[0]+PupX,PupBNextCritLeft[1]+PupY);

		if(IfaceHold==IFACE_PUP_SCRCR_R)
			SprMngr.DrawSprite(PupBNextCritRightPicDown,PupBNextCritRight[0]+PupX,PupBNextCritRight[1]+PupY);
		else
			SprMngr.DrawSprite(PupBNextCritRightPicUp,PupBNextCritRight[0]+PupX,PupBNextCritRight[1]+PupY);
	}

	// Items
	ContainerDraw(INTRECT(PupWCont1,PupX,PupY),PupHeightItem1,PupScroll1,PupCont1,IfaceHold==IFACE_PUP_CONT1?PupHoldId:0);
	ContainerDraw(INTRECT(PupWCont2,PupX,PupY),PupHeightItem2,PupScroll2,PupCont2,IfaceHold==IFACE_PUP_CONT2?PupHoldId:0);
}

void FOClient::PupLMouseDown()
{
	PupHoldId=0;
	IfaceHold=IFACE_NONE;
	if(!Chosen) return;

	if(IsCurInRect(PupWCont1,PupX,PupY))
	{
		PupHoldId=GetCurContainerItemId(INTRECT(PupWCont1,PupX,PupY),PupHeightItem1,PupScroll1,PupCont1);
		if(PupHoldId) IfaceHold=IFACE_PUP_CONT1;
	}
	else if(IsCurInRect(PupWCont2,PupX,PupY))
	{
		PupHoldId=GetCurContainerItemId(INTRECT(PupWCont2,PupX,PupY),PupHeightItem2,PupScroll2,PupCont2);
		if(PupHoldId) IfaceHold=IFACE_PUP_CONT2;
	}
	else if(IsCurInRect(PupBOk,PupX,PupY)) IfaceHold=IFACE_PUP_OK;
	else if(IsCurInRect(PupBScrUp1,PupX,PupY))
	{
		Timer::StartAccelerator(ACCELERATE_PUP_SCRUP1);
		IfaceHold=IFACE_PUP_SCRUP1;
	}
	else if(IsCurInRect(PupBScrDw1,PupX,PupY))
	{
		Timer::StartAccelerator(ACCELERATE_PUP_SCRDOWN1);
		IfaceHold=IFACE_PUP_SCRDOWN1;
	}
	else if(IsCurInRect(PupBScrUp2,PupX,PupY))
	{
		Timer::StartAccelerator(ACCELERATE_PUP_SCRUP2);
		IfaceHold=IFACE_PUP_SCRUP2;
	}
	else if(IsCurInRect(PupBScrDw2,PupX,PupY))
	{
		Timer::StartAccelerator(ACCELERATE_PUP_SCRDOWN2);
		IfaceHold=IFACE_PUP_SCRDOWN2;
	}
	else if(IsCurInRect(PupBTakeAll,PupX,PupY)) IfaceHold=IFACE_PUP_TAKEALL;
	else if(IsCurInRect(PupBNextCritLeft,PupX,PupY) && PupGetLootCrits().size()>1)
	{
		if(Chosen->IsFree() && ChosenAction.empty()) IfaceHold=IFACE_PUP_SCRCR_L;
	}
	else if(IsCurInRect(PupBNextCritRight,PupX,PupY) && PupGetLootCrits().size()>1)
	{
		if(Chosen->IsFree() && ChosenAction.empty()) IfaceHold=IFACE_PUP_SCRCR_R;
	}
	else if(IsCurInRect(PupWMain,PupX,PupY))
	{
		PupVectX=CurX-PupX;
		PupVectY=CurY-PupY;
		IfaceHold=IFACE_PUP_MAIN;
	}

	if(IsCurMode(CUR_DEFAULT) && (IfaceHold==IFACE_PUP_CONT1 || IfaceHold==IFACE_PUP_CONT2))
	{
		IfaceHold=IFACE_NONE;
		LMenuTryActivate();
	}
}

void FOClient::PupLMouseUp()
{
	if(!Chosen) IfaceHold=IFACE_NONE;

	switch(IfaceHold)
	{
	case IFACE_PUP_CONT2:
		{
			if(!IsCurInRect(PupWCont1,PupX,PupY)) break;

			ItemVecIt it=std::find(PupCont2.begin(),PupCont2.end(),PupHoldId);
			if(it!=PupCont2.end())
			{
				Item& item=*it;
				if(item.GetCount()>1)
					SplitStart(&item,IFACE_PUP_CONT2);
				else
					SetAction(CHOSEN_MOVE_ITEM_CONT,PupHoldId,IFACE_PUP_CONT2,1);
			}
		}
		break;
	case IFACE_PUP_CONT1:
		{
			if(!IsCurInRect(PupWCont2,PupX,PupY)) break;

			ItemVecIt it=std::find(PupCont1.begin(),PupCont1.end(),PupHoldId);
			if(it!=PupCont1.end())
			{
				Item& item=*it;
				if(item.GetCount()>1)
					SplitStart(&item,IFACE_PUP_CONT1);
				else
					SetAction(CHOSEN_MOVE_ITEM_CONT,PupHoldId,IFACE_PUP_CONT1,1);
			}
		}
		break;
	case IFACE_PUP_OK:
		{
			if(!IsCurInRect(PupBOk,PupX,PupY)) break;
			ShowScreen(SCREEN_NONE);
		}
		break;
	case IFACE_PUP_SCRUP1:
		{
			if(!IsCurInRect(PupBScrUp1,PupX,PupY)) break;
			if(PupScroll1>0) PupScroll1--;
		}
		break;
	case IFACE_PUP_SCRDOWN1:
		{
			if(!IsCurInRect(PupBScrDw1,PupX,PupY)) break;
			if(PupScroll1<(int)Chosen->GetItemsCountInv()-(PupWCont2[3]-PupWCont2[1])/PupHeightItem2)
				PupScroll1++;
		}
		break;
	case IFACE_PUP_SCRUP2:
		{
			if(!IsCurInRect(PupBScrUp2,PupX,PupY)) break;
			if(PupScroll2>0) PupScroll2--;
		}
		break;
	case IFACE_PUP_SCRDOWN2:
		{
			if(!IsCurInRect(PupBScrDw2,PupX,PupY)) break;
			if(PupScroll2<(int)PupCont2.size()-(PupWCont1[3]-PupWCont1[1])/PupHeightItem1)
				PupScroll2++;
		}
		break;
	case IFACE_PUP_TAKEALL:
		{
			if(PupTransferType==TRANSFER_CRIT_STEAL) break;
			if(!IsCurInRect(PupBTakeAll,PupX,PupY)) break;
			SetAction(CHOSEN_TAKE_ALL);
		}
		break;
	case IFACE_PUP_SCRCR_L:
		{
			if(!IsCurInRect(PupBNextCritLeft,PupX,PupY)) break;
			if(!Chosen->IsFree() || !ChosenAction.empty()) break;
			int cnt=PupGetLootCrits().size();
			if(cnt<2) break;
			if(!PupScrollCrit) PupScrollCrit=cnt-1;
			else PupScrollCrit--;
			CritterCl* cr=PupGetLootCrit(PupScrollCrit);
			if(!cr) break;
			SetAction(CHOSEN_PICK_CRIT,cr->GetId(),0);
		}
		break;
	case IFACE_PUP_SCRCR_R:
		{
			if(!IsCurInRect(PupBNextCritRight,PupX,PupY)) break;
			if(!Chosen->IsFree() || !ChosenAction.empty()) break;
			int cnt=PupGetLootCrits().size();
			if(cnt<2) break;
			if(PupScrollCrit+1>=cnt) PupScrollCrit=0;
			else PupScrollCrit++;
			CritterCl* cr=PupGetLootCrit(PupScrollCrit);
			if(!cr) break;
			SetAction(CHOSEN_PICK_CRIT,cr->GetId(),0);
		}
		break;
	default:
		break;
	}

	IfaceHold=IFACE_NONE;
	PupHoldId=0;
}

void FOClient::PupMouseMove()
{
	if(IfaceHold==IFACE_PUP_MAIN)
	{
		PupX=CurX-PupVectX;
		PupY=CurY-PupVectY;

		if(PupX<0) PupX=0;
		if(PupX+PupWMain[2]>MODE_WIDTH) PupX=MODE_WIDTH-PupWMain[2];
		if(PupY<0) PupY=0;
		//if(PupY+PupWMain[3]>IntY) PupY=IntY-PupWMain[3];
		if(PupY+PupWMain[3]>MODE_HEIGHT) PupY=MODE_HEIGHT-PupWMain[3];
	}
}

void FOClient::PupRMouseDown()
{
	SetCurCastling(CUR_DEFAULT,CUR_HAND);
}

void FOClient::PupTransfer(DWORD item_id, DWORD cont, DWORD count)
{
	if(!count) return;

	// From Chosen to container
	if(cont==IFACE_PUP_CONT1)
	{
		Item* item=Chosen->GetItem(item_id);
		if(!item) return;

		PupLastPutId=item_id;
		Net_SendItemCont(PupTransferType,PupContId,item_id,count,CONT_PUT);
		WaitPing();
	}
	// From container to Chosen
	else if(cont==IFACE_PUP_CONT2)
	{
		ItemVecIt it=std::find(PupCont2Init.begin(),PupCont2Init.end(),item_id);
		if(it==PupCont2Init.end()) return;
		Item& item=*it;

		if(item.IsGrouped() && count<item.GetCount())
			item.Count_Sub(count);
		else
			PupCont2Init.erase(it);

		Net_SendItemCont(PupTransferType,PupContId,item_id,count,CONT_GET);
		WaitPing();
	}
	CollectContItems();
}

CritVec& FOClient::PupGetLootCrits()
{
	static CritVec loot;
	loot.clear();
	if(PupTransferType!=TRANSFER_CRIT_LOOT) return loot;
	CritterCl* loot_cr=HexMngr.GetCritter(PupContId);
	if(!loot_cr || !loot_cr->IsDead()) return loot;
	Field& f=HexMngr.GetField(loot_cr->GetHexX(),loot_cr->GetHexY());
	for(int i=0,j=f.DeadCrits.size();i<j;i++)
		if(!f.DeadCrits[i]->IsPerk(MODE_NO_LOOT)) loot.push_back(f.DeadCrits[i]);
	return loot;
}

CritterCl* FOClient::PupGetLootCrit(int scroll)
{
	CritVec& loot=PupGetLootCrits();
	for(int i=0,j=loot.size();i<j;i++)
	{
		if(i==scroll) return loot[i];
	}
	return NULL;
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::CurDraw()
{
	if(GameOpt.HideCursor) return;

	DWORD spr_id;
	SpriteInfo* si=NULL;
	int x=0,y=0;

	// Wait
	if(IsCurMode(CUR_WAIT))
	{
		if(!(si=SprMngr.GetSpriteInfo(CurPWait))) return;
		x=CurX-(si->Width/2)+si->OffsX;
		y=CurY-si->Height+si->OffsY;
		SprMngr.DrawSprite(CurPWait,x,y);
		return;
	}

	// Game scroll
	if(IsMainScreen(SCREEN_GAME) && GetActiveScreen()==SCREEN_NONE)
	{
		DWORD cur_di=0;

		if(GameOpt.ScrollMouseRight) cur_di=CurPScrRt;
		else if(GameOpt.ScrollMouseLeft) cur_di=CurPScrLt;
		else if(GameOpt.ScrollMouseUp) cur_di=CurPScrUp;
		else if(GameOpt.ScrollMouseDown) cur_di=CurPScrDw;

		if		(GameOpt.ScrollMouseRight&& GameOpt.ScrollMouseUp	) cur_di=CurPScrRU;
		else if	(GameOpt.ScrollMouseLeft	&& GameOpt.ScrollMouseUp	) cur_di=CurPScrLU;
		else if	(GameOpt.ScrollMouseRight&& GameOpt.ScrollMouseDown) cur_di=CurPScrRD;
		else if	(GameOpt.ScrollMouseLeft	&& GameOpt.ScrollMouseDown) cur_di=CurPScrLD;

		if(cur_di)
		{
			if(!(si=SprMngr.GetSpriteInfo(CurPDef))) return;

			//	x=cur_x-si->offs_x;
			//	y=cur_y-si->offs_y;
			x=CurX-(si->Width/2)+si->OffsX;
			y=CurY-si->Height+si->OffsY;

			if(cur_di==CurPScrRt) {x-=si->Width; y-=si->Height/2;}
			else if(cur_di==CurPScrLt) y-=si->Height/2;
			else if(cur_di==CurPScrUp) x-=si->Width/2;
			else if(cur_di==CurPScrDw) {x-=si->Width/2; y-=si->Height;}
			else if(cur_di==CurPScrRU) x-=si->Width;
			else if(cur_di==CurPScrRD) {x-=si->Width; y-=si->Height;}
			else if(cur_di==CurPScrLD) y-=si->Height;

			SprMngr.DrawSprite(cur_di,x,y);
			return;
		}
	}

	// Messboxes scroll
	INTRECT rmb=MessBoxCurRectScroll();
	if(!rmb.IsZero() && IsCurInRect(rmb))
	{
		spr_id=IntPBScrDnDn;
		if(IsCurInRect(INTRECT(rmb.L,rmb.T,rmb.R,rmb.CY()))) spr_id=IntPBScrUpDn;
		si=SprMngr.GetSpriteInfo(spr_id);
		if(si)
		{
			SprMngr.DrawSprite(spr_id,CurX-si->Width/2,CurY-si->Height/2);
			return;
		}
	}

	// Other cursors
	switch(GetCurMode())
	{
	case CUR_USE_WEAPON:
		{
			if(!(si=SprMngr.GetSpriteInfo(CurPUseItem))) return;
			x=CurX-(si->Width/2)+si->OffsX;
			y=CurY-si->Height+si->OffsY;
			SprMngr.DrawSprite(CurPUseItem,x,y);

			CritterCl* cr=HexMngr.GetCritterPixel(CurX,CurY,true);
			if(!cr || !Chosen || cr==Chosen) break;

			if(!HexMngr.TraceBullet(Chosen->GetHexX(),Chosen->GetHexY(),cr->GetHexX(),cr->GetHexY(),Chosen->GetAttackDist(),0.0f,cr,false,NULL,0,NULL,NULL,NULL,true)) break;

			int hit=ScriptGetHitProc(cr,HIT_LOCATION_NONE);
			if(!hit) break;

			char str[16];
			sprintf(str,"%d%%",hit);

			SprMngr.Flush();
			SprMngr.DrawStr(INTRECT(CurX+6,CurY+6,x+500,y+500),str,0,COLOR_TEXT_RED);
		}
		break;
	case CUR_USE_ITEM:
	case CUR_USE_SKILL:
		{
			if(!(si=SprMngr.GetSpriteInfo(CurPUseSkill))) return;
			x=CurX-(si->Width/2)+si->OffsX;
			y=CurY-si->Height+si->OffsY;
			SprMngr.DrawSprite(CurPUseSkill,x,y);
		}
		break;
	case CUR_MOVE:
		if(GetMouseHex()) break;
		/*{
			Field& f=HexMngr.HexField[TargetY][TargetX];
			if(!(si=SprMngr.GetSpriteInfo(CurPMove))) return;

			if(HexMngr.IsHexPassed(TargetX,TargetY))
				SprMngr.DrawSpriteSize(CurPMove,(f.ScrX+1+GameOpt.ScrOx)/GameOpt.SpritesZoom,(f.ScrY-1+GameOpt.ScrOy)/GameOpt.SpritesZoom,si->Width/GameOpt.SpritesZoom,si->Height/GameOpt.SpritesZoom,TRUE,FALSE);
			else
				SprMngr.DrawSpriteSize(CurPMoveBlock,(f.ScrX+1+GameOpt.ScrOx)/GameOpt.SpritesZoom,(f.ScrY-1+GameOpt.ScrOy)/GameOpt.SpritesZoom,si->Width/GameOpt.SpritesZoom,si->Height/GameOpt.SpritesZoom,TRUE,FALSE);

			break;
		}*/
		// Draw default
	case CUR_DEFAULT:
		if(!(si=SprMngr.GetSpriteInfo(CurPDef))) return;
		if(IsLMenu())
		{
			x=LMenuRestoreCurX-(si->Width/2)+si->OffsX;
			y=LMenuRestoreCurY-si->Height+si->OffsY;
		}
		else
		{
			x=CurX-(si->Width/2)+si->OffsX;
			y=CurY-si->Height+si->OffsY;
		}
		SprMngr.DrawSprite(CurPDef,x,y);
		break;
	case CUR_HAND:
		if(!Chosen) break;

		if(GetActiveScreen()==SCREEN__INVENTORY)
		{
			if(IfaceHold && InvHoldId)
			{
				Item* item=Chosen->GetItem(InvHoldId);
				if(!item) break;
				AnyFrames* anim=ResMngr.GetInvAnim(item->GetPicInv());
				if(!anim) break;
				spr_id=anim->GetCurSprId();
				if(!(si=SprMngr.GetSpriteInfo(spr_id))) return;
				x=CurX-(si->Width/2)+si->OffsX;
				y=CurY-(si->Height/2)+si->OffsY;
				SprMngr.DrawSprite(spr_id,x,y,item->GetInvColor());
			}
			else
			{
DrawCurHand:
				if(!(si=SprMngr.GetSpriteInfo(CurPHand))) return;
				x=CurX-(si->Width/2)+si->OffsX;
				y=CurY-si->Height+si->OffsY;
				SprMngr.DrawSprite(CurPHand,x,y);
				break;
			}
		}
		else if(GetActiveScreen()==SCREEN__BARTER)
		{
			if(IfaceHold && BarterHoldId)
			{
				ItemVec* cont=NULL;
				switch(IfaceHold)
				{
				case IFACE_BARTER_CONT1: cont=&InvContInit; break;
				case IFACE_BARTER_CONT2: cont=&BarterCont2Init; break;
				case IFACE_BARTER_CONT1O: cont=&BarterCont1oInit; break;
				case IFACE_BARTER_CONT2O: cont=&BarterCont2oInit; break;
				default: goto DrawCurHand;
				}

				ItemVecIt it=std::find(cont->begin(),cont->end(),BarterHoldId);
				if(it==cont->end()) goto DrawCurHand;
				Item* item=&(*it);

				AnyFrames* anim=ResMngr.GetInvAnim(item->GetPicInv());
				if(!anim) goto DrawCurHand;
				spr_id=anim->GetCurSprId();
				if(!(si=SprMngr.GetSpriteInfo(spr_id))) goto DrawCurHand;
				x=CurX-(si->Width/2)+si->OffsX;
				y=CurY-(si->Height/2)+si->OffsY;
				SprMngr.DrawSprite(spr_id,x,y,item->GetInvColor());
			}
			else goto DrawCurHand;
		}
		else if(GetActiveScreen()==SCREEN__PICKUP && PupHoldId)
		{
			Item* item=GetContainerItem(IfaceHold==IFACE_PUP_CONT1?PupCont1:PupCont2,PupHoldId);
			if(item)
			{
				AnyFrames* anim=ResMngr.GetInvAnim(item->GetPicInv());
				if(anim)
				{
					spr_id=anim->GetCurSprId();
					if(!(si=SprMngr.GetSpriteInfo(spr_id))) return;
					x=CurX-(si->Width/2)+si->OffsX;
					y=CurY-(si->Height/2)+si->OffsY;
					SprMngr.DrawSprite(spr_id,x,y,item->GetInvColor());
				}
			}
		}
		else goto DrawCurHand;
		break;
	default:
		SetCurMode(CUR_DEFAULT);
		break;
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::DlgboxDraw()
{
	// Check for end time
	if(DlgboxWait && Timer::GameTick()>DlgboxWait)
	{
		ShowScreen(SCREEN_NONE);
		return;
	}

	SprMngr.DrawSprite(DlgboxWTopPicNone,DlgboxWTop[0]+DlgboxX,DlgboxWTop[1]+DlgboxY);
	SprMngr.DrawStr(INTRECT(DlgboxWText,DlgboxX,DlgboxY),DlgboxText,FT_COLORIZE);
	DWORD y_offs=DlgboxWTop.H();
	for(int i=0;i<DlgboxButtonsCount;i++)
	{
		SprMngr.DrawSprite(DlgboxWMiddlePicNone,DlgboxWMiddle[0]+DlgboxX,DlgboxWMiddle[1]+DlgboxY+y_offs);
		if(IfaceHold==IFACE_DIALOG_BTN && i==DlgboxSelectedButton) SprMngr.DrawSprite(DlgboxBButtonPicDown,DlgboxBButton[0]+DlgboxX,DlgboxBButton[1]+DlgboxY+y_offs);
		SprMngr.DrawStr(INTRECT(DlgboxBButtonText,DlgboxX,DlgboxY+y_offs),DlgboxButtonText[i].c_str(),FT_NOBREAK|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
		y_offs+=DlgboxWMiddle.H();
	}
	SprMngr.DrawSprite(DlgboxWBottomPicNone,DlgboxWTop[0]+DlgboxX,DlgboxWTop[1]+DlgboxY+y_offs);

	//static char ftime[32];
	//sprintf(ftime,"Осталось времени: %d")
}

void FOClient::DlgboxLMouseDown()
{
	IfaceHold=IFACE_NONE;

	DWORD y_offs=DlgboxWTop.H();
	for(int i=0;i<DlgboxButtonsCount;i++)
	{
		if(IsCurInRect(DlgboxBButton,DlgboxX,DlgboxY+y_offs))
		{
			DlgboxSelectedButton=i;
			IfaceHold=IFACE_DIALOG_BTN;
			return;
		}
		if(IsCurInRect(DlgboxWMiddle,DlgboxX,DlgboxY+y_offs))
		{
			DlgboxVectX=CurX-DlgboxX;
			DlgboxVectY=CurY-DlgboxY;
			IfaceHold=IFACE_DIALOG_MAIN;
			return;
		}
		y_offs+=DlgboxWMiddle.H();
	}

	if(IsCurInRect(DlgboxWTop,DlgboxX,DlgboxY) || IsCurInRect(DlgboxWBottom,DlgboxX,DlgboxY+y_offs))
	{
		DlgboxVectX=CurX-DlgboxX;
		DlgboxVectY=CurY-DlgboxY;
		IfaceHold=IFACE_DIALOG_MAIN;
	}
}

void FOClient::DlgboxLMouseUp()
{
	switch(IfaceHold)
	{
	case IFACE_DIALOG_BTN:
		if(!IsCurInRect(DlgboxBButton,DlgboxX,DlgboxY+DlgboxWTop.H()+DlgboxSelectedButton*DlgboxWMiddle.H())) break;

		if(DlgboxSelectedButton==DlgboxButtonsCount-1)
		{
			if(DlgboxType>=DIALOGBOX_ENCOUNTER_ANY && DlgboxType<=DIALOGBOX_ENCOUNTER_TB) Net_SendRuleGlobal(GM_CMD_ANSWER,-1);
			//if(DlgboxType==DIALOGBOX_BARTER) Net_SendPlayersBarter(BARTER_END,PBarterPlayerId,true);
			DlgboxType=DIALOGBOX_NONE;
			ShowScreen(SCREEN_NONE);
			return;
		}

		if(DlgboxType==DIALOGBOX_FOLLOW)
		{
			Net_SendRuleGlobal(GM_CMD_FOLLOW,FollowRuleId);
		}
		else if(DlgboxType==DIALOGBOX_BARTER)
		{
			if(DlgboxSelectedButton==0) Net_SendPlayersBarter(BARTER_ACCEPTED,PBarterPlayerId,false);
			else Net_SendPlayersBarter(BARTER_ACCEPTED,PBarterPlayerId,true);
		}
		else if(DlgboxType==DIALOGBOX_ENCOUNTER_ANY)
		{
			if(DlgboxSelectedButton==0) Net_SendRuleGlobal(GM_CMD_ANSWER,COMBAT_MODE_REAL_TIME);
			else Net_SendRuleGlobal(GM_CMD_ANSWER,COMBAT_MODE_TURN_BASED);
		}
		else if(DlgboxType==DIALOGBOX_ENCOUNTER_RT)
		{
			Net_SendRuleGlobal(GM_CMD_ANSWER,COMBAT_MODE_REAL_TIME);
		}
		else if(DlgboxType==DIALOGBOX_ENCOUNTER_TB)
		{
			Net_SendRuleGlobal(GM_CMD_ANSWER,COMBAT_MODE_TURN_BASED);
		}
		else if(DlgboxType==DIALOGBOX_MANUAL)
		{
			if(ShowScreenType && ShowScreenNeedAnswer) Net_SendScreenAnswer(DlgboxSelectedButton,"");
		}
		DlgboxType=DIALOGBOX_NONE;
		ShowScreen(SCREEN_NONE);
		break;
	default:
		break;
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::DlgboxMouseMove()
{
	if(IfaceHold==IFACE_DIALOG_MAIN)
	{
		DlgboxX=CurX-DlgboxVectX;
		DlgboxY=CurY-DlgboxVectY;

		DWORD height=DlgboxWTop.H()+DlgboxButtonsCount*DlgboxWMiddle.H()+DlgboxWBottom.H();
		if(DlgboxX<0) DlgboxX=0;
		if(DlgboxX+DlgboxWTop[2]>MODE_WIDTH) DlgboxX=MODE_WIDTH-DlgboxWTop[2];
		if(DlgboxY<0) DlgboxY=0;
		if(DlgboxY+height>MODE_HEIGHT) DlgboxY=MODE_HEIGHT-height;
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::ElevatorDraw()
{
	SprMngr.DrawSprite(ElevatorMainPic,ElevatorMain[0]+ElevatorX,ElevatorMain[1]+ElevatorY);
	if(ElevatorExtPic) SprMngr.DrawSprite(ElevatorExtPic,ElevatorExt[0]+ElevatorX,ElevatorExt[1]+ElevatorY);
	if(ElevatorIndicatorAnim) SprMngr.DrawSprite(AnimGetCurSpr(ElevatorIndicatorAnim),ElevatorIndicator[0]+ElevatorX,ElevatorIndicator[1]+ElevatorY);

	if(IfaceHold==IFACE_ELEVATOR_BTN || ElevatorAnswerDone)
	{
		for(int i=0;i<ElevatorButtonsCount;i++)
		{
			INTRECT& r=ElevatorButtons[i];
			if(i==ElevatorSelectedButton) SprMngr.DrawSprite(ElevatorButtonPicDown,r[0]+ElevatorX,r[1]+ElevatorY);
		}
	}
}

void FOClient::ElevatorLMouseDown()
{
	IfaceHold=IFACE_NONE;

	if(!ElevatorAnswerDone && (ElevatorSelectedButton=ElevatorGetCurButton())!=-1)
	{
		IfaceHold=IFACE_ELEVATOR_BTN;
	}
	else if(IsCurInRect(ElevatorMain,ElevatorX,ElevatorY))
	{
		ElevatorVectX=CurX-ElevatorX;
		ElevatorVectY=CurY-ElevatorY;
		IfaceHold=IFACE_ELEVATOR_MAIN;
	}
}

void FOClient::ElevatorLMouseUp()
{
	switch(IfaceHold)
	{
	case IFACE_ELEVATOR_BTN:
		if(ElevatorAnswerDone) break;
		if(ElevatorSelectedButton!=ElevatorGetCurButton()) break;
		if(ElevatorStartLevel+ElevatorSelectedButton!=ElevatorCurrentLevel && ShowScreenType && ShowScreenNeedAnswer)
		{
			ElevatorAnswerDone=true;
			ElevatorSendAnswerTick=Timer::GameTick();
			int diff=abs((int)ElevatorCurrentLevel-int(ElevatorStartLevel+ElevatorSelectedButton));
			AnyFrames* anim=AnimGetFrames(ElevatorIndicatorAnim);
			if(anim) ElevatorSendAnswerTick+=anim->Ticks/anim->GetCnt()*(anim->GetCnt()*Procent(ElevatorLevelsCount-1,diff)/100);
			AnimRun(ElevatorIndicatorAnim,ElevatorStartLevel+ElevatorSelectedButton<ElevatorCurrentLevel?ANIMRUN_FROM_END:ANIMRUN_TO_END);
			return;
		}
		ShowScreen(SCREEN_NONE);
		break;
	default:
		break;
	}

	if(!ElevatorAnswerDone) ElevatorSelectedButton=-1;
	IfaceHold=IFACE_NONE;
}

void FOClient::ElevatorMouseMove()
{
	if(IfaceHold==IFACE_ELEVATOR_MAIN)
	{
		ElevatorX=CurX-ElevatorVectX;
		ElevatorY=CurY-ElevatorVectY;

		if(ElevatorX<0) ElevatorX=0;
		if(ElevatorX+ElevatorMain[2]>MODE_WIDTH) ElevatorX=MODE_WIDTH-ElevatorMain[2];
		if(ElevatorY<0) ElevatorY=0;
		if(ElevatorY+ElevatorMain[3]>MODE_HEIGHT) ElevatorY=MODE_HEIGHT-ElevatorMain[3];
	}
}

void FOClient::ElevatorGenerate(DWORD param)
{
	ElevatorMainPic=0;
	ElevatorExtPic=0;
	ElevatorIndicatorAnim=0;
	ElevatorButtonPicDown=0;
	ElevatorButtonsCount=0;
	ElevatorLevelsCount=0;
	ElevatorStartLevel=0;
	ElevatorCurrentLevel=0;

	if(!Script::PrepareContext(ClientFunctions.GetElevator,CALL_FUNC_STR,"Game")) return;
	asIScriptArray* arr=Script::CreateArray("int[]");
	if(!arr) return;
	Script::SetArgDword(param);
	Script::SetArgObject(arr);
	if(!Script::RunPrepared() || !Script::GetReturnedBool())
	{
		arr->Release();
		return;
	}

	DWORD added_buttons=0;
	for(int i=0,j=arr->GetElementCount();i<j;i++)
	{
		if(i>100) break;

		DWORD val=*(DWORD*)arr->GetElementPointer(i);
		switch(i)
		{
		case 0: ElevatorCurrentLevel=val; break;
		case 1: ElevatorStartLevel=val; break;
		case 2: ElevatorLevelsCount=val; break;
		case 3: ElevatorMainPic=val; break;
		case 4: ElevatorMain.R=val; break;
		case 5: ElevatorMain.B=val; break;
		case 6: ElevatorExtPic=val; break;
		case 7: ElevatorExt.L=val; break;
		case 8: ElevatorExt.T=val; break;
		case 9: ElevatorIndicatorAnim=val; break;
		case 10: ElevatorIndicator.L=val; break;
		case 11: ElevatorIndicator.T=val; break;
		case 12: ElevatorButtonPicDown=val; break;
		case 13: ElevatorButtonsCount=val; break;
		default: ElevatorButtons[(i-14)/4][(i-14)%4]=val; if((i-14)%4==3) added_buttons++; break;
		}
	}

	arr->Release();
	if(!ElevatorLevelsCount || ElevatorLevelsCount>MAX_DLGBOX_BUTTONS) return;
	if(ElevatorCurrentLevel<ElevatorStartLevel || ElevatorCurrentLevel>=ElevatorStartLevel+ElevatorLevelsCount) return;
	if(ElevatorButtonsCount>MAX_DLGBOX_BUTTONS || ElevatorButtonsCount!=added_buttons) return;
	if(ElevatorMainPic && !(ElevatorMainPic=ResMngr.GetIfaceSprId(ElevatorMainPic))) return;
	if(ElevatorExtPic && !(ElevatorExtPic=ResMngr.GetIfaceSprId(ElevatorExtPic))) return;
	if(ElevatorIndicatorAnim && !(ElevatorIndicatorAnim=AnimLoad(ElevatorIndicatorAnim,0,RES_IFACE))) return;
	if(ElevatorButtonPicDown && !(ElevatorButtonPicDown=ResMngr.GetIfaceSprId(ElevatorButtonPicDown))) return;

	AnimRun(ElevatorIndicatorAnim,ANIMRUN_SET_FRM(AnimGetSprCount(ElevatorIndicatorAnim)*Procent(ElevatorLevelsCount-1,ElevatorCurrentLevel-ElevatorStartLevel)/100)|ANIMRUN_STOP);
	ElevatorX=(MODE_WIDTH-ElevatorMain.W())/2;
	ElevatorY=(MODE_HEIGHT-ElevatorMain.H())/2;
	ElevatorAnswerDone=false;
	ElevatorSendAnswerTick=0;
	ShowScreen(SCREEN__ELEVATOR);
}

void FOClient::ElevatorProcess()
{
	if(ElevatorAnswerDone && Timer::GameTick()>=ElevatorSendAnswerTick)
	{
		AnimRun(ElevatorIndicatorAnim,ANIMRUN_SET_FRM(AnimGetSprCount(ElevatorIndicatorAnim)*Procent(ElevatorLevelsCount-1,ElevatorSelectedButton)/100)|ANIMRUN_STOP);
		if(ShowScreenNeedAnswer)
		{
			Net_SendScreenAnswer(ElevatorStartLevel+ElevatorSelectedButton,"");
			ShowScreenNeedAnswer=false;
			ElevatorSendAnswerTick+=1000;
			WaitPing();
		}
		else
		{
			ShowScreen(SCREEN_NONE);
		}
	}
}

int FOClient::ElevatorGetCurButton()
{
	for(int i=0;i<ElevatorButtonsCount;i++) if(IsCurInRectNoTransp(ElevatorButtonPicDown,ElevatorButtons[i],ElevatorX,ElevatorY)) return i;
	return -1;
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::SayDraw()
{
	SprMngr.DrawSprite(SayWMainPicNone,SayWMain[0]+SayX,SayWMain[1]+SayY);
	switch(IfaceHold)
	{
	case IFACE_SAY_OK: SprMngr.DrawSprite(SayBOkPicDown,SayBOk[0]+SayX,SayBOk[1]+SayY); break;
	case IFACE_SAY_CANCEL: SprMngr.DrawSprite(SayBCancelPicDown,SayBCancel[0]+SayX,SayBCancel[1]+SayY); break;
	default: break;
	}

	SprMngr.DrawStr(INTRECT(SayWMainText,SayX,SayY),SayTitle.c_str(),FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(SayBOkText,SayX,SayY),MsgGame->GetStr(STR_SAY_OK),FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(SayBCancelText,SayX,SayY),MsgGame->GetStr(STR_SAY_CANCEL),FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(SayWSay,SayX,SayY),SayText,FT_NOBREAK|FT_CENTERY);
}

void FOClient::SayLMouseDown()
{
	IfaceHold=IFACE_NONE;
	if(!IsCurInRect(SayWMain,SayX,SayY)) return;

	if(IsCurInRect(SayBOk,SayX,SayY)) IfaceHold=IFACE_SAY_OK;
	else if(IsCurInRect(SayBCancel,SayX,SayY)) IfaceHold=IFACE_SAY_CANCEL;

	if(IfaceHold!=IFACE_NONE) return;
	SayVectX=CurX-SayX;
	SayVectY=CurY-SayY;
	IfaceHold=IFACE_SAY_MAIN;
}

void FOClient::SayLMouseUp()
{
	switch(IfaceHold)
	{
	case IFACE_SAY_OK:
		if(!IsCurInRect(SayBOk,SayX,SayY)) break;
		if(!strlen(SayText)) break;
		if(ShowScreenType)
		{
			if(ShowScreenNeedAnswer) Net_SendScreenAnswer(0,SayText);
		}
		else
		{
			if(SayType==DIALOGSAY_TEXT) Net_SendSayNpc(DlgIsNpc,DlgNpcId,SayText);
			else if(SayType==DIALOGSAY_SAVE) SaveLoadSaveGame(SayText);
		}
		ShowScreen(SCREEN_NONE);
		WaitPing();
		return;
	case IFACE_SAY_CANCEL:
		if(!IsCurInRect(SayBCancel,SayX,SayY)) break;
		ShowScreen(SCREEN_NONE);
		return;
	default:
		break;
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::SayMouseMove()
{
	if(IfaceHold==IFACE_SAY_MAIN)
	{
		SayX=CurX-SayVectX;
		SayY=CurY-SayVectY;

		if(SayX<0) SayX=0;
		if(SayX+SayWMain[2]>MODE_WIDTH) SayX=MODE_WIDTH-SayWMain[2];
		if(SayY<0) SayY=0;
		if(SayY+SayWMain[3]>MODE_HEIGHT) SayY=MODE_HEIGHT-SayWMain[3];
	}
}

void FOClient::SayKeyDown(BYTE dik)
{
	if(dik==DIK_RETURN || dik==DIK_NUMPADENTER)
	{
		if(!strlen(SayText)) return;
		if(ShowScreenType)
		{
			if(ShowScreenNeedAnswer) Net_SendScreenAnswer(0,SayText);
		}
		else
		{
			if(SayType==DIALOGSAY_TEXT) Net_SendSayNpc(DlgIsNpc,DlgNpcId,SayText);
			else if(SayType==DIALOGSAY_SAVE) SaveLoadSaveGame(SayText);
		}
		ShowScreen(SCREEN_NONE);
		WaitPing();
		return;
	}

	if(SayType==DIALOGSAY_TEXT) Keyb::GetChar(dik,SayText,NULL,MAX_SAY_NPC_TEXT,SayOnlyNumbers?KIF_ONLY_NUMBERS:KIF_NO_SPEC_SYMBOLS);
	else if(SayType==DIALOGSAY_SAVE) Keyb::GetChar(dik,SayText,NULL,MAX_FOPATH,SayOnlyNumbers?KIF_ONLY_NUMBERS:KIF_FILE_NAME);
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::WaitDraw()
{
	SprMngr.DrawSpriteSize(WaitPic,0,0,MODE_WIDTH,MODE_HEIGHT,true,true);
	SprMngr.Flush();
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::SplitStart(Item* item, int to_cont)
{
	SplitItemId=item->GetId();
	SplitCont=to_cont;
	SplitValue=1;
	SplitMinValue=1;
	SplitMaxValue=item->GetCount();
	SplitValueKeyPressed=false;
	SplitItemPic=ResMngr.GetInvSprId(item->GetPicInv());
	SplitItemColor=item->GetInvColor();
	SplitParentScreen=GetActiveScreen();

	ShowScreen(SCREEN__SPLIT);
}

void FOClient::SplitClose(bool change)
{
	if(change && (SplitValue<SplitMinValue || SplitValue>SplitMaxValue)) return;

	if((SplitCont&0xFFFF)==0xFF) goto label_DropItems;
	switch(SplitParentScreen)
	{
	case SCREEN__INVENTORY:
		IfaceHold=IFACE_NONE;
		InvHoldId=0;
label_DropItems:
		if(!change) break;
		AddActionBack(CHOSEN_MOVE_ITEM,SplitItemId,SplitValue,SplitCont&0xFFFF,(SplitCont>>16)&1);
		break;
	case SCREEN__BARTER:
		IfaceHold=IFACE_NONE;
		BarterHoldId=0;
		if(!change) break;
		BarterTransfer(SplitItemId,SplitCont&0xFFFF,SplitValue);
		break;
	case SCREEN__PICKUP:
		IfaceHold=IFACE_NONE;
		PupHoldId=0;
		if(!change) break;
		SetAction(CHOSEN_MOVE_ITEM_CONT,SplitItemId,SplitCont&0xFFFF,SplitValue);
		break;
	default:
		break;
	}

	SplitParentScreen=SCREEN_NONE;
	ShowScreen(SCREEN_NONE,-2);
}

void FOClient::SplitDraw()
{
	SprMngr.DrawSprite(SplitMainPic,SplitWMain[0]+SplitX,SplitWMain[1]+SplitY);

	switch(IfaceHold)
	{
	case IFACE_SPLIT_UP: SprMngr.DrawSprite(SplitPBUpDn,SplitBUp[0]+SplitX,SplitBUp[1]+SplitY); break;
	case IFACE_SPLIT_DOWN: SprMngr.DrawSprite(SplitPBDnDn,SplitBDown[0]+SplitX,SplitBDown[1]+SplitY); break;
	case IFACE_SPLIT_ALL: SprMngr.DrawSprite(SplitPBAllDn,SplitBAll[0]+SplitX,SplitBAll[1]+SplitY); break;
	case IFACE_SPLIT_DONE: SprMngr.DrawSprite(SplitPBDoneDn,SplitBDone[0]+SplitX,SplitBDone[1]+SplitY); break;
	case IFACE_SPLIT_CANCEL: SprMngr.DrawSprite(SplitPBCancelDn,SplitBCancel[0]+SplitX,SplitBCancel[1]+SplitY); break;
	default: break;
	}

	if(SplitItemPic) SprMngr.DrawSpriteSize(SplitItemPic,SplitWItem[0]+SplitX,SplitWItem[1]+SplitY,SplitWItem[2]-SplitWItem[0],SplitWItem[3]-SplitWItem[1],false,true,SplitItemColor);

	SprMngr.DrawStr(INTRECT(SplitWTitle,SplitX,SplitY),MsgGame->GetStr(STR_SPLIT_TITLE),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(SplitBAll,SplitX,SplitY-(IfaceHold==IFACE_SPLIT_ALL?1:0)),MsgGame->GetStr(STR_SPLIT_ALL),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(SplitWValue,SplitX,SplitY),Str::Format("%05d",SplitValue),FT_NOBREAK,COLOR_IFACE,FONT_BIG_NUM);
}

void FOClient::SplitKeyDown(BYTE dik)
{
	int add=0;

	switch(dik)
	{
	case DIK_RETURN:
	case DIK_NUMPADENTER:
		SplitClose(true);
		return;
	case DIK_ESCAPE:
		SplitClose(false);
		return;
	case DIK_BACKSPACE:
		add=-1;
		break;
	case DIK_DELETE:
		add=-2;
		break;
	case DIK_0:
	case DIK_NUMPAD0: add=0; break;
	case DIK_1:
	case DIK_NUMPAD1: add=1; break;
	case DIK_2:
	case DIK_NUMPAD2: add=2; break;
	case DIK_3:
	case DIK_NUMPAD3: add=3; break;
	case DIK_4:
	case DIK_NUMPAD4: add=4; break;
	case DIK_5:
	case DIK_NUMPAD5: add=5; break;
	case DIK_6:
	case DIK_NUMPAD6: add=6; break;
	case DIK_7:
	case DIK_NUMPAD7: add=7; break;
	case DIK_8:
	case DIK_NUMPAD8: add=8; break;
	case DIK_9:
	case DIK_NUMPAD9: add=9; break;
	default:
		return;
	}

	if(add==-1) SplitValue/=10;
	else if(add==-2) SplitValue=0;
	else if(!SplitValueKeyPressed) SplitValue=add;
	else SplitValue=SplitValue*10+add;

	SplitValueKeyPressed=true;
	if(SplitValue>=MAX_SPLIT_VALUE) SplitValue=SplitValue%MAX_SPLIT_VALUE;
}

void FOClient::SplitLMouseDown()
{
	IfaceHold=IFACE_NONE;

	if(!IsCurInRect(SplitWMain,SplitX,SplitY)) return;

	if(IsCurInRect(SplitBUp,SplitX,SplitY))
	{
		Timer::StartAccelerator(ACCELERATE_SPLIT_UP);
		IfaceHold=IFACE_SPLIT_UP;
	}
	else if(IsCurInRect(SplitBDown,SplitX,SplitY))
	{
		Timer::StartAccelerator(ACCELERATE_SPLIT_DOWN);
		IfaceHold=IFACE_SPLIT_DOWN;
	}
	else if(IsCurInRect(SplitBAll,SplitX,SplitY)) IfaceHold=IFACE_SPLIT_ALL;
	else if(IsCurInRect(SplitBDone,SplitX,SplitY)) IfaceHold=IFACE_SPLIT_DONE;
	else if(IsCurInRect(SplitBCancel,SplitX,SplitY)) IfaceHold=IFACE_SPLIT_CANCEL;
	else
	{
		SplitVectX=CurX-SplitX;
		SplitVectY=CurY-SplitY;
		IfaceHold=IFACE_SPLIT_MAIN;
	}
}

void FOClient::SplitLMouseUp()
{
	switch(IfaceHold)
	{
	case IFACE_SPLIT_UP:
		if(!IsCurInRect(SplitBUp,SplitX,SplitY)) break;
		if(SplitValue<SplitMaxValue) SplitValue++;
		break;
	case IFACE_SPLIT_DOWN:
		if(!IsCurInRect(SplitBDown,SplitX,SplitY)) break;
		if(SplitValue>SplitMinValue) SplitValue--;
		break;
	case IFACE_SPLIT_ALL:
		if(!IsCurInRect(SplitBAll,SplitX,SplitY)) break;
		SplitValue=SplitMaxValue;
		if(SplitValue>=MAX_SPLIT_VALUE) SplitValue=MAX_SPLIT_VALUE-1;
		break;
	case IFACE_SPLIT_DONE:
		if(!IsCurInRect(SplitBDone,SplitX,SplitY)) break;
		SplitClose(true);
		break;
	case IFACE_SPLIT_CANCEL:
		if(!IsCurInRect(SplitBCancel,SplitX,SplitY)) break;
		SplitClose(false);
		break;
	default:
		break;
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::SplitMouseMove()
{
	if(IfaceHold==IFACE_SPLIT_MAIN)
	{
		SplitX=CurX-SplitVectX;
		SplitY=CurY-SplitVectY;

		if(SplitX<0) SplitX=0;
		if(SplitX+SplitWMain[2]>MODE_WIDTH) SplitX=MODE_WIDTH-SplitWMain[2];
		if(SplitY<0) SplitY=0;
		if(SplitY+SplitWMain[3]>MODE_HEIGHT) SplitY=MODE_HEIGHT-SplitWMain[3];
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::TimerStart(DWORD item_id, DWORD pic, DWORD pic_color)
{
	TimerItemId=item_id;
	TimerValue=TIMER_MIN_VALUE;
	TimerItemPic=pic;
	TimerItemColor=pic_color;

	ShowScreen(SCREEN__TIMER);
}

void FOClient::TimerClose(bool done)
{
	if(done)
	{
		if(ShowScreenType)
		{
			if(ShowScreenNeedAnswer) Net_SendScreenAnswer(TimerValue,"");
		}
		else AddActionBack(CHOSEN_USE_ITEM,TimerItemId,0,TARGET_SELF,0,USE_USE,TimerValue);
	}
	ShowScreen(SCREEN_NONE,-2);
}

void FOClient::TimerDraw()
{
	SprMngr.DrawSprite(TimerMainPic,TimerWMain[0]+TimerX,TimerWMain[1]+TimerY);

	switch(IfaceHold)
	{
	case IFACE_TIMER_UP: SprMngr.DrawSprite(TimerBUpPicDown,TimerBUp[0]+TimerX,TimerBUp[1]+TimerY); break;
	case IFACE_TIMER_DOWN: SprMngr.DrawSprite(TimerBDownPicDown,TimerBDown[0]+TimerX,TimerBDown[1]+TimerY); break;
	case IFACE_TIMER_DONE: SprMngr.DrawSprite(TimerBDonePicDown,TimerBDone[0]+TimerX,TimerBDone[1]+TimerY); break;
	case IFACE_TIMER_CANCEL: SprMngr.DrawSprite(TimerBCancelPicDown,TimerBCancel[0]+TimerX,TimerBCancel[1]+TimerY); break;
	default: break;
	}

	if(TimerItemPic) SprMngr.DrawSpriteSize(TimerItemPic,TimerWItem[0]+TimerX,TimerWItem[1]+TimerY,TimerWItem[2]-TimerWItem[0],TimerWItem[3]-TimerWItem[1],false,true,TimerItemColor);
	SprMngr.Flush();

	SprMngr.DrawStr(INTRECT(TimerWTitle,TimerX,TimerY),MsgGame->GetStr(STR_TIMER_TITLE),FT_NOBREAK|FT_CENTERX|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(TimerWValue,TimerX,TimerY),Str::Format("%d%c%02d",TimerValue/60,'9'+3,TimerValue%60),FT_NOBREAK,COLOR_IFACE,FONT_BIG_NUM);
}

void FOClient::TimerKeyDown(BYTE dik)
{
	switch(dik)
	{
	case DIK_RETURN:
	case DIK_NUMPADENTER:
		TimerClose(true);
		return;
	case DIK_ESCAPE:
		TimerClose(false);
		return;
	// Numetric +, -
	// Arrow Right, Left
	case DIK_EQUALS:
	case DIK_ADD:
	case DIK_RIGHT: TimerValue+=1; break;
	case DIK_MINUS:
	case DIK_SUBTRACT:
	case DIK_LEFT: TimerValue-=1; break;
	// PageUp, PageDown
	case DIK_PRIOR: TimerValue+=60; break;
	case DIK_NEXT: TimerValue-=60; break;
	// Arrow Up, Down
	case DIK_UP: TimerValue+=10; break;
	case DIK_DOWN: TimerValue-=10; break;
	// End, Home
	case DIK_END: TimerValue=TIMER_MAX_VALUE; break;
	case DIK_HOME: TimerValue=TIMER_MIN_VALUE; break;
	default:
		return;
	}

	if(TimerValue<TIMER_MIN_VALUE) TimerValue=TIMER_MIN_VALUE;
	if(TimerValue>TIMER_MAX_VALUE) TimerValue=TIMER_MAX_VALUE;
}

void FOClient::TimerLMouseDown()
{
	IfaceHold=IFACE_NONE;

	if(!IsCurInRect(TimerWMain,TimerX,TimerY)) return;

	if(IsCurInRect(TimerBUp,TimerX,TimerY))
	{
		Timer::StartAccelerator(ACCELERATE_TIMER_UP);
		IfaceHold=IFACE_TIMER_UP;
	}
	else if(IsCurInRect(TimerBDown,TimerX,TimerY))
	{
		Timer::StartAccelerator(ACCELERATE_TIMER_DOWN);
		IfaceHold=IFACE_TIMER_DOWN;
	}
	else if(IsCurInRect(TimerBDone,TimerX,TimerY)) IfaceHold=IFACE_TIMER_DONE;
	else if(IsCurInRect(TimerBCancel,TimerX,TimerY)) IfaceHold=IFACE_TIMER_CANCEL;
	else
	{
		TimerVectX=CurX-TimerX;
		TimerVectY=CurY-TimerY;
		IfaceHold=IFACE_TIMER_MAIN;
	}
}

void FOClient::TimerLMouseUp()
{
	switch(IfaceHold)
	{
	case IFACE_TIMER_UP:
		if(!IsCurInRect(TimerBUp,TimerX,TimerY)) break;
		if(TimerValue<TIMER_MAX_VALUE) TimerValue++;
		break;
	case IFACE_TIMER_DOWN:
		if(!IsCurInRect(TimerBDown,TimerX,TimerY)) break;
		if(TimerValue>TIMER_MIN_VALUE) TimerValue--;
		break;
	case IFACE_TIMER_DONE:
		if(!IsCurInRect(TimerBDone,TimerX,TimerY)) break;
		TimerClose(true);
		break;
	case IFACE_TIMER_CANCEL:
		if(!IsCurInRect(TimerBCancel,TimerX,TimerY)) break;
		TimerClose(false);
		break;
	default:
		break;
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::TimerMouseMove()
{
	if(IfaceHold==IFACE_TIMER_MAIN)
	{
		TimerX=CurX-TimerVectX;
		TimerY=CurY-TimerVectY;

		if(TimerX<0) TimerX=0;
		if(TimerX+TimerWMain[2]>MODE_WIDTH) TimerX=MODE_WIDTH-TimerWMain[2];
		if(TimerY<0) TimerY=0;
		if(TimerY+TimerWMain[3]>MODE_HEIGHT) TimerY=MODE_HEIGHT-TimerWMain[3];
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::FixGenerate(int fix_mode)
{
	if(!Chosen)
	{
		ShowScreen(SCREEN_NONE);
		return;
	}

	FixMode=fix_mode;
/************************************************************************/
/* LIST                                                                 */
/************************************************************************/
	if(fix_mode==FIX_MODE_LIST)
	{
		FixCraftLst.clear();

		CraftItemVec true_crafts;
		MrFixit.GetShowCrafts(Chosen,true_crafts);

		SCraftVec scraft_vec;
		DwordVec script_craft;
		for(int i=0,cur_height=0;i<true_crafts.size();++i)
		{
			CraftItem* craft=true_crafts[i];

			if(craft->Script.length())
			{
				script_craft.push_back(craft->Num);
				if(!FixShowCraft.count(craft->Num)) continue;
			}

			INTRECT pos(
				FixWWin[0],
				FixWWin[1]+cur_height,
				FixWWin[2],
				FixWWin[1]+cur_height+100); //Any

			int line_height=SprMngr.GetLinesHeight(FixWWin.W(),0,craft->Name.c_str());

			cur_height+=line_height;
			pos.B=FixWWin[1]+cur_height; //Bottom

			if(cur_height>FixWWin.H())
			{	
				FixCraftLst.push_back(scraft_vec);
				scraft_vec.clear();

				i--;
				cur_height=0;
				continue;

				pos.T=FixWWin[1];
				pos.B=FixWWin[1]+line_height;
				cur_height=line_height;
			}

			scraft_vec.push_back(SCraft(pos,craft->Name,craft->Num,MrFixit.IsTrueCraft(Chosen,craft->Num)));
		}

		if(!scraft_vec.empty()) FixCraftLst.push_back(scraft_vec);
		if(script_craft.size() && Timer::FastTick()>=FixNextShowCraftTick) Net_SendCraftAsk(script_craft);
	}
/************************************************************************/
/* FIXIT                                                                */
/************************************************************************/
	else if(fix_mode==FIX_MODE_FIXIT)
	{
		SCraftVec* cur_vec=GetCurSCrafts();
		if(!cur_vec || FixCurCraft>=cur_vec->size())
		{
			ShowScreen(SCREEN_NONE);
			return;
		}

		SCraft* scraft=&(*cur_vec)[FixCurCraft];
		CraftItem* craft=MrFixit.GetCraft(scraft->Num);

		if(!craft)
		{
			ShowScreen(SCREEN_NONE);
			return;
		}

//=================================================
#define FIX_PARSE_STR_LINE \
	do{\
	r.B+=SprMngr.GetLinesHeight(FixWWin.W(),0,str.c_str());\
	FixDrawComp.push_back(new FixDrawComponent(r,str));\
	r.T=r.B;\
	}while(0)

#define FIX_PARSE_ITEMS(items_vec,val_vec,or_vec) \
	str="";\
	for(int i=0,j=items_vec##.size();i<j;i++)\
	{\
		DWORD color=COLOR_TEXT;\
		if(Chosen->CountItemPid(items_vec##[i])<val_vec##[i]) color=COLOR_TEXT_DGREEN;\
		str+="|";\
		str+=Str::Format("%u",color);\
		str+=" ";\
\
		if(i>0)\
		{\
			if(or_vec##[i-1])\
				str+=MsgGame->GetStr(STR_OR);\
			else\
				str+=MsgGame->GetStr(STR_AND);\
		}\
\
		ProtoItem* proto=ItemMngr.GetProtoItem(items_vec##[i]);\
		if(!proto)\
			str+="???";\
		else\
			str+=MsgItem->GetStr(proto->GetInfo());\
\
		if(val_vec##[i]>1)\
		{\
			str+=" ";\
			str+=Str::DWtoA(val_vec##[i]);\
			str+=" ";\
			str+=MsgGame->GetStr(STR_FIX_PIECES);\
		}\
\
		str+="\n";\
\
	}\
	FIX_PARSE_STR_LINE;\
	\
	x=FixWWin[0]+FixWWin.W()/2-FIX_DRAW_PIC_WIDTH/2*items_vec##.size();\
	for(int i=0,j=items_vec##.size();i<j;i++,x+=FIX_DRAW_PIC_WIDTH)\
	{\
		ProtoItem* proto=ItemMngr.GetProtoItem(items_vec##[i]);\
		if(!proto) continue;\
\
		DWORD spr_id=ResMngr.GetInvSprId(proto->PicInvHash);\
		if(!spr_id) continue;\
\
		INTRECT r2=r;\
		r2.L=x;\
		r2.R=x+FIX_DRAW_PIC_WIDTH;\
		r2.B+=FIX_DRAW_PIC_HEIGHT;\
\
		FixDrawComp.push_back(new FixDrawComponent(r2,spr_id));\
	}\
	r.B+=FIX_DRAW_PIC_HEIGHT;\
	r.T=r.B;
//=================================================

		INTRECT r(FixWWin[0],FixWWin[1],FixWWin[2],FixWWin[1]);
		string str;
		int x;

		for(int i=0,j=FixDrawComp.size();i<j;i++)
			delete FixDrawComp[i];
		FixDrawComp.clear();

		// Out items
		ByteVec tmp_vec; //Temp vector
		for(int i=0;i<craft->OutItems.size();i++) tmp_vec.push_back(0); //Push AND
		FIX_PARSE_ITEMS(craft->OutItems,craft->OutItemsVal,tmp_vec);

		// About
		if(craft->Info.length())
		{
			str="\n";
			str+=craft->Info;
			str+="\n";
			FIX_PARSE_STR_LINE;
		}

		// Need params
		if(craft->NeedPNum.size())
		{
			str="\n";
			str+=MsgGame->GetStr(STR_FIX_PARAMS);
			str+="\n";
			for(int i=0,j=craft->NeedPNum.size();i<j;i++)
			{
				// Need
				str+=MsgGame->GetStr(STR_PARAM_NAME_(craft->NeedPNum[i]));
				str+=" ";
				str+=Str::ItoA(craft->NeedPVal[i]);

				if(craft->NeedPNum[i]>=SKILL_BEGIN && craft->NeedPNum[i]<=SKILL_END) str+="%";

				// You have
				str+=" (";
				str+=MsgGame->GetStr(STR_FIX_YOUHAVE);
				str+=Str::ItoA(Chosen->GetParam(craft->NeedPNum[i]));
				str+=")";

				// And, or
				if(i==j-1) break;

				str+="\n";
				if(craft->NeedPOr[i])
					str+=MsgGame->GetStr(STR_OR);
				else
					str+=MsgGame->GetStr(STR_AND);
			}
			FIX_PARSE_STR_LINE;
		}

		// Need tools
		if(craft->NeedTools.size())
		{
			str="\n";
			str+=MsgGame->GetStr(STR_FIX_TOOLS);
			FIX_PARSE_STR_LINE;
			FIX_PARSE_ITEMS(craft->NeedTools,craft->NeedToolsVal,craft->NeedToolsOr);
		}

		// Need items
		if(craft->NeedItems.size())
		{
			str="\n";
			str+=MsgGame->GetStr(STR_FIX_ITEMS);
			FIX_PARSE_STR_LINE;
			FIX_PARSE_ITEMS(craft->NeedItems,craft->NeedItemsVal,craft->NeedItemsOr);
		}
	}
/************************************************************************/
/* RESULT                                                               */
/************************************************************************/
	else if(fix_mode==FIX_MODE_RESULT)
	{
		FixResultStr="ERROR RESULT";

		switch(FixResult)
		{
		case CRAFT_RESULT_NONE: FixResultStr="..."; break;
		case CRAFT_RESULT_SUCC: FixResultStr=MsgGame->GetStr(STR_FIX_SUCCESS); break;
		case CRAFT_RESULT_FAIL: FixResultStr=MsgGame->GetStr(STR_FIX_FAIL); break;
		case CRAFT_RESULT_TIMEOUT: FixResultStr=MsgGame->GetStr(STR_FIX_TIMEOUT); break;
		default: break;
		}
	}
/************************************************************************/
/*                                                                      */
/************************************************************************/
}

int FOClient::GetMouseCraft()
{
	if(IfaceHold!=IFACE_NONE) return -1;
	if(!IsCurInRect(FixWWin,FixX,FixY)) return -1;

	SCraftVec* cur_vec=GetCurSCrafts();
	if(!cur_vec) return -1;

	for(int i=0,j=cur_vec->size();i<j;i++)
	{
		SCraft* scraft=&(*cur_vec)[i];
		if(IsCurInRect(scraft->Pos,FixX,FixY)) return i;
	}

	return -1;
}

FOClient::SCraftVec* FOClient::GetCurSCrafts()
{
	if(FixCraftLst.empty()) return NULL;
	if(FixScrollLst>=FixCraftLst.size()) FixScrollLst=FixCraftLst.size()-1;
	return &FixCraftLst[FixScrollLst];
}

void FOClient::FixDraw()
{
//GRAPH
	SprMngr.DrawSprite(FixMainPic,FixWMain[0]+FixX,FixWMain[1]+FixY);

	switch(IfaceHold)
	{
	case IFACE_FIX_SCRUP: SprMngr.DrawSprite(FixPBScrUpDn,FixBScrUp[0]+FixX,FixBScrUp[1]+FixY); break;
	case IFACE_FIX_SCRDN: SprMngr.DrawSprite(FixPBScrDnDn,FixBScrDn[0]+FixX,FixBScrDn[1]+FixY); break;
	case IFACE_FIX_DONE:  SprMngr.DrawSprite(FixPBDoneDn,FixBDone[0]+FixX,FixBDone[1]+FixY); break;
	case IFACE_FIX_FIX:   SprMngr.DrawSprite(FixPBFixDn,FixBFix[0]+FixX,FixBFix[1]+FixY); break;	
	default: break;
	}

	if(FixMode!=FIX_MODE_FIXIT) SprMngr.DrawSprite(FixPBFixDn,FixBFix[0]+FixX,FixBFix[1]+FixY);

	if(FixMode==FIX_MODE_FIXIT)
	{
		for(int i=0,j=FixDrawComp.size();i<j;i++)
		{
			FixDrawComponent* c=FixDrawComp[i];
			if(!c->IsText) SprMngr.DrawSpriteSize(c->SprId,c->Rect.L+FixX,c->Rect.T+FixY,c->Rect.W(),c->Rect.H(),false,true);
		}
	}

	SprMngr.Flush();

	switch(FixMode)
	{
/************************************************************************/
/* LIST                                                                 */
/************************************************************************/	
	case FIX_MODE_LIST:
		{
		//Crafts names
			SCraftVec* cur_vec=GetCurSCrafts();
			if(!cur_vec) break;

			for(int i=0,j=cur_vec->size();i<j;i++)
			{
				SCraft* scraft=&(*cur_vec)[i];
				DWORD col=COLOR_TEXT;
				if(!scraft->IsTrue) col=COLOR_TEXT_DRED;
				if(i==FixCurCraft)
				{
					if(IfaceHold==IFACE_FIX_CHOOSE) col=COLOR_TEXT_DBLUE;
					else col=COLOR_TEXT_DGREEN;
				}

				SprMngr.DrawStr(INTRECT(scraft->Pos,FixX,FixY),scraft->Name.c_str(),0,col);
			}

		//Number of page
			char str[64];
			sprintf(str,"%u/%u",FixScrollLst+1,FixCraftLst.size());
			SprMngr.DrawStr(INTRECT(FixWWin[2]-30+FixX,FixWWin[3]-15+FixY,FixWWin[2]+FixX,FixWWin[3]+FixY),str,FT_NOBREAK);
		}
		break;
/************************************************************************/
/* FIXIT                                                                */
/************************************************************************/
	case FIX_MODE_FIXIT:
		{
			for(int i=0,j=FixDrawComp.size();i<j;i++)
			{
				FixDrawComponent* c=FixDrawComp[i];
				if(c->IsText) SprMngr.DrawStr(INTRECT(c->Rect,FixX,FixY),c->Text.c_str(),FT_CENTERX|FT_COLORIZE);
			}
		}
		break;
/************************************************************************/
/* RESULT                                                                 */
/************************************************************************/
	case FIX_MODE_RESULT:
		{
			SprMngr.DrawStr(INTRECT(FixWWin,FixX,FixY),FixResultStr.c_str(),FT_CENTERX|FT_COLORIZE);
		}
		break;
/************************************************************************/
/*                                                                      */
/************************************************************************/
	default:
		break;
	}
}

void FOClient::FixLMouseDown()
{
	IfaceHold=IFACE_NONE;
	if(!IsCurInRect(FixWMain,FixX,FixY)) return;

	if(IsCurInRect(FixBScrUp,FixX,FixY)) IfaceHold=IFACE_FIX_SCRUP;
	else if(IsCurInRect(FixBScrDn,FixX,FixY)) IfaceHold=IFACE_FIX_SCRDN;
	else if(IsCurInRect(FixBDone,FixX,FixY)) IfaceHold=IFACE_FIX_DONE;
	else if(IsCurInRect(FixBFix,FixX,FixY)) IfaceHold=IFACE_FIX_FIX;
	else if(FixMode==FIX_MODE_LIST && IsCurInRect(FixWWin,FixX,FixY))
	{
		FixCurCraft=GetMouseCraft();
		if(FixCurCraft!=-1) IfaceHold=IFACE_FIX_CHOOSE;
	}

	if(IfaceHold!=IFACE_NONE) return;

	FixVectX=CurX-FixX;
	FixVectY=CurY-FixY;
	IfaceHold=IFACE_FIX_MAIN;
}

void FOClient::FixLMouseUp()
{
	switch(IfaceHold)
	{
	case IFACE_FIX_SCRUP:
		{
			if(!IsCurInRect(FixBScrUp,FixX,FixY)) break;

			if(FixMode==FIX_MODE_LIST)
			{
				if(FixScrollLst>0) FixScrollLst--;
			}
			else if(FixMode==FIX_MODE_FIXIT)
			{
				if(FixScrollFix>0) FixScrollFix--;
			}
		}
		break;
	case IFACE_FIX_SCRDN:
		{
			if(!IsCurInRect(FixBScrDn,FixX,FixY)) break;

			if(FixMode==FIX_MODE_LIST)
			{
				if(FixScrollLst<(int)FixCraftLst.size()-1) FixScrollLst++;
			}
			else if(FixMode==FIX_MODE_FIXIT)
			{
				if(FixScrollFix<(int)FixCraftFix.size()-1) FixScrollFix++;
			}
		}
		break;
	case IFACE_FIX_DONE:
		{
			if(!IsCurInRect(FixBDone,FixX,FixY)) break;

			if(FixMode!=FIX_MODE_LIST) FixGenerate(FIX_MODE_LIST);
			else ShowScreen(SCREEN_NONE);
		}
		break;
	case IFACE_FIX_FIX:
		{
			if(!IsCurInRect(FixBFix,FixX,FixY)) break;
			if(FixMode!=FIX_MODE_FIXIT) break;

			SCraftVec* cur_vec=GetCurSCrafts();
			if(!cur_vec) break;
			if(FixCurCraft>=cur_vec->size()) break;

			SCraft& craft=(*cur_vec)[FixCurCraft];
			DWORD num=craft.Num;

			FixResult=CRAFT_RESULT_NONE;
			FixGenerate(FIX_MODE_RESULT);
			Net_SendCraft(num);
			WaitPing();
		}
		break;
	case IFACE_FIX_CHOOSE:
		{
			if(FixMode!=FIX_MODE_LIST) break;
			if(FixCurCraft<0) break;
			IfaceHold=IFACE_NONE;
			if(FixCurCraft!=GetMouseCraft()) break;
			SCraftVec* cur_vec=GetCurSCrafts();
			if(!cur_vec) break;
			if(FixCurCraft>=cur_vec->size()) break;

			FixGenerate(FIX_MODE_FIXIT);
		}
		break;
	default:
		break;
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::FixMouseMove()
{
	if(FixMode==FIX_MODE_LIST && IfaceHold==IFACE_NONE) FixCurCraft=GetMouseCraft();

	if(IfaceHold==IFACE_FIX_MAIN)
	{
		FixX=CurX-FixVectX;
		FixY=CurY-FixVectY;

		if(FixX<0) FixX=0;
		if(FixX+FixWMain[2]>MODE_WIDTH) FixX=MODE_WIDTH-FixWMain[2];
		if(FixY<0) FixY=0;
		if(FixY+FixWMain[3]>MODE_HEIGHT) FixY=MODE_HEIGHT-FixWMain[3];
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

void FOClient::IboxDraw()
{
	SprMngr.DrawSprite(IboxWMainPicNone,IboxWMain[0]+IboxX,IboxWMain[1]+IboxY);

	switch(IfaceHold)
	{
	case IFACE_IBOX_DONE: SprMngr.DrawSprite(IboxBDonePicDown,IboxBDone[0]+IboxX,IboxBDone[1]+IboxY); break;
	case IFACE_IBOX_CANCEL: SprMngr.DrawSprite(IboxBCancelPicDown,IboxBCancel[0]+IboxX,IboxBCancel[1]+IboxY); break;
	default: break;
	}

	char* buf=(char*)Str::Format("%s",IboxTitle.c_str());
	if(IfaceHold==IFACE_IBOX_TITLE) Str::Insert(&buf[IboxTitleCur],Timer::FastTick()%798<399?"!":".");
	SprMngr.DrawStr(INTRECT(IboxWTitle,IboxX,IboxY),buf,FT_NOBREAK|FT_CENTERY);
	buf=(char*)Str::Format("%s",IboxText.c_str());
	if(IfaceHold==IFACE_IBOX_TEXT) Str::Insert(&buf[IboxTextCur],Timer::FastTick()%798<399?".":"!");
	SprMngr.DrawStr(INTRECT(IboxWText,IboxX,IboxY),buf,0);

	SprMngr.DrawStr(INTRECT(IboxBDoneText,IboxX,IboxY),MsgGame->GetStr(STR_INPUT_BOX_WRITE),FT_NOBREAK|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(IboxBCancelText,IboxX,IboxY),MsgGame->GetStr(STR_INPUT_BOX_BACK),FT_NOBREAK|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
}

void FOClient::IboxLMouseDown()
{
	IfaceHold=IFACE_NONE;
	if(!IsCurInRect(IboxWMain,IboxX,IboxY)) return;

	if(IsCurInRect(IboxWTitle,IboxX,IboxY)) IfaceHold=IFACE_IBOX_TITLE;
	else if(IsCurInRect(IboxWText,IboxX,IboxY)) IfaceHold=IFACE_IBOX_TEXT;
	else if(IsCurInRect(IboxBDone,IboxX,IboxY)) IfaceHold=IFACE_IBOX_DONE;
	else if(IsCurInRect(IboxBCancel,IboxX,IboxY)) IfaceHold=IFACE_IBOX_CANCEL;

	if(IfaceHold==IFACE_NONE)
	{
		IboxVectX=CurX-IboxX;
		IboxVectY=CurY-IboxY;
		IfaceHold=IFACE_IBOX_MAIN;
	}
}

void FOClient::IboxLMouseUp()
{
	switch(IfaceHold)
	{
	case IFACE_IBOX_DONE:
		{
			if(!IsCurInRect(IboxBDone,IboxX,IboxY)) break;
			AddActionBack(CHOSEN_WRITE_HOLO,IboxHolodiskId);
			ShowScreen(SCREEN_NONE);
		}
		break;
	case IFACE_IBOX_CANCEL:
		{
			if(!IsCurInRect(IboxBCancel,IboxX,IboxY)) break;
			ShowScreen(SCREEN_NONE);
		}
		break;
	case IFACE_IBOX_TITLE:
	case IFACE_IBOX_TEXT:
		return;
	default:
		break;
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::IboxKeyDown(BYTE dik)
{
	if(IfaceHold==IFACE_IBOX_TITLE) Keyb::GetChar(dik,IboxTitle,&IboxTitleCur,USER_HOLO_MAX_TITLE_LEN,KIF_NO_SPEC_SYMBOLS);
	else if(IfaceHold==IFACE_IBOX_TEXT) Keyb::GetChar(dik,IboxText,&IboxTextCur,USER_HOLO_MAX_LEN,0);
	else return;
	if(dik==DIK_PAUSE) return;
	IboxLastKey=dik;
	Timer::StartAccelerator(ACCELERATE_IBOX);
}

void FOClient::IboxKeyUp(BYTE dik)
{
	IboxLastKey=0;
}

void FOClient::IboxProcess()
{
	if(IboxLastKey && Timer::ProcessAccelerator(ACCELERATE_IBOX))
	{
		if(IfaceHold==IFACE_IBOX_TITLE) Keyb::GetChar(IboxLastKey,IboxTitle,&IboxTitleCur,USER_HOLO_MAX_TITLE_LEN,KIF_NO_SPEC_SYMBOLS);
		else if(IfaceHold==IFACE_IBOX_TEXT) Keyb::GetChar(IboxLastKey,IboxText,&IboxTextCur,USER_HOLO_MAX_LEN,0);
	}
}

void FOClient::IboxMouseMove()
{
	if(IfaceHold==IFACE_IBOX_MAIN)
	{
		IboxX=CurX-IboxVectX;
		IboxY=CurY-IboxVectY;

		if(IboxX<0) IboxX=0;
		if(IboxX+IboxWMain[2]>MODE_WIDTH) IboxX=MODE_WIDTH-IboxWMain[2];
		if(IboxY<0) IboxY=0;
		if(IboxY+IboxWMain[3]>MODE_HEIGHT) IboxY=MODE_HEIGHT-IboxWMain[3];
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================

#define SAVE_LOAD_LINES_PER_SLOT         (3)
void FOClient::SaveLoadCollect()
{
	// Collect singleplayer saves
	SaveLoadDataSlots.clear();

	// For each all saves in folder
	StrVec fnames;
	FileManager::GetFolderFileNames(PT_SAVE,"fo",fnames);
	PtrVec open_handles;
	for(size_t i=0;i<fnames.size();i++)
	{
		const string& fname=fnames[i];

		// Open file
		HANDLE hf=CreateFile(FileManager::GetFullPath(fname.c_str(),PT_DATA),FILE_GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
		if(hf==INVALID_HANDLE_VALUE) continue;
		open_handles.push_back(hf);

		// Get file information
		BY_HANDLE_FILE_INFORMATION finfo;
		if(!GetFileInformationByHandle(hf,&finfo)) continue;

		// Read save data, offsets see SaveGameInfoFile in Server.cpp
		DWORD dw;

		// Check singleplayer data
		DWORD sp;
		SetFilePointer(hf,4,NULL,FILE_BEGIN);
		if(!ReadFile(hf,&sp,sizeof(sp),&dw,NULL)) continue;
		if(sp!=1) continue; // Save not contain singleplayer data

		// Critter name
		char crname[MAX_NAME+1];
		SetFilePointer(hf,8,NULL,FILE_BEGIN);
		if(!ReadFile(hf,crname,sizeof(crname),&dw,NULL)) continue;

		// Map pid
		WORD map_pid;
		SetFilePointer(hf,8+31+68,NULL,FILE_BEGIN);
		if(!ReadFile(hf,&map_pid,sizeof(map_pid),&dw,NULL)) continue;

		// Calculate critter time events size
		DWORD te_size;
		SetFilePointer(hf,8+31+7404+6944,NULL,FILE_BEGIN);
		if(!ReadFile(hf,&te_size,sizeof(te_size),&dw,NULL)) continue;
		te_size=te_size*16+4;

		// Picture data
		DWORD pic_data_len;
		ByteVec pic_data;
		SetFilePointer(hf,8+31+7404+6944+te_size,NULL,FILE_BEGIN);
		if(!ReadFile(hf,&pic_data_len,sizeof(pic_data_len),&dw,NULL)) continue;
		if(pic_data_len)
		{
			pic_data.resize(pic_data_len);
			if(!ReadFile(hf,&pic_data[0],pic_data_len,&dw,NULL)) continue;
		}

		// Game time
		WORD year,month,day,hour,minute;
		SetFilePointer(hf,8+31+7404+6944+te_size+4+pic_data_len+2,NULL,FILE_BEGIN);
		if(!ReadFile(hf,&year,sizeof(year),&dw,NULL)) continue;
		if(!ReadFile(hf,&month,sizeof(month),&dw,NULL)) continue;
		if(!ReadFile(hf,&day,sizeof(day),&dw,NULL)) continue;
		if(!ReadFile(hf,&hour,sizeof(hour),&dw,NULL)) continue;
		if(!ReadFile(hf,&minute,sizeof(minute),&dw,NULL)) continue;

		// Extract name
		char name[MAX_FOPATH];
		FileManager::ExtractFileName(fname.c_str(),name);
		if(strlen(name)<4) continue;
		name[strlen(name)-3]=0;

		// Extract full path
		char fpath_[MAX_FOPATH];
		char fpath[MAX_FOPATH];
		StringCopy(fpath_,FileManager::GetDataPath(PT_DATA));
		StringAppend(fpath_,fname.c_str());
		if(!GetFullPathName(fpath_,MAX_FOPATH,fpath,NULL)) continue;

		// Convert time
		FILETIMELI writeft;
		SYSTEMTIME write_utc,write;
		writeft.ft=finfo.ftLastWriteTime;
		if(!FileTimeToSystemTime(&writeft.ft,&write_utc)) continue;
		if(!SystemTimeToTzSpecificLocalTime(NULL,&write_utc,&write)) continue;

		// Fill slot data
		SaveLoadDataSlot slot;
		slot.Name=name;
		slot.Info=Str::Format("%s\n%02d.%02d.%04d %02d:%02d:%02d\n",name,
			write.wDay,write.wMonth,write.wYear,write.wHour,write.wMinute,write.wSecond);
		slot.InfoExt=Str::Format("%s\n%02d %3s %04d %02d%02d\n%s",crname,
			day,MsgGame->GetStr(STR_MONTH(month)),year,hour,minute,MsgGM->GetStr(STR_MAP_NAME_(map_pid)));
		slot.FileName=fpath;
		slot.RealTime=writeft.ul.QuadPart;
		slot.PicData=pic_data;
		SaveLoadDataSlots.push_back(slot);
	}

	// Close opened file handles
	for(PtrVecIt it=open_handles.begin();it!=open_handles.end();++it) CloseHandle(*it);

	// Sort by creation time
	std::sort(SaveLoadDataSlots.begin(),SaveLoadDataSlots.end());

	// Set scroll data
	SaveLoadSlotScroll=0;
	SaveLoadSlotsMax=SaveLoadSlots.H()/SprMngr.GetLinesHeight(0,0,"")/SAVE_LOAD_LINES_PER_SLOT;

	// Show actual draft
	SaveLoadShowDraft();
}

void FOClient::SaveLoadSaveGame(const char* name)
{
	// Get name of new save
	char fpath_[MAX_FOPATH];
	char fpath[MAX_FOPATH];
	StringCopy(fpath_,FileManager::GetDataPath(PT_SAVE));
	StringAppend(fpath_,FileManager::GetPath(PT_SAVE));
	StringAppend(fpath_,name);
	StringAppend(fpath_,".fo");
	if(!GetFullPathName(fpath_,MAX_FOPATH,fpath,NULL)) return;

	// Delete old files
	if(SaveLoadFileName!="") DeleteFile(SaveLoadFileName.c_str());
	DeleteFile(fpath);

	// Get image data from surface
	ByteVec pic_data;
	if(SaveLoadDraftValid)
	{
		LPD3DXBUFFER img=NULL;
		if(SUCCEEDED(D3DXSaveSurfaceToFileInMemory(&img,D3DXIFF_BMP,SaveLoadDraft,NULL,NULL)))
		{
			pic_data.resize(img->GetBufferSize());
			memcpy(&pic_data[0],img->GetBufferPointer(),img->GetBufferSize());
		}
		SAFEREL(img);
	}

	// Send request
	Net_SendSaveLoad(true,fpath,&pic_data);

	// Close save/load screen
	ShowScreen(SCREEN_NONE);
}

void FOClient::SaveLoadFillDraft()
{
	// Fill game preview draft
	LPDIRECT3DDEVICE device=SprMngr.GetDevice();
	LPDIRECT3DSURFACE rt=NULL;
	SaveLoadDraftValid=(SUCCEEDED(device->GetRenderTarget(0,&rt)) &&
		SUCCEEDED(device->StretchRect(rt,NULL,SaveLoadDraft,NULL,D3DTEXF_LINEAR)));
	SAFEREL(rt);
	SaveLoadProcessDraft=false;
}

void FOClient::SaveLoadShowDraft()
{
	SaveLoadDraftValid=false;
	if(SaveLoadSlotIndex>=0 && SaveLoadSlotIndex<(int)SaveLoadDataSlots.size())
	{
		// Get surface from image data
		SaveLoadDataSlot& slot=SaveLoadDataSlots[SaveLoadSlotIndex];
		SaveLoadDraftValid=(slot.PicData.size() && SUCCEEDED(D3DXLoadSurfaceFromFileInMemory(SaveLoadDraft,NULL,NULL,
			&slot.PicData[0],slot.PicData.size(),NULL,D3DX_FILTER_LINEAR,0,NULL)));
	}
	else if(SaveLoadSave && SaveLoadSlotIndex==(int)SaveLoadDataSlots.size())
	{
		// Process SaveLoadFillDraft
		SaveLoadProcessDraft=true;
	}
}

void FOClient::SaveLoadProcessDone()
{
	if(SaveLoadSave)
	{
		SaveLoadProcessDraft=true;

		ShowScreen(SCREEN__SAY);
		SayType=DIALOGSAY_SAVE;
		SayTitle=MsgGame->GetStr(STR_SAVE_LOAD_TYPE_RECORD_NAME);
		SayText[0]=0;
		SayOnlyNumbers=false;

		SaveLoadFileName="";
		if(SaveLoadSlotIndex>=0 && SaveLoadSlotIndex<(int)SaveLoadDataSlots.size())
		{
			StringCopy(SayText,SaveLoadDataSlots[SaveLoadSlotIndex].Name.c_str());
			SaveLoadFileName=SaveLoadDataSlots[SaveLoadSlotIndex].FileName;
		}
	}
	else if(SaveLoadSlotIndex>=0 && SaveLoadSlotIndex<(int)SaveLoadDataSlots.size())
	{
		if(NetState==STATE_DISCONNECT)
		{
			SaveLoadFileName=SaveLoadDataSlots[SaveLoadSlotIndex].FileName;
			NetState=STATE_INIT_NET;
			InitNetReason=INIT_NET_REASON_LOAD;
		}
		else
		{
			Net_SendSaveLoad(false,SaveLoadDataSlots[SaveLoadSlotIndex].FileName.c_str(),NULL);
			ShowScreen(SCREEN_NONE);
			WaitPing();
		}
	}
}

void FOClient::SaveLoadDraw()
{
	int ox=(SaveLoadLoginScreen?SaveLoadCX:SaveLoadX);
	int oy=(SaveLoadLoginScreen?SaveLoadCY:SaveLoadY);

	if(SaveLoadLoginScreen) SprMngr.ClearCurRenderTarget(0xFF000000); // Black background
	SprMngr.DrawSprite(SaveLoadMainPic,SaveLoadMain[0]+ox,SaveLoadMain[1]+oy);

	if(IfaceHold==IFACE_SAVELOAD_SCR_UP) SprMngr.DrawSprite(SaveLoadScrUpPicDown,SaveLoadScrUp[0]+ox,SaveLoadScrUp[1]+oy);
	else if(IfaceHold==IFACE_SAVELOAD_SCR_DN) SprMngr.DrawSprite(SaveLoadScrDownPicDown,SaveLoadScrDown[0]+ox,SaveLoadScrDown[1]+oy);
	else if(IfaceHold==IFACE_SAVELOAD_DONE) SprMngr.DrawSprite(SaveLoadDonePicDown,SaveLoadDone[0]+ox,SaveLoadDone[1]+oy);
	else if(IfaceHold==IFACE_SAVELOAD_BACK) SprMngr.DrawSprite(SaveLoadBackPicDown,SaveLoadBack[0]+ox,SaveLoadBack[1]+oy);

	SprMngr.DrawStr(INTRECT(SaveLoadText,ox,oy),MsgGame->GetStr(SaveLoadSave?STR_SAVE_LOAD_SAVE:STR_SAVE_LOAD_LOAD),FT_NOBREAK|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(SaveLoadDoneText,ox,oy),MsgGame->GetStr(STR_SAVE_LOAD_DONE),FT_NOBREAK|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);
	SprMngr.DrawStr(INTRECT(SaveLoadBackText,ox,oy),MsgGame->GetStr(STR_SAVE_LOAD_BACK),FT_NOBREAK|FT_CENTERY,COLOR_TEXT_SAND,FONT_FAT);

	// Slots
	int line_height=SprMngr.GetLinesHeight(0,0,"");
	int cur=0;
	for(int i=SaveLoadSlotScroll,j=(int)SaveLoadDataSlots.size();i<j;i++)
	{
		SaveLoadDataSlot& slot=SaveLoadDataSlots[i];
		SprMngr.DrawStr(INTRECT(SaveLoadSlots,ox,oy+cur*line_height*SAVE_LOAD_LINES_PER_SLOT),
			slot.Info.c_str(),FT_NOBREAK_LINE,i==SaveLoadSlotIndex?COLOR_TEXT_DDGREEN:COLOR_TEXT);
		if(++cur>=SaveLoadSlotsMax) break;
	}
	if(SaveLoadSave && SaveLoadSlotScroll<=(int)SaveLoadDataSlots.size() && cur<=SaveLoadSlotsMax-1)
	{
		SprMngr.DrawStr(INTRECT(SaveLoadSlots,ox,oy+cur*line_height*SAVE_LOAD_LINES_PER_SLOT),
			MsgGame->GetStr(STR_SAVE_LOAD_NEW_RECORD),FT_NOBREAK_LINE,SaveLoadSlotIndex==SaveLoadDataSlots.size()?COLOR_TEXT_DDGREEN:COLOR_TEXT);
	}

	// Selected slot ext info
	if(SaveLoadSlotIndex>=0 && SaveLoadSlotIndex<(int)SaveLoadDataSlots.size())
	{
		SaveLoadDataSlot& slot=SaveLoadDataSlots[SaveLoadSlotIndex];
		SprMngr.DrawStr(INTRECT(SaveLoadInfo,ox,oy),slot.InfoExt.c_str(),FT_CENTERY|FT_NOBREAK_LINE);
	}
	if(SaveLoadSave && SaveLoadSlotIndex==(int)SaveLoadDataSlots.size())
	{
		SprMngr.DrawStr(INTRECT(SaveLoadInfo,ox,oy),MsgGame->GetStr(STR_SAVE_LOAD_NEW_RECORD),FT_CENTERY|FT_NOBREAK_LINE);
	}

	// Draw preview draft
	if(SaveLoadDraftValid)
	{
		RECT dst={SaveLoadPic.L+ox,SaveLoadPic.T+oy,SaveLoadPic.R+ox,SaveLoadPic.B+oy};
		SprMngr.DrawSurface(SaveLoadDraft,dst);
	}
}

void FOClient::SaveLoadLMouseDown()
{
	IfaceHold=IFACE_NONE;
	int ox=(SaveLoadLoginScreen?SaveLoadCX:SaveLoadX);
	int oy=(SaveLoadLoginScreen?SaveLoadCY:SaveLoadY);

	if(!IsCurInRectNoTransp(SaveLoadMainPic,SaveLoadMain,ox,oy)) return;

	if(IsCurInRect(SaveLoadScrUp,ox,oy))
	{
		IfaceHold=IFACE_SAVELOAD_SCR_UP;
		Timer::StartAccelerator(ACCELERATE_SAVE_LOAD_SCR_UP);
	}
	else if(IsCurInRect(SaveLoadScrDown,ox,oy))
	{
		IfaceHold=IFACE_SAVELOAD_SCR_DN;
		Timer::StartAccelerator(ACCELERATE_SAVE_LOAD_SCR_DN);
	}
	else if(IsCurInRect(SaveLoadDone,ox,oy)) IfaceHold=IFACE_SAVELOAD_DONE;
	else if(IsCurInRect(SaveLoadBack,ox,oy)) IfaceHold=IFACE_SAVELOAD_BACK;
	else if(IsCurInRect(SaveLoadSlots,ox,oy))
	{
		int line_height=SprMngr.GetLinesHeight(0,0,"");
		int line=(CurY-SaveLoadSlots.T-oy)/line_height;
		int index=line/SAVE_LOAD_LINES_PER_SLOT;
		if(index<SaveLoadSlotsMax)
		{
			SaveLoadSlotIndex=index+SaveLoadSlotScroll;
			SaveLoadShowDraft();

			if(SaveLoadSlotIndex==SaveLoadClickSlotIndex && Timer::FastTick()-SaveLoadClickSlotTick<=GetDoubleClickTime())
			{
				SaveLoadProcessDone();
			}
			else
			{
				SaveLoadClickSlotIndex=SaveLoadSlotIndex;
				SaveLoadClickSlotTick=Timer::FastTick();
			}
		}
	}
	else if(!SaveLoadLoginScreen)
	{
		SaveLoadVectX=CurX-SaveLoadX;
		SaveLoadVectY=CurY-SaveLoadY;
		IfaceHold=IFACE_SAVELOAD_MAIN;
	}
}

void FOClient::SaveLoadLMouseUp()
{
	int ox=(SaveLoadLoginScreen?SaveLoadCX:SaveLoadX);
	int oy=(SaveLoadLoginScreen?SaveLoadCY:SaveLoadY);

	if(IfaceHold==IFACE_SAVELOAD_SCR_UP && IsCurInRect(SaveLoadScrUp,ox,oy))
	{
		if(SaveLoadSlotScroll>0) SaveLoadSlotScroll--;
	}
	else if(IfaceHold==IFACE_SAVELOAD_SCR_DN && IsCurInRect(SaveLoadScrDown,ox,oy))
	{
		int max=(int)SaveLoadDataSlots.size()-SaveLoadSlotsMax+(SaveLoadSave?1:0);
		if(SaveLoadSlotScroll<max) SaveLoadSlotScroll++;
	}
	else if(IfaceHold==IFACE_SAVELOAD_DONE && IsCurInRect(SaveLoadDone,ox,oy))
	{
		SaveLoadProcessDone();
	}
	else if(IfaceHold==IFACE_SAVELOAD_BACK && IsCurInRect(SaveLoadBack,ox,oy))
	{
		ShowScreen(SCREEN_NONE);
	}

	IfaceHold=IFACE_NONE;
}

void FOClient::SaveLoadMouseMove()
{
	if(IfaceHold==IFACE_SAVELOAD_MAIN)
	{
		SaveLoadX=CurX-SaveLoadVectX;
		SaveLoadY=CurY-SaveLoadVectY;

		if(SaveLoadX<0) SaveLoadX=0;
		if(SaveLoadX+SaveLoadMain[2]>MODE_WIDTH) SaveLoadX=MODE_WIDTH-SaveLoadMain[2];
		if(SaveLoadY<0) SaveLoadY=0;
		if(SaveLoadY+SaveLoadMain[3]>MODE_HEIGHT) SaveLoadY=MODE_HEIGHT-SaveLoadMain[3];
	}
}

//==============================================================================================================================
//******************************************************************************************************************************
//==============================================================================================================================