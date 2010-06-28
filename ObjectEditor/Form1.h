/********************************************************************
	created:	16:05:2007   00:00
	author:		Anton Tvetinsky aka Cvet
	purpose:	
*********************************************************************/

#pragma once

#pragma warning (disable : 4996)

#pragma region Header

#include "vcclr.h"
#include <Defines.h>
#include <Crypt.h>
#include <Log.h>
#include <Text.h>
#include <Item.h>
#include <ItemManager.h>
#include <Names.h>
#include <Version.h>

FOMsg MsgObj;

namespace ObjectEditor {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;
	using namespace System::IO;

	const char* ToAnsi(String^ str)
	{
		static char buf[4096];
		pin_ptr<const wchar_t> wch=PtrToStringChars(str);
		size_t origsize=wcslen(wch)+1;
		size_t convertedChars=0;
		wcstombs(buf,wch,sizeof(buf));
		return buf;
	}

	String^ ToClrString(const char* str)
	{
		String^ res=String(str).ToString();
		return res;
	}

	String^ ToClrString(string& str)
	{
		return ToClrString(str.c_str());
	}

	String^ ByteToString(BYTE b)
	{
		String^ res="";
		char buf[64];
		if(!b) return res;
		sprintf(buf,"%c",b);
		res=String(buf).ToString();
		return res;
	}

#pragma endregion

#pragma region Components
	/// <summary>
	/// Summary for Form1
	///
	/// WARNING: If you change the name of this class, you will need to change the
	///          'Resource File Name' property for the managed resource compiler tool
	///          associated with all .resx files this class depends on.  Otherwise,
	///          the designers will not be able to interact properly with localized
	///          resources associated with this form.
	/// </summary>
	public ref class Form1 : public System::Windows::Forms::Form
	{
	public:
		Form1(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here
			//
		}

		void Log(String^ str)
		{
			WriteLog("%s\n",ToAnsi(str));

		//	String^ nstr=(String^)str;

			cmbLog->Items->Add(str);
			cmbLog->Text=str;
		};

	private: System::Windows::Forms::Label^  label9;
	private: System::Windows::Forms::NumericUpDown^  numAmmoDmgDiv;
	private: System::Windows::Forms::NumericUpDown^  numAmmoDmgMult;
	private: System::Windows::Forms::NumericUpDown^  numAmmoDRMod;
	private: System::Windows::Forms::NumericUpDown^  numAmmoACMod;
	private: System::Windows::Forms::Label^  label93;
	private: System::Windows::Forms::Label^  label39;
	private: System::Windows::Forms::Label^  label21;
	private: System::Windows::Forms::Label^  label20;
	private: System::Windows::Forms::NumericUpDown^  numMisc2SV1;
	private: System::Windows::Forms::Label^  label96;
	private: System::Windows::Forms::Label^  label95;
	private: System::Windows::Forms::Label^  label94;
	private: System::Windows::Forms::CheckBox^  cbContChangeble;
	private: System::Windows::Forms::Label^  label97;
	private: System::Windows::Forms::CheckBox^  cbContMagicHandsGrnd;
	private: System::Windows::Forms::CheckBox^  cbContCannotPickUp;
	private: System::Windows::Forms::CheckBox^  cbDoorWalkThru;


	private: System::Windows::Forms::GroupBox^  groupBox3;
	private: System::Windows::Forms::RadioButton^  rbGridLadderTop;
	private: System::Windows::Forms::RadioButton^  rbGridLadderBottom;
	private: System::Windows::Forms::RadioButton^  rbGridStairs;
	private: System::Windows::Forms::RadioButton^  rbGridExitGrid;
	private: System::Windows::Forms::RadioButton^  rbGridElevator;
	private: System::Windows::Forms::RadioButton^  rbGridElevatr;
	private: System::Windows::Forms::TabPage^  tWall;


	private: System::Windows::Forms::CheckBox^  cbShowWall;
	private: System::Windows::Forms::CheckBox^  cbWeapNoWear;
	private: System::Windows::Forms::TabPage^  cbWeapMain;

	private: System::Windows::Forms::NumericUpDown^  numeapDefAmmo;
	private: System::Windows::Forms::Label^  label99;

	private: System::Windows::Forms::NumericUpDown^  numWeapPerk;
	private: System::Windows::Forms::Label^  label105;
	private: System::Windows::Forms::GroupBox^  groupBox4;
	private: System::Windows::Forms::TextBox^  txtSoundId;

private: System::Windows::Forms::TextBox^  txtWeapPASoundId;
private: System::Windows::Forms::Label^  label104;
private: System::Windows::Forms::TextBox^  txtWeapSASoundId;
private: System::Windows::Forms::Label^  label106;
private: System::Windows::Forms::TextBox^  txtWeapTASoundId;
private: System::Windows::Forms::Label^  label107;
	private: System::Windows::Forms::NumericUpDown^  numWeapMinAg;

private: System::Windows::Forms::Label^  label108;
private: System::Windows::Forms::NumericUpDown^  numWeapDefAmmo;
private: System::Windows::Forms::NumericUpDown^  numWeapMinSt;
private: System::Windows::Forms::Label^  label102;
private: System::Windows::Forms::Button^  bLoad1;
private: System::Windows::Forms::Button^  bSave1;
private: System::Windows::Forms::GroupBox^  groupBox5;
private: System::Windows::Forms::CheckBox^  cbMisc2IsCar;
private: System::Windows::Forms::TextBox^  txtMisc2CarBlocks;
private: System::Windows::Forms::Label^  label109;
private: System::Windows::Forms::NumericUpDown^  numMisc2SV3;
private: System::Windows::Forms::NumericUpDown^  numMisc2SV2;
	private: System::Windows::Forms::Label^  label110;
	private: System::Windows::Forms::TextBox^  txtMisc2CarBag2;
	private: System::Windows::Forms::Label^  label112;
	private: System::Windows::Forms::TextBox^  txtMisc2CarBag1;
	private: System::Windows::Forms::Label^  label111;
private: System::Windows::Forms::CheckBox^  cbContIsNoOpen;
private: System::Windows::Forms::CheckBox^  cbDoorIsNoOpen;
private: System::Windows::Forms::CheckBox^  cbWeapIsNeedAct;
private: System::Windows::Forms::NumericUpDown^  nWeapTAdmgmax;
private: System::Windows::Forms::NumericUpDown^  nWeapTAround;
private: System::Windows::Forms::RadioButton^  rbMisc2CarWater;
private: System::Windows::Forms::RadioButton^  rbMisc2CarGround;
private: System::Windows::Forms::RadioButton^  rbMisc2CarFly;
private: System::Windows::Forms::GroupBox^  groupBox7;
private: System::Windows::Forms::CheckBox^  cbHasTimer;
private: System::Windows::Forms::CheckBox^  cbBadItem;
private: System::Windows::Forms::CheckBox^  checkBox1;
private: System::Windows::Forms::NumericUpDown^  numDrawPosOffsY;
private: System::Windows::Forms::NumericUpDown^  numMisc2CarRunToBreak;
private: System::Windows::Forms::NumericUpDown^  numMisc2CarFuelTank;
private: System::Windows::Forms::NumericUpDown^  numMisc2CarCritCapacity;
private: System::Windows::Forms::NumericUpDown^  numMisc2CarWearConsumption;
private: System::Windows::Forms::NumericUpDown^  numMisc2CarNegotiability;
private: System::Windows::Forms::NumericUpDown^  numMisc2CarSpeed;
private: System::Windows::Forms::NumericUpDown^  numMisc2CarFuelConsumption;
private: System::Windows::Forms::Label^  label117;
private: System::Windows::Forms::Label^  label116;
private: System::Windows::Forms::Label^  label115;
private: System::Windows::Forms::Label^  label114;
private: System::Windows::Forms::Label^  label101;
private: System::Windows::Forms::Label^  label84;
private: System::Windows::Forms::Label^  label11;
private: System::Windows::Forms::CheckBox^  cbAlwaysView;
private: System::Windows::Forms::Label^  label103;
private: System::Windows::Forms::CheckBox^  cbGekk;
private: System::Windows::Forms::CheckBox^  cbWeapUnarmed;
private: System::Windows::Forms::Label^  label119;
private: System::Windows::Forms::Label^  label118;
private: System::Windows::Forms::NumericUpDown^  numWeapMinLevel;
private: System::Windows::Forms::NumericUpDown^  numWeapUnarmedTree;
private: System::Windows::Forms::Label^  label120;
private: System::Windows::Forms::NumericUpDown^  numWeapMinUnarmed;
private: System::Windows::Forms::NumericUpDown^  numWeapUnarmedPriory;
private: System::Windows::Forms::Label^  label121;
private: System::Windows::Forms::NumericUpDown^  numWeapUnarmedCritBonus;
private: System::Windows::Forms::CheckBox^  cbWeapArmorPiercing;
private: System::Windows::Forms::Label^  label10;
private: System::Windows::Forms::NumericUpDown^  numMisc2CarEntire;
private: System::Windows::Forms::TextBox^  textBox2;
private: System::Windows::Forms::TextBox^  textBox1;
private: System::Windows::Forms::Button^  bSaveList;
private: System::Windows::Forms::Button^  bLoad;
private: System::Windows::Forms::TabPage^  tDrug;
private: System::Windows::Forms::ComboBox^  cmbLog;
private: System::Windows::Forms::Label^  label32;
private: System::Windows::Forms::GroupBox^  groupBox9;
private: System::Windows::Forms::NumericUpDown^  numSlot;
private: System::Windows::Forms::TextBox^  txtPicInv;
private: System::Windows::Forms::TextBox^  txtPicMap;
private: System::Windows::Forms::TextBox^  txtWeapPApic;
private: System::Windows::Forms::TextBox^  textBox3;
private: System::Windows::Forms::TextBox^  txtWeapTApic;
private: System::Windows::Forms::NumericUpDown^  numRed;
private: System::Windows::Forms::Label^  label35;
private: System::Windows::Forms::Label^  label34;
private: System::Windows::Forms::Label^  label33;
private: System::Windows::Forms::NumericUpDown^  numBlue;
private: System::Windows::Forms::NumericUpDown^  numGreen;
private: System::Windows::Forms::TabPage^  tabPage3;
private: System::Windows::Forms::GroupBox^  groupBox8;
private: System::Windows::Forms::Label^  label3;
private: System::Windows::Forms::Label^  label2;
private: System::Windows::Forms::TextBox^  txtScriptFunc;
private: System::Windows::Forms::TextBox^  txtScriptModule;
private: System::Windows::Forms::GroupBox^  groupBox6;
private: System::Windows::Forms::Label^  label74;
private: System::Windows::Forms::Label^  label59;
private: System::Windows::Forms::Label^  label45;
private: System::Windows::Forms::NumericUpDown^  numHideAnim1;
private: System::Windows::Forms::NumericUpDown^  numHideAnim0;
private: System::Windows::Forms::NumericUpDown^  numShowAnim1;
private: System::Windows::Forms::NumericUpDown^  numShowAnim0;
private: System::Windows::Forms::NumericUpDown^  numStayAnim1;
private: System::Windows::Forms::NumericUpDown^  numStayAnim0;
private: System::Windows::Forms::CheckBox^  cbShowAnimExt;
private: System::Windows::Forms::NumericUpDown^  numAnimWaitRndMax;
private: System::Windows::Forms::NumericUpDown^  numAnimWaitRndMin;
private: System::Windows::Forms::Label^  label113;
private: System::Windows::Forms::NumericUpDown^  numAnimWaitBase;
private: System::Windows::Forms::CheckBox^  cbShowAnim;
private: System::Windows::Forms::CheckBox^  cbNoHighlight;
private: System::Windows::Forms::CheckBox^  cbCached;
private: System::Windows::Forms::CheckBox^  cbGag;
private: System::Windows::Forms::CheckBox^  cbNoSteal;
private: System::Windows::Forms::CheckBox^  cbNoLoot;
private: System::Windows::Forms::CheckBox^  cbTrap;
private: System::Windows::Forms::CheckBox^  cbNoLightInfluence;
private: System::Windows::Forms::GroupBox^  groupBox10;
private: System::Windows::Forms::CheckBox^  cbDisableEgg;
private: System::Windows::Forms::GroupBox^  groupBox11;
private: System::Windows::Forms::CheckBox^  cbLight;
private: System::Windows::Forms::CheckBox^  checkBox2;
private: System::Windows::Forms::Label^  label36;
private: System::Windows::Forms::RadioButton^  rbLightSouth;
private: System::Windows::Forms::RadioButton^  rbLightEast;
private: System::Windows::Forms::RadioButton^  rbLightNorth;
private: System::Windows::Forms::RadioButton^  rbLightWest;
private: System::Windows::Forms::RadioButton^  rbLightWestEast;
private: System::Windows::Forms::RadioButton^  rbLightNorthSouth;
private: System::Windows::Forms::CheckBox^  cbColorizing;
private: System::Windows::Forms::CheckBox^  cbLightInverse;
private: System::Windows::Forms::CheckBox^  cbLightGlobal;
private: System::Windows::Forms::CheckBox^  cbDisableDir0;
private: System::Windows::Forms::CheckBox^  cbDisableDir1;
private: System::Windows::Forms::CheckBox^  cbDisableDir2;
private: System::Windows::Forms::CheckBox^  cbDisableDir3;
private: System::Windows::Forms::CheckBox^  cbDisableDir5;
private: System::Windows::Forms::CheckBox^  cbDisableDir4;

		 RichTextBox^ RtbIfaceLst;

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~Form1()
		{
			if (components)
			{
				delete components;
			}
		}
	private: System::Windows::Forms::CheckBox^ cbFlat;
	private: System::Windows::Forms::TabControl^  tcType;
	private: System::Windows::Forms::TabPage^  tArmor;
	private: System::Windows::Forms::TabPage^  tContainer;

	private: System::Windows::Forms::TabPage^  tWeapon;
	private: System::Windows::Forms::TabPage^  tAmmo;
	private: System::Windows::Forms::TabPage^  tMisc;
	private: System::Windows::Forms::TabPage^  tKey;
	private: System::Windows::Forms::TabPage^  tDoor;
	private: System::Windows::Forms::TabPage^  tGrid;
	private: System::Windows::Forms::RichTextBox^  txtInfo;
	private: System::Windows::Forms::TextBox^  txtName;
	private: System::Windows::Forms::Label^  label1;
	private: System::Windows::Forms::Label^  label4;
	private: System::Windows::Forms::Label^  label5;
	private: System::Windows::Forms::Label^  label7;
	private: System::Windows::Forms::Label^  label8;
	private: System::Windows::Forms::GroupBox^  gmMoveFlags;
	private: System::Windows::Forms::CheckBox^  cbNoBlock;
	private: System::Windows::Forms::GroupBox^  gbAlpha;
	private: System::Windows::Forms::GroupBox^  gbActionFlags;
	private: System::Windows::Forms::CheckBox^  cbCanUseOnSmtn;
	private: System::Windows::Forms::CheckBox^  cbCanUse;
	private: System::Windows::Forms::CheckBox^  cbCanPickUp;
private: System::Windows::Forms::NumericUpDown^  numAlpha;
	private: System::Windows::Forms::NumericUpDown^  nLightDist;
	private: System::Windows::Forms::NumericUpDown^  nLightInt;
	private: System::Windows::Forms::CheckBox^  cbHidden;
	private: System::Windows::Forms::NumericUpDown^  nWeight;
	private: System::Windows::Forms::NumericUpDown^  nSize;
	private: System::Windows::Forms::NumericUpDown^  nCost;
	private: System::Windows::Forms::Label^  label13;
	private: System::Windows::Forms::GroupBox^  gbMaterial;
	private: System::Windows::Forms::RadioButton^  rbLeather;
	private: System::Windows::Forms::RadioButton^  rbCement;
	private: System::Windows::Forms::RadioButton^  rbStone;
	private: System::Windows::Forms::RadioButton^  rbDirt;
	private: System::Windows::Forms::RadioButton^  rbWood;
	private: System::Windows::Forms::RadioButton^  rbPlastic;
	private: System::Windows::Forms::RadioButton^  rbMetal;
	private: System::Windows::Forms::RadioButton^  rbGlass;
	private: System::Windows::Forms::NumericUpDown^  nArmTypeFemale;
	private: System::Windows::Forms::NumericUpDown^  nArmTypeMale;
	private: System::Windows::Forms::Label^  label19;
	private: System::Windows::Forms::Label^  label18;
	private: System::Windows::Forms::Label^  label14;
	private: System::Windows::Forms::Label^  label12;
	private: System::Windows::Forms::Label^  label6;
	private: System::Windows::Forms::NumericUpDown^  nArmDRnorm;
	private: System::Windows::Forms::NumericUpDown^  nArmPerk;
	private: System::Windows::Forms::NumericUpDown^  nArmAC;
	private: System::Windows::Forms::NumericUpDown^  nArmDRexpl;
	private: System::Windows::Forms::Label^  label27;
	private: System::Windows::Forms::NumericUpDown^  nArmDRemp;
	private: System::Windows::Forms::Label^  label26;
	private: System::Windows::Forms::NumericUpDown^  nArmDRelectr;
	private: System::Windows::Forms::Label^  label25;
	private: System::Windows::Forms::NumericUpDown^  nArmDRplasma;
	private: System::Windows::Forms::Label^  label24;
	private: System::Windows::Forms::NumericUpDown^  nArmDRfire;
	private: System::Windows::Forms::Label^  label23;
	private: System::Windows::Forms::NumericUpDown^  nArmDRlaser;
	private: System::Windows::Forms::Label^  label22;
	private: System::Windows::Forms::NumericUpDown^  nArmDTexpl;
	private: System::Windows::Forms::Label^  label15;
	private: System::Windows::Forms::NumericUpDown^  nArmDTemp;
	private: System::Windows::Forms::Label^  label16;
	private: System::Windows::Forms::NumericUpDown^  nArmDTelectr;
	private: System::Windows::Forms::Label^  label17;
	private: System::Windows::Forms::NumericUpDown^  nArmDTplasma;
	private: System::Windows::Forms::Label^  label28;
	private: System::Windows::Forms::NumericUpDown^  nArmDTfire;
	private: System::Windows::Forms::Label^  label29;
	private: System::Windows::Forms::NumericUpDown^  nArmDTlaser;
	private: System::Windows::Forms::Label^  label30;
	private: System::Windows::Forms::NumericUpDown^  nArmDTnorm;
	private: System::Windows::Forms::Label^  label31;
	private: System::Windows::Forms::NumericUpDown^  nContSize;
	private: System::Windows::Forms::Label^  label46;
	private: System::Windows::Forms::Label^  label44;
	private: System::Windows::Forms::Label^  label40;
private: System::Windows::Forms::TabControl^  txtWeapSApic;
	private: System::Windows::Forms::TabPage^  tabPage11;
	private: System::Windows::Forms::TabPage^  tabPage12;
	private: System::Windows::Forms::TabPage^  tabPage13;
	private: System::Windows::Forms::Label^  label47;
private: System::Windows::Forms::NumericUpDown^  nWeapPAEffect;
	private: System::Windows::Forms::NumericUpDown^  nWeapPAdistmax;
	private: System::Windows::Forms::Label^  label66;
	private: System::Windows::Forms::Label^  label65;
	private: System::Windows::Forms::NumericUpDown^  nWeapPAdmgmin;
	private: System::Windows::Forms::NumericUpDown^  nWeapPAdmgmax;
	private: System::Windows::Forms::Label^  label64;
	private: System::Windows::Forms::Label^  label63;
	private: System::Windows::Forms::NumericUpDown^  nWeapPAanim2;
	private: System::Windows::Forms::Label^  label62;
	private: System::Windows::Forms::CheckBox^  cbWeapPAremove;
	private: System::Windows::Forms::CheckBox^  cbWeapPAaim;
	private: System::Windows::Forms::Label^  label61;
	private: System::Windows::Forms::NumericUpDown^  nWeapPAround;
	private: System::Windows::Forms::Label^  label60;
	private: System::Windows::Forms::ComboBox^  cmbWeapPAtypeattack;
	private: System::Windows::Forms::Label^  label58;
	private: System::Windows::Forms::NumericUpDown^  nWeapPAskill;
	private: System::Windows::Forms::Label^  label57;
private: System::Windows::Forms::NumericUpDown^  nWeapSAEffect;
	private: System::Windows::Forms::NumericUpDown^  nWeapSAdistmax;
	private: System::Windows::Forms::Label^  label67;
	private: System::Windows::Forms::Label^  label68;
	private: System::Windows::Forms::NumericUpDown^  nWeapSAdmgmin;
	private: System::Windows::Forms::NumericUpDown^  nWeapSAdmgmax;
	private: System::Windows::Forms::Label^  label69;
	private: System::Windows::Forms::Label^  label70;
	private: System::Windows::Forms::NumericUpDown^  nWeapSAanim2;
	private: System::Windows::Forms::Label^  label71;
	private: System::Windows::Forms::CheckBox^  cbWeapSAremove;
	private: System::Windows::Forms::CheckBox^  cbWeapSAaim;
	private: System::Windows::Forms::Label^  label72;
	private: System::Windows::Forms::NumericUpDown^  nWeapSAround;
	private: System::Windows::Forms::Label^  label73;
	private: System::Windows::Forms::ComboBox^  cmbWeapSAtypeattack;
	private: System::Windows::Forms::Label^  label75;
	private: System::Windows::Forms::NumericUpDown^  nWeapSAskill;
	private: System::Windows::Forms::Label^  label76;
	private: System::Windows::Forms::CheckBox^  cbWeapSA;
private: System::Windows::Forms::NumericUpDown^  nWeapTAEffect;
	private: System::Windows::Forms::NumericUpDown^  nWeapTAdistmax;
	private: System::Windows::Forms::Label^  label77;
	private: System::Windows::Forms::Label^  label78;
	private: System::Windows::Forms::NumericUpDown^  nWeapTAdmgmin;
	private: System::Windows::Forms::Label^  label79;
	private: System::Windows::Forms::Label^  label80;
	private: System::Windows::Forms::NumericUpDown^  nWeapTAanim2;
	private: System::Windows::Forms::Label^  label81;
	private: System::Windows::Forms::CheckBox^  cbWeapTAremove;
	private: System::Windows::Forms::CheckBox^  cbWeapTAaim;
private: System::Windows::Forms::Label^  label41;
private: System::Windows::Forms::Label^  label42;
private: System::Windows::Forms::Label^  label43;
	private: System::Windows::Forms::Label^  label82;
	private: System::Windows::Forms::Label^  label83;
	private: System::Windows::Forms::ComboBox^  cmbWeapTAtypeattack;
	private: System::Windows::Forms::Label^  label85;
	private: System::Windows::Forms::NumericUpDown^  nWeapTAskill;
	private: System::Windows::Forms::Label^  label86;
	private: System::Windows::Forms::CheckBox^  cbWeapTA;
	private: System::Windows::Forms::NumericUpDown^  nWeapCrFail;
	private: System::Windows::Forms::NumericUpDown^  nWeapCaliber;
	private: System::Windows::Forms::NumericUpDown^  nWeapHolder;
	private: System::Windows::Forms::NumericUpDown^  nWeapAnim1;
	private: System::Windows::Forms::Label^  label89;
	private: System::Windows::Forms::NumericUpDown^  nAmmoCaliber;
	private: System::Windows::Forms::NumericUpDown^  nAmmoSCount;
	private: System::Windows::Forms::Label^  label87;
	private: System::Windows::Forms::Label^  label88;
	private: System::Windows::Forms::Button^  bSave;
	private: System::Windows::Forms::Label^  label90;
	private: System::Windows::Forms::NumericUpDown^  nPid;
	private: System::Windows::Forms::CheckBox^  cbWeapPA;
	private: System::Windows::Forms::NumericUpDown^  nWeapPAtime;
	private: System::Windows::Forms::Label^  label100;
	private: System::Windows::Forms::NumericUpDown^  nWeapSAtime;
	private: System::Windows::Forms::Label^  label53;
	private: System::Windows::Forms::NumericUpDown^  nWeapTAtime;
	private: System::Windows::Forms::Label^  label54;
private: System::Windows::Forms::Label^  label55;
private: System::Windows::Forms::Label^  label91;
private: System::Windows::Forms::CheckBox^  cbFix;
private: System::Windows::Forms::ListBox^  lbList;
private: System::Windows::Forms::TabPage^  tGeneric;
private: System::Windows::Forms::Button^  bAdd;
private: System::Windows::Forms::GroupBox^  groupBox1;
private: System::Windows::Forms::Label^  label92;
private: System::Windows::Forms::CheckBox^  cbBigGun;
private: System::Windows::Forms::CheckBox^  cbTwoHands;
private: System::Windows::Forms::CheckBox^  cbWallTransEnd;
private: System::Windows::Forms::CheckBox^  cbLightThru;
private: System::Windows::Forms::CheckBox^  cbShootThru;
private: System::Windows::Forms::CheckBox^  cbMultiHex;
private: System::Windows::Forms::CheckBox^  cbCanLook;
private: System::Windows::Forms::CheckBox^  cbCanTalk;
private: System::Windows::Forms::GroupBox^  groupBox2;
private: System::Windows::Forms::TabControl^  tabControl1;
private: System::Windows::Forms::TabPage^  tabPage1;
private: System::Windows::Forms::TabPage^  tabPage2;
private: System::Windows::Forms::TabPage^  tMiscEx;
private: System::Windows::Forms::CheckBox^  cbShowArmor;
private: System::Windows::Forms::CheckBox^  cbShowMiscEx;
private: System::Windows::Forms::CheckBox^  cbShowMisc;
private: System::Windows::Forms::CheckBox^  cbShowAmmo;
private: System::Windows::Forms::CheckBox^  cbShowWeapon;
private: System::Windows::Forms::CheckBox^  cbShowDrug;
private: System::Windows::Forms::CheckBox^  cbShowGeneric;
private: System::Windows::Forms::CheckBox^  cbShowGrid;
private: System::Windows::Forms::CheckBox^  cbShowDoor;
private: System::Windows::Forms::CheckBox^  cbShowContainer;
private: System::Windows::Forms::CheckBox^  cbShowKey;
	private:
		/// <summary>
		/// Required designer variable.
		/// </summary>
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			System::ComponentModel::ComponentResourceManager^  resources = (gcnew System::ComponentModel::ComponentResourceManager(Form1::typeid));
			this->tcType = (gcnew System::Windows::Forms::TabControl());
			this->tArmor = (gcnew System::Windows::Forms::TabPage());
			this->nArmDTexpl = (gcnew System::Windows::Forms::NumericUpDown());
			this->label15 = (gcnew System::Windows::Forms::Label());
			this->nArmDTemp = (gcnew System::Windows::Forms::NumericUpDown());
			this->label16 = (gcnew System::Windows::Forms::Label());
			this->nArmDTelectr = (gcnew System::Windows::Forms::NumericUpDown());
			this->label17 = (gcnew System::Windows::Forms::Label());
			this->nArmDTplasma = (gcnew System::Windows::Forms::NumericUpDown());
			this->label28 = (gcnew System::Windows::Forms::Label());
			this->nArmDTfire = (gcnew System::Windows::Forms::NumericUpDown());
			this->label29 = (gcnew System::Windows::Forms::Label());
			this->nArmDTlaser = (gcnew System::Windows::Forms::NumericUpDown());
			this->label30 = (gcnew System::Windows::Forms::Label());
			this->nArmDTnorm = (gcnew System::Windows::Forms::NumericUpDown());
			this->label31 = (gcnew System::Windows::Forms::Label());
			this->nArmDRexpl = (gcnew System::Windows::Forms::NumericUpDown());
			this->label27 = (gcnew System::Windows::Forms::Label());
			this->nArmDRemp = (gcnew System::Windows::Forms::NumericUpDown());
			this->label26 = (gcnew System::Windows::Forms::Label());
			this->nArmDRelectr = (gcnew System::Windows::Forms::NumericUpDown());
			this->label25 = (gcnew System::Windows::Forms::Label());
			this->nArmDRplasma = (gcnew System::Windows::Forms::NumericUpDown());
			this->label24 = (gcnew System::Windows::Forms::Label());
			this->nArmDRfire = (gcnew System::Windows::Forms::NumericUpDown());
			this->label23 = (gcnew System::Windows::Forms::Label());
			this->nArmDRlaser = (gcnew System::Windows::Forms::NumericUpDown());
			this->label22 = (gcnew System::Windows::Forms::Label());
			this->nArmDRnorm = (gcnew System::Windows::Forms::NumericUpDown());
			this->nArmPerk = (gcnew System::Windows::Forms::NumericUpDown());
			this->nArmAC = (gcnew System::Windows::Forms::NumericUpDown());
			this->nArmTypeFemale = (gcnew System::Windows::Forms::NumericUpDown());
			this->nArmTypeMale = (gcnew System::Windows::Forms::NumericUpDown());
			this->label19 = (gcnew System::Windows::Forms::Label());
			this->label18 = (gcnew System::Windows::Forms::Label());
			this->label14 = (gcnew System::Windows::Forms::Label());
			this->label12 = (gcnew System::Windows::Forms::Label());
			this->label6 = (gcnew System::Windows::Forms::Label());
			this->tDrug = (gcnew System::Windows::Forms::TabPage());
			this->label32 = (gcnew System::Windows::Forms::Label());
			this->tWeapon = (gcnew System::Windows::Forms::TabPage());
			this->txtWeapSApic = (gcnew System::Windows::Forms::TabControl());
			this->cbWeapMain = (gcnew System::Windows::Forms::TabPage());
			this->cbWeapArmorPiercing = (gcnew System::Windows::Forms::CheckBox());
			this->label121 = (gcnew System::Windows::Forms::Label());
			this->numWeapUnarmedCritBonus = (gcnew System::Windows::Forms::NumericUpDown());
			this->numWeapUnarmedPriory = (gcnew System::Windows::Forms::NumericUpDown());
			this->label120 = (gcnew System::Windows::Forms::Label());
			this->numWeapMinUnarmed = (gcnew System::Windows::Forms::NumericUpDown());
			this->label119 = (gcnew System::Windows::Forms::Label());
			this->label118 = (gcnew System::Windows::Forms::Label());
			this->numWeapMinLevel = (gcnew System::Windows::Forms::NumericUpDown());
			this->label103 = (gcnew System::Windows::Forms::Label());
			this->numWeapUnarmedTree = (gcnew System::Windows::Forms::NumericUpDown());
			this->cbWeapUnarmed = (gcnew System::Windows::Forms::CheckBox());
			this->cbWeapIsNeedAct = (gcnew System::Windows::Forms::CheckBox());
			this->numWeapMinAg = (gcnew System::Windows::Forms::NumericUpDown());
			this->label108 = (gcnew System::Windows::Forms::Label());
			this->numWeapMinSt = (gcnew System::Windows::Forms::NumericUpDown());
			this->label102 = (gcnew System::Windows::Forms::Label());
			this->numWeapPerk = (gcnew System::Windows::Forms::NumericUpDown());
			this->label105 = (gcnew System::Windows::Forms::Label());
			this->numWeapDefAmmo = (gcnew System::Windows::Forms::NumericUpDown());
			this->label99 = (gcnew System::Windows::Forms::Label());
			this->cbWeapNoWear = (gcnew System::Windows::Forms::CheckBox());
			this->label40 = (gcnew System::Windows::Forms::Label());
			this->nWeapCrFail = (gcnew System::Windows::Forms::NumericUpDown());
			this->label47 = (gcnew System::Windows::Forms::Label());
			this->nWeapAnim1 = (gcnew System::Windows::Forms::NumericUpDown());
			this->nWeapCaliber = (gcnew System::Windows::Forms::NumericUpDown());
			this->nWeapHolder = (gcnew System::Windows::Forms::NumericUpDown());
			this->label44 = (gcnew System::Windows::Forms::Label());
			this->label46 = (gcnew System::Windows::Forms::Label());
			this->tabPage11 = (gcnew System::Windows::Forms::TabPage());
			this->txtWeapPApic = (gcnew System::Windows::Forms::TextBox());
			this->label41 = (gcnew System::Windows::Forms::Label());
			this->txtWeapPASoundId = (gcnew System::Windows::Forms::TextBox());
			this->label104 = (gcnew System::Windows::Forms::Label());
			this->nWeapPAtime = (gcnew System::Windows::Forms::NumericUpDown());
			this->label100 = (gcnew System::Windows::Forms::Label());
			this->nWeapPAEffect = (gcnew System::Windows::Forms::NumericUpDown());
			this->nWeapPAdistmax = (gcnew System::Windows::Forms::NumericUpDown());
			this->label66 = (gcnew System::Windows::Forms::Label());
			this->label65 = (gcnew System::Windows::Forms::Label());
			this->nWeapPAdmgmin = (gcnew System::Windows::Forms::NumericUpDown());
			this->nWeapPAdmgmax = (gcnew System::Windows::Forms::NumericUpDown());
			this->label64 = (gcnew System::Windows::Forms::Label());
			this->label63 = (gcnew System::Windows::Forms::Label());
			this->nWeapPAanim2 = (gcnew System::Windows::Forms::NumericUpDown());
			this->label62 = (gcnew System::Windows::Forms::Label());
			this->cbWeapPAremove = (gcnew System::Windows::Forms::CheckBox());
			this->cbWeapPAaim = (gcnew System::Windows::Forms::CheckBox());
			this->label61 = (gcnew System::Windows::Forms::Label());
			this->nWeapPAround = (gcnew System::Windows::Forms::NumericUpDown());
			this->label60 = (gcnew System::Windows::Forms::Label());
			this->cmbWeapPAtypeattack = (gcnew System::Windows::Forms::ComboBox());
			this->label58 = (gcnew System::Windows::Forms::Label());
			this->nWeapPAskill = (gcnew System::Windows::Forms::NumericUpDown());
			this->label57 = (gcnew System::Windows::Forms::Label());
			this->cbWeapPA = (gcnew System::Windows::Forms::CheckBox());
			this->tabPage12 = (gcnew System::Windows::Forms::TabPage());
			this->textBox3 = (gcnew System::Windows::Forms::TextBox());
			this->label42 = (gcnew System::Windows::Forms::Label());
			this->txtWeapSASoundId = (gcnew System::Windows::Forms::TextBox());
			this->label106 = (gcnew System::Windows::Forms::Label());
			this->nWeapSAtime = (gcnew System::Windows::Forms::NumericUpDown());
			this->label53 = (gcnew System::Windows::Forms::Label());
			this->nWeapSAEffect = (gcnew System::Windows::Forms::NumericUpDown());
			this->nWeapSAdistmax = (gcnew System::Windows::Forms::NumericUpDown());
			this->label67 = (gcnew System::Windows::Forms::Label());
			this->label68 = (gcnew System::Windows::Forms::Label());
			this->nWeapSAdmgmin = (gcnew System::Windows::Forms::NumericUpDown());
			this->nWeapSAdmgmax = (gcnew System::Windows::Forms::NumericUpDown());
			this->label69 = (gcnew System::Windows::Forms::Label());
			this->label70 = (gcnew System::Windows::Forms::Label());
			this->nWeapSAanim2 = (gcnew System::Windows::Forms::NumericUpDown());
			this->label71 = (gcnew System::Windows::Forms::Label());
			this->cbWeapSAremove = (gcnew System::Windows::Forms::CheckBox());
			this->cbWeapSAaim = (gcnew System::Windows::Forms::CheckBox());
			this->label72 = (gcnew System::Windows::Forms::Label());
			this->nWeapSAround = (gcnew System::Windows::Forms::NumericUpDown());
			this->label73 = (gcnew System::Windows::Forms::Label());
			this->cmbWeapSAtypeattack = (gcnew System::Windows::Forms::ComboBox());
			this->label75 = (gcnew System::Windows::Forms::Label());
			this->nWeapSAskill = (gcnew System::Windows::Forms::NumericUpDown());
			this->label76 = (gcnew System::Windows::Forms::Label());
			this->cbWeapSA = (gcnew System::Windows::Forms::CheckBox());
			this->tabPage13 = (gcnew System::Windows::Forms::TabPage());
			this->txtWeapTApic = (gcnew System::Windows::Forms::TextBox());
			this->label43 = (gcnew System::Windows::Forms::Label());
			this->txtWeapTASoundId = (gcnew System::Windows::Forms::TextBox());
			this->label107 = (gcnew System::Windows::Forms::Label());
			this->nWeapTAtime = (gcnew System::Windows::Forms::NumericUpDown());
			this->label54 = (gcnew System::Windows::Forms::Label());
			this->nWeapTAEffect = (gcnew System::Windows::Forms::NumericUpDown());
			this->nWeapTAdistmax = (gcnew System::Windows::Forms::NumericUpDown());
			this->label77 = (gcnew System::Windows::Forms::Label());
			this->label78 = (gcnew System::Windows::Forms::Label());
			this->nWeapTAdmgmin = (gcnew System::Windows::Forms::NumericUpDown());
			this->nWeapTAdmgmax = (gcnew System::Windows::Forms::NumericUpDown());
			this->label79 = (gcnew System::Windows::Forms::Label());
			this->label80 = (gcnew System::Windows::Forms::Label());
			this->nWeapTAanim2 = (gcnew System::Windows::Forms::NumericUpDown());
			this->label81 = (gcnew System::Windows::Forms::Label());
			this->cbWeapTAremove = (gcnew System::Windows::Forms::CheckBox());
			this->cbWeapTAaim = (gcnew System::Windows::Forms::CheckBox());
			this->label82 = (gcnew System::Windows::Forms::Label());
			this->nWeapTAround = (gcnew System::Windows::Forms::NumericUpDown());
			this->label83 = (gcnew System::Windows::Forms::Label());
			this->cmbWeapTAtypeattack = (gcnew System::Windows::Forms::ComboBox());
			this->label85 = (gcnew System::Windows::Forms::Label());
			this->nWeapTAskill = (gcnew System::Windows::Forms::NumericUpDown());
			this->label86 = (gcnew System::Windows::Forms::Label());
			this->cbWeapTA = (gcnew System::Windows::Forms::CheckBox());
			this->tAmmo = (gcnew System::Windows::Forms::TabPage());
			this->numAmmoDmgDiv = (gcnew System::Windows::Forms::NumericUpDown());
			this->numAmmoDmgMult = (gcnew System::Windows::Forms::NumericUpDown());
			this->numAmmoDRMod = (gcnew System::Windows::Forms::NumericUpDown());
			this->numAmmoACMod = (gcnew System::Windows::Forms::NumericUpDown());
			this->label93 = (gcnew System::Windows::Forms::Label());
			this->label39 = (gcnew System::Windows::Forms::Label());
			this->label21 = (gcnew System::Windows::Forms::Label());
			this->label20 = (gcnew System::Windows::Forms::Label());
			this->label9 = (gcnew System::Windows::Forms::Label());
			this->label89 = (gcnew System::Windows::Forms::Label());
			this->nAmmoCaliber = (gcnew System::Windows::Forms::NumericUpDown());
			this->nAmmoSCount = (gcnew System::Windows::Forms::NumericUpDown());
			this->label88 = (gcnew System::Windows::Forms::Label());
			this->label87 = (gcnew System::Windows::Forms::Label());
			this->tContainer = (gcnew System::Windows::Forms::TabPage());
			this->cbContIsNoOpen = (gcnew System::Windows::Forms::CheckBox());
			this->cbContCannotPickUp = (gcnew System::Windows::Forms::CheckBox());
			this->cbContMagicHandsGrnd = (gcnew System::Windows::Forms::CheckBox());
			this->cbContChangeble = (gcnew System::Windows::Forms::CheckBox());
			this->label97 = (gcnew System::Windows::Forms::Label());
			this->nContSize = (gcnew System::Windows::Forms::NumericUpDown());
			this->tMiscEx = (gcnew System::Windows::Forms::TabPage());
			this->numMisc2SV3 = (gcnew System::Windows::Forms::NumericUpDown());
			this->numMisc2SV2 = (gcnew System::Windows::Forms::NumericUpDown());
			this->groupBox5 = (gcnew System::Windows::Forms::GroupBox());
			this->label10 = (gcnew System::Windows::Forms::Label());
			this->numMisc2CarEntire = (gcnew System::Windows::Forms::NumericUpDown());
			this->label117 = (gcnew System::Windows::Forms::Label());
			this->label116 = (gcnew System::Windows::Forms::Label());
			this->label115 = (gcnew System::Windows::Forms::Label());
			this->label114 = (gcnew System::Windows::Forms::Label());
			this->label101 = (gcnew System::Windows::Forms::Label());
			this->label84 = (gcnew System::Windows::Forms::Label());
			this->label11 = (gcnew System::Windows::Forms::Label());
			this->numMisc2CarFuelConsumption = (gcnew System::Windows::Forms::NumericUpDown());
			this->numMisc2CarRunToBreak = (gcnew System::Windows::Forms::NumericUpDown());
			this->numMisc2CarFuelTank = (gcnew System::Windows::Forms::NumericUpDown());
			this->numMisc2CarCritCapacity = (gcnew System::Windows::Forms::NumericUpDown());
			this->numMisc2CarWearConsumption = (gcnew System::Windows::Forms::NumericUpDown());
			this->numMisc2CarNegotiability = (gcnew System::Windows::Forms::NumericUpDown());
			this->numMisc2CarSpeed = (gcnew System::Windows::Forms::NumericUpDown());
			this->rbMisc2CarWater = (gcnew System::Windows::Forms::RadioButton());
			this->rbMisc2CarGround = (gcnew System::Windows::Forms::RadioButton());
			this->rbMisc2CarFly = (gcnew System::Windows::Forms::RadioButton());
			this->txtMisc2CarBag2 = (gcnew System::Windows::Forms::TextBox());
			this->label112 = (gcnew System::Windows::Forms::Label());
			this->txtMisc2CarBag1 = (gcnew System::Windows::Forms::TextBox());
			this->label111 = (gcnew System::Windows::Forms::Label());
			this->label109 = (gcnew System::Windows::Forms::Label());
			this->txtMisc2CarBlocks = (gcnew System::Windows::Forms::TextBox());
			this->cbMisc2IsCar = (gcnew System::Windows::Forms::CheckBox());
			this->numMisc2SV1 = (gcnew System::Windows::Forms::NumericUpDown());
			this->label96 = (gcnew System::Windows::Forms::Label());
			this->label95 = (gcnew System::Windows::Forms::Label());
			this->label94 = (gcnew System::Windows::Forms::Label());
			this->tDoor = (gcnew System::Windows::Forms::TabPage());
			this->cbDoorIsNoOpen = (gcnew System::Windows::Forms::CheckBox());
			this->cbDoorWalkThru = (gcnew System::Windows::Forms::CheckBox());
			this->tGrid = (gcnew System::Windows::Forms::TabPage());
			this->groupBox3 = (gcnew System::Windows::Forms::GroupBox());
			this->rbGridElevatr = (gcnew System::Windows::Forms::RadioButton());
			this->rbGridLadderTop = (gcnew System::Windows::Forms::RadioButton());
			this->rbGridLadderBottom = (gcnew System::Windows::Forms::RadioButton());
			this->rbGridStairs = (gcnew System::Windows::Forms::RadioButton());
			this->rbGridExitGrid = (gcnew System::Windows::Forms::RadioButton());
			this->tMisc = (gcnew System::Windows::Forms::TabPage());
			this->tKey = (gcnew System::Windows::Forms::TabPage());
			this->tGeneric = (gcnew System::Windows::Forms::TabPage());
			this->tWall = (gcnew System::Windows::Forms::TabPage());
			this->txtInfo = (gcnew System::Windows::Forms::RichTextBox());
			this->txtName = (gcnew System::Windows::Forms::TextBox());
			this->label1 = (gcnew System::Windows::Forms::Label());
			this->label4 = (gcnew System::Windows::Forms::Label());
			this->label5 = (gcnew System::Windows::Forms::Label());
			this->label7 = (gcnew System::Windows::Forms::Label());
			this->label8 = (gcnew System::Windows::Forms::Label());
			this->gmMoveFlags = (gcnew System::Windows::Forms::GroupBox());
			this->cbCached = (gcnew System::Windows::Forms::CheckBox());
			this->cbGag = (gcnew System::Windows::Forms::CheckBox());
			this->cbBigGun = (gcnew System::Windows::Forms::CheckBox());
			this->cbTwoHands = (gcnew System::Windows::Forms::CheckBox());
			this->cbWallTransEnd = (gcnew System::Windows::Forms::CheckBox());
			this->cbLightThru = (gcnew System::Windows::Forms::CheckBox());
			this->cbShootThru = (gcnew System::Windows::Forms::CheckBox());
			this->cbMultiHex = (gcnew System::Windows::Forms::CheckBox());
			this->cbNoBlock = (gcnew System::Windows::Forms::CheckBox());
			this->cbHidden = (gcnew System::Windows::Forms::CheckBox());
			this->cbFlat = (gcnew System::Windows::Forms::CheckBox());
			this->gbAlpha = (gcnew System::Windows::Forms::GroupBox());
			this->cbColorizing = (gcnew System::Windows::Forms::CheckBox());
			this->label35 = (gcnew System::Windows::Forms::Label());
			this->label34 = (gcnew System::Windows::Forms::Label());
			this->label33 = (gcnew System::Windows::Forms::Label());
			this->numBlue = (gcnew System::Windows::Forms::NumericUpDown());
			this->numGreen = (gcnew System::Windows::Forms::NumericUpDown());
			this->numRed = (gcnew System::Windows::Forms::NumericUpDown());
			this->label110 = (gcnew System::Windows::Forms::Label());
			this->numAlpha = (gcnew System::Windows::Forms::NumericUpDown());
			this->gbActionFlags = (gcnew System::Windows::Forms::GroupBox());
			this->cbNoSteal = (gcnew System::Windows::Forms::CheckBox());
			this->cbNoLoot = (gcnew System::Windows::Forms::CheckBox());
			this->cbTrap = (gcnew System::Windows::Forms::CheckBox());
			this->cbNoLightInfluence = (gcnew System::Windows::Forms::CheckBox());
			this->cbNoHighlight = (gcnew System::Windows::Forms::CheckBox());
			this->cbGekk = (gcnew System::Windows::Forms::CheckBox());
			this->cbAlwaysView = (gcnew System::Windows::Forms::CheckBox());
			this->cbBadItem = (gcnew System::Windows::Forms::CheckBox());
			this->cbHasTimer = (gcnew System::Windows::Forms::CheckBox());
			this->cbCanLook = (gcnew System::Windows::Forms::CheckBox());
			this->cbCanTalk = (gcnew System::Windows::Forms::CheckBox());
			this->cbCanUseOnSmtn = (gcnew System::Windows::Forms::CheckBox());
			this->cbCanUse = (gcnew System::Windows::Forms::CheckBox());
			this->cbCanPickUp = (gcnew System::Windows::Forms::CheckBox());
			this->nLightDist = (gcnew System::Windows::Forms::NumericUpDown());
			this->nLightInt = (gcnew System::Windows::Forms::NumericUpDown());
			this->nWeight = (gcnew System::Windows::Forms::NumericUpDown());
			this->nSize = (gcnew System::Windows::Forms::NumericUpDown());
			this->nCost = (gcnew System::Windows::Forms::NumericUpDown());
			this->label13 = (gcnew System::Windows::Forms::Label());
			this->gbMaterial = (gcnew System::Windows::Forms::GroupBox());
			this->rbLeather = (gcnew System::Windows::Forms::RadioButton());
			this->rbCement = (gcnew System::Windows::Forms::RadioButton());
			this->rbStone = (gcnew System::Windows::Forms::RadioButton());
			this->rbDirt = (gcnew System::Windows::Forms::RadioButton());
			this->rbWood = (gcnew System::Windows::Forms::RadioButton());
			this->rbPlastic = (gcnew System::Windows::Forms::RadioButton());
			this->rbMetal = (gcnew System::Windows::Forms::RadioButton());
			this->rbGlass = (gcnew System::Windows::Forms::RadioButton());
			this->bSave = (gcnew System::Windows::Forms::Button());
			this->label90 = (gcnew System::Windows::Forms::Label());
			this->nPid = (gcnew System::Windows::Forms::NumericUpDown());
			this->label55 = (gcnew System::Windows::Forms::Label());
			this->label91 = (gcnew System::Windows::Forms::Label());
			this->cbFix = (gcnew System::Windows::Forms::CheckBox());
			this->lbList = (gcnew System::Windows::Forms::ListBox());
			this->bAdd = (gcnew System::Windows::Forms::Button());
			this->groupBox1 = (gcnew System::Windows::Forms::GroupBox());
			this->cbDisableDir4 = (gcnew System::Windows::Forms::CheckBox());
			this->cbDisableDir5 = (gcnew System::Windows::Forms::CheckBox());
			this->cbDisableDir3 = (gcnew System::Windows::Forms::CheckBox());
			this->cbDisableDir2 = (gcnew System::Windows::Forms::CheckBox());
			this->cbDisableDir1 = (gcnew System::Windows::Forms::CheckBox());
			this->cbDisableDir0 = (gcnew System::Windows::Forms::CheckBox());
			this->cbLightGlobal = (gcnew System::Windows::Forms::CheckBox());
			this->cbLightInverse = (gcnew System::Windows::Forms::CheckBox());
			this->cbLight = (gcnew System::Windows::Forms::CheckBox());
			this->groupBox2 = (gcnew System::Windows::Forms::GroupBox());
			this->tabControl1 = (gcnew System::Windows::Forms::TabControl());
			this->tabPage1 = (gcnew System::Windows::Forms::TabPage());
			this->txtPicInv = (gcnew System::Windows::Forms::TextBox());
			this->txtPicMap = (gcnew System::Windows::Forms::TextBox());
			this->bLoad = (gcnew System::Windows::Forms::Button());
			this->bSaveList = (gcnew System::Windows::Forms::Button());
			this->bLoad1 = (gcnew System::Windows::Forms::Button());
			this->bSave1 = (gcnew System::Windows::Forms::Button());
			this->cbShowWall = (gcnew System::Windows::Forms::CheckBox());
			this->cbShowGeneric = (gcnew System::Windows::Forms::CheckBox());
			this->cbShowGrid = (gcnew System::Windows::Forms::CheckBox());
			this->cbShowDoor = (gcnew System::Windows::Forms::CheckBox());
			this->cbShowContainer = (gcnew System::Windows::Forms::CheckBox());
			this->cbShowKey = (gcnew System::Windows::Forms::CheckBox());
			this->cbShowMiscEx = (gcnew System::Windows::Forms::CheckBox());
			this->cbShowMisc = (gcnew System::Windows::Forms::CheckBox());
			this->cbShowAmmo = (gcnew System::Windows::Forms::CheckBox());
			this->cbShowWeapon = (gcnew System::Windows::Forms::CheckBox());
			this->cbShowDrug = (gcnew System::Windows::Forms::CheckBox());
			this->cbShowArmor = (gcnew System::Windows::Forms::CheckBox());
			this->tabPage2 = (gcnew System::Windows::Forms::TabPage());
			this->groupBox11 = (gcnew System::Windows::Forms::GroupBox());
			this->rbLightSouth = (gcnew System::Windows::Forms::RadioButton());
			this->rbLightEast = (gcnew System::Windows::Forms::RadioButton());
			this->rbLightNorth = (gcnew System::Windows::Forms::RadioButton());
			this->rbLightWest = (gcnew System::Windows::Forms::RadioButton());
			this->rbLightWestEast = (gcnew System::Windows::Forms::RadioButton());
			this->rbLightNorthSouth = (gcnew System::Windows::Forms::RadioButton());
			this->groupBox10 = (gcnew System::Windows::Forms::GroupBox());
			this->cbDisableEgg = (gcnew System::Windows::Forms::CheckBox());
			this->groupBox9 = (gcnew System::Windows::Forms::GroupBox());
			this->numSlot = (gcnew System::Windows::Forms::NumericUpDown());
			this->groupBox7 = (gcnew System::Windows::Forms::GroupBox());
			this->numDrawPosOffsY = (gcnew System::Windows::Forms::NumericUpDown());
			this->groupBox4 = (gcnew System::Windows::Forms::GroupBox());
			this->txtSoundId = (gcnew System::Windows::Forms::TextBox());
			this->tabPage3 = (gcnew System::Windows::Forms::TabPage());
			this->groupBox8 = (gcnew System::Windows::Forms::GroupBox());
			this->label3 = (gcnew System::Windows::Forms::Label());
			this->label2 = (gcnew System::Windows::Forms::Label());
			this->txtScriptFunc = (gcnew System::Windows::Forms::TextBox());
			this->txtScriptModule = (gcnew System::Windows::Forms::TextBox());
			this->groupBox6 = (gcnew System::Windows::Forms::GroupBox());
			this->label74 = (gcnew System::Windows::Forms::Label());
			this->label59 = (gcnew System::Windows::Forms::Label());
			this->label45 = (gcnew System::Windows::Forms::Label());
			this->numHideAnim1 = (gcnew System::Windows::Forms::NumericUpDown());
			this->numHideAnim0 = (gcnew System::Windows::Forms::NumericUpDown());
			this->numShowAnim1 = (gcnew System::Windows::Forms::NumericUpDown());
			this->numShowAnim0 = (gcnew System::Windows::Forms::NumericUpDown());
			this->numStayAnim1 = (gcnew System::Windows::Forms::NumericUpDown());
			this->numStayAnim0 = (gcnew System::Windows::Forms::NumericUpDown());
			this->cbShowAnimExt = (gcnew System::Windows::Forms::CheckBox());
			this->numAnimWaitRndMax = (gcnew System::Windows::Forms::NumericUpDown());
			this->numAnimWaitRndMin = (gcnew System::Windows::Forms::NumericUpDown());
			this->label113 = (gcnew System::Windows::Forms::Label());
			this->numAnimWaitBase = (gcnew System::Windows::Forms::NumericUpDown());
			this->cbShowAnim = (gcnew System::Windows::Forms::CheckBox());
			this->cmbLog = (gcnew System::Windows::Forms::ComboBox());
			this->tcType->SuspendLayout();
			this->tArmor->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTexpl))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTemp))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTelectr))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTplasma))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTfire))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTlaser))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTnorm))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRexpl))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRemp))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRelectr))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRplasma))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRfire))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRlaser))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRnorm))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmPerk))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmAC))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmTypeFemale))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmTypeMale))->BeginInit();
			this->tDrug->SuspendLayout();
			this->tWeapon->SuspendLayout();
			this->txtWeapSApic->SuspendLayout();
			this->cbWeapMain->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapUnarmedCritBonus))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapUnarmedPriory))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapMinUnarmed))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapMinLevel))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapUnarmedTree))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapMinAg))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapMinSt))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapPerk))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapDefAmmo))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapCrFail))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapAnim1))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapCaliber))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapHolder))->BeginInit();
			this->tabPage11->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAtime))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAEffect))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAdistmax))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAdmgmin))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAdmgmax))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAanim2))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAround))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAskill))->BeginInit();
			this->tabPage12->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAtime))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAEffect))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAdistmax))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAdmgmin))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAdmgmax))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAanim2))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAround))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAskill))->BeginInit();
			this->tabPage13->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAtime))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAEffect))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAdistmax))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAdmgmin))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAdmgmax))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAanim2))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAround))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAskill))->BeginInit();
			this->tAmmo->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAmmoDmgDiv))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAmmoDmgMult))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAmmoDRMod))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAmmoACMod))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nAmmoCaliber))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nAmmoSCount))->BeginInit();
			this->tContainer->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nContSize))->BeginInit();
			this->tMiscEx->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2SV3))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2SV2))->BeginInit();
			this->groupBox5->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarEntire))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarFuelConsumption))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarRunToBreak))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarFuelTank))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarCritCapacity))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarWearConsumption))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarNegotiability))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarSpeed))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2SV1))->BeginInit();
			this->tDoor->SuspendLayout();
			this->tGrid->SuspendLayout();
			this->groupBox3->SuspendLayout();
			this->gmMoveFlags->SuspendLayout();
			this->gbAlpha->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numBlue))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numGreen))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numRed))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAlpha))->BeginInit();
			this->gbActionFlags->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nLightDist))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nLightInt))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeight))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nSize))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nCost))->BeginInit();
			this->gbMaterial->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPid))->BeginInit();
			this->groupBox1->SuspendLayout();
			this->groupBox2->SuspendLayout();
			this->tabControl1->SuspendLayout();
			this->tabPage1->SuspendLayout();
			this->tabPage2->SuspendLayout();
			this->groupBox11->SuspendLayout();
			this->groupBox10->SuspendLayout();
			this->groupBox9->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numSlot))->BeginInit();
			this->groupBox7->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numDrawPosOffsY))->BeginInit();
			this->groupBox4->SuspendLayout();
			this->tabPage3->SuspendLayout();
			this->groupBox8->SuspendLayout();
			this->groupBox6->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numHideAnim1))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numHideAnim0))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numShowAnim1))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numShowAnim0))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numStayAnim1))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numStayAnim0))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAnimWaitRndMax))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAnimWaitRndMin))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAnimWaitBase))->BeginInit();
			this->SuspendLayout();
			// 
			// tcType
			// 
			this->tcType->Controls->Add(this->tArmor);
			this->tcType->Controls->Add(this->tDrug);
			this->tcType->Controls->Add(this->tWeapon);
			this->tcType->Controls->Add(this->tAmmo);
			this->tcType->Controls->Add(this->tContainer);
			this->tcType->Controls->Add(this->tMiscEx);
			this->tcType->Controls->Add(this->tDoor);
			this->tcType->Controls->Add(this->tGrid);
			this->tcType->Controls->Add(this->tMisc);
			this->tcType->Controls->Add(this->tKey);
			this->tcType->Controls->Add(this->tGeneric);
			this->tcType->Controls->Add(this->tWall);
			this->tcType->Location = System::Drawing::Point(12, 308);
			this->tcType->Name = L"tcType";
			this->tcType->SelectedIndex = 0;
			this->tcType->Size = System::Drawing::Size(668, 195);
			this->tcType->TabIndex = 0;
			this->tcType->Selecting += gcnew System::Windows::Forms::TabControlCancelEventHandler(this, &Form1::tcType_Selecting);
			this->tcType->Selected += gcnew System::Windows::Forms::TabControlEventHandler(this, &Form1::tcType_Selected);
			this->tcType->SelectedIndexChanged += gcnew System::EventHandler(this, &Form1::tcType_SelectedIndexChanged);
			// 
			// tArmor
			// 
			this->tArmor->Controls->Add(this->nArmDTexpl);
			this->tArmor->Controls->Add(this->label15);
			this->tArmor->Controls->Add(this->nArmDTemp);
			this->tArmor->Controls->Add(this->label16);
			this->tArmor->Controls->Add(this->nArmDTelectr);
			this->tArmor->Controls->Add(this->label17);
			this->tArmor->Controls->Add(this->nArmDTplasma);
			this->tArmor->Controls->Add(this->label28);
			this->tArmor->Controls->Add(this->nArmDTfire);
			this->tArmor->Controls->Add(this->label29);
			this->tArmor->Controls->Add(this->nArmDTlaser);
			this->tArmor->Controls->Add(this->label30);
			this->tArmor->Controls->Add(this->nArmDTnorm);
			this->tArmor->Controls->Add(this->label31);
			this->tArmor->Controls->Add(this->nArmDRexpl);
			this->tArmor->Controls->Add(this->label27);
			this->tArmor->Controls->Add(this->nArmDRemp);
			this->tArmor->Controls->Add(this->label26);
			this->tArmor->Controls->Add(this->nArmDRelectr);
			this->tArmor->Controls->Add(this->label25);
			this->tArmor->Controls->Add(this->nArmDRplasma);
			this->tArmor->Controls->Add(this->label24);
			this->tArmor->Controls->Add(this->nArmDRfire);
			this->tArmor->Controls->Add(this->label23);
			this->tArmor->Controls->Add(this->nArmDRlaser);
			this->tArmor->Controls->Add(this->label22);
			this->tArmor->Controls->Add(this->nArmDRnorm);
			this->tArmor->Controls->Add(this->nArmPerk);
			this->tArmor->Controls->Add(this->nArmAC);
			this->tArmor->Controls->Add(this->nArmTypeFemale);
			this->tArmor->Controls->Add(this->nArmTypeMale);
			this->tArmor->Controls->Add(this->label19);
			this->tArmor->Controls->Add(this->label18);
			this->tArmor->Controls->Add(this->label14);
			this->tArmor->Controls->Add(this->label12);
			this->tArmor->Controls->Add(this->label6);
			this->tArmor->Location = System::Drawing::Point(4, 22);
			this->tArmor->Name = L"tArmor";
			this->tArmor->Padding = System::Windows::Forms::Padding(3);
			this->tArmor->Size = System::Drawing::Size(660, 169);
			this->tArmor->TabIndex = 0;
			this->tArmor->Text = L"Armor";
			this->tArmor->UseVisualStyleBackColor = true;
			this->tArmor->Click += gcnew System::EventHandler(this, &Form1::tabPage1_Click);
			// 
			// nArmDTexpl
			// 
			this->nArmDTexpl->Location = System::Drawing::Point(476, 133);
			this->nArmDTexpl->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDTexpl->Name = L"nArmDTexpl";
			this->nArmDTexpl->Size = System::Drawing::Size(43, 20);
			this->nArmDTexpl->TabIndex = 48;
			// 
			// label15
			// 
			this->label15->AutoSize = true;
			this->label15->Location = System::Drawing::Point(382, 135);
			this->label15->Name = L"label15";
			this->label15->Size = System::Drawing::Size(61, 13);
			this->label15->TabIndex = 47;
			this->label15->Text = L"DT Explode";
			// 
			// nArmDTemp
			// 
			this->nArmDTemp->Location = System::Drawing::Point(476, 114);
			this->nArmDTemp->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDTemp->Name = L"nArmDTemp";
			this->nArmDTemp->Size = System::Drawing::Size(43, 20);
			this->nArmDTemp->TabIndex = 46;
			// 
			// label16
			// 
			this->label16->AutoSize = true;
			this->label16->Location = System::Drawing::Point(382, 116);
			this->label16->Name = L"label16";
			this->label16->Size = System::Drawing::Size(43, 13);
			this->label16->TabIndex = 45;
			this->label16->Text = L"DT EMP";
			this->label16->Click += gcnew System::EventHandler(this, &Form1::label16_Click);
			// 
			// nArmDTelectr
			// 
			this->nArmDTelectr->Location = System::Drawing::Point(476, 96);
			this->nArmDTelectr->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDTelectr->Name = L"nArmDTelectr";
			this->nArmDTelectr->Size = System::Drawing::Size(43, 20);
			this->nArmDTelectr->TabIndex = 44;
			// 
			// label17
			// 
			this->label17->AutoSize = true;
			this->label17->Location = System::Drawing::Point(382, 98);
			this->label17->Name = L"label17";
			this->label17->Size = System::Drawing::Size(56, 13);
			this->label17->TabIndex = 43;
			this->label17->Text = L"DT Electro";
			// 
			// nArmDTplasma
			// 
			this->nArmDTplasma->Location = System::Drawing::Point(476, 78);
			this->nArmDTplasma->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDTplasma->Name = L"nArmDTplasma";
			this->nArmDTplasma->Size = System::Drawing::Size(43, 20);
			this->nArmDTplasma->TabIndex = 42;
			// 
			// label28
			// 
			this->label28->AutoSize = true;
			this->label28->Location = System::Drawing::Point(382, 80);
			this->label28->Name = L"label28";
			this->label28->Size = System::Drawing::Size(56, 13);
			this->label28->TabIndex = 41;
			this->label28->Text = L"DT Plasma";
			// 
			// nArmDTfire
			// 
			this->nArmDTfire->Location = System::Drawing::Point(476, 59);
			this->nArmDTfire->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDTfire->Name = L"nArmDTfire";
			this->nArmDTfire->Size = System::Drawing::Size(43, 20);
			this->nArmDTfire->TabIndex = 40;
			this->nArmDTfire->ValueChanged += gcnew System::EventHandler(this, &Form1::numericUpDown29_ValueChanged);
			// 
			// label29
			// 
			this->label29->AutoSize = true;
			this->label29->Location = System::Drawing::Point(382, 61);
			this->label29->Name = L"label29";
			this->label29->Size = System::Drawing::Size(41, 13);
			this->label29->TabIndex = 39;
			this->label29->Text = L"DT Fire";
			// 
			// nArmDTlaser
			// 
			this->nArmDTlaser->Location = System::Drawing::Point(476, 41);
			this->nArmDTlaser->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDTlaser->Name = L"nArmDTlaser";
			this->nArmDTlaser->Size = System::Drawing::Size(43, 20);
			this->nArmDTlaser->TabIndex = 38;
			// 
			// label30
			// 
			this->label30->AutoSize = true;
			this->label30->Location = System::Drawing::Point(382, 43);
			this->label30->Name = L"label30";
			this->label30->Size = System::Drawing::Size(49, 13);
			this->label30->TabIndex = 37;
			this->label30->Text = L"DT Laser";
			// 
			// nArmDTnorm
			// 
			this->nArmDTnorm->Location = System::Drawing::Point(476, 22);
			this->nArmDTnorm->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDTnorm->Name = L"nArmDTnorm";
			this->nArmDTnorm->Size = System::Drawing::Size(43, 20);
			this->nArmDTnorm->TabIndex = 36;
			// 
			// label31
			// 
			this->label31->AutoSize = true;
			this->label31->Location = System::Drawing::Point(382, 24);
			this->label31->Name = L"label31";
			this->label31->Size = System::Drawing::Size(56, 13);
			this->label31->TabIndex = 35;
			this->label31->Text = L"DT Normal";
			// 
			// nArmDRexpl
			// 
			this->nArmDRexpl->Location = System::Drawing::Point(291, 133);
			this->nArmDRexpl->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDRexpl->Name = L"nArmDRexpl";
			this->nArmDRexpl->Size = System::Drawing::Size(43, 20);
			this->nArmDRexpl->TabIndex = 34;
			// 
			// label27
			// 
			this->label27->AutoSize = true;
			this->label27->Location = System::Drawing::Point(206, 135);
			this->label27->Name = L"label27";
			this->label27->Size = System::Drawing::Size(62, 13);
			this->label27->TabIndex = 33;
			this->label27->Text = L"DR Explode";
			this->label27->Click += gcnew System::EventHandler(this, &Form1::label27_Click);
			// 
			// nArmDRemp
			// 
			this->nArmDRemp->Location = System::Drawing::Point(291, 114);
			this->nArmDRemp->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDRemp->Name = L"nArmDRemp";
			this->nArmDRemp->Size = System::Drawing::Size(43, 20);
			this->nArmDRemp->TabIndex = 32;
			// 
			// label26
			// 
			this->label26->AutoSize = true;
			this->label26->Location = System::Drawing::Point(206, 116);
			this->label26->Name = L"label26";
			this->label26->Size = System::Drawing::Size(44, 13);
			this->label26->TabIndex = 31;
			this->label26->Text = L"DR EMP";
			// 
			// nArmDRelectr
			// 
			this->nArmDRelectr->Location = System::Drawing::Point(291, 96);
			this->nArmDRelectr->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDRelectr->Name = L"nArmDRelectr";
			this->nArmDRelectr->Size = System::Drawing::Size(43, 20);
			this->nArmDRelectr->TabIndex = 30;
			// 
			// label25
			// 
			this->label25->AutoSize = true;
			this->label25->Location = System::Drawing::Point(206, 98);
			this->label25->Name = L"label25";
			this->label25->Size = System::Drawing::Size(57, 13);
			this->label25->TabIndex = 29;
			this->label25->Text = L"DR Electro";
			// 
			// nArmDRplasma
			// 
			this->nArmDRplasma->Location = System::Drawing::Point(291, 78);
			this->nArmDRplasma->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDRplasma->Name = L"nArmDRplasma";
			this->nArmDRplasma->Size = System::Drawing::Size(43, 20);
			this->nArmDRplasma->TabIndex = 28;
			// 
			// label24
			// 
			this->label24->AutoSize = true;
			this->label24->Location = System::Drawing::Point(206, 80);
			this->label24->Name = L"label24";
			this->label24->Size = System::Drawing::Size(57, 13);
			this->label24->TabIndex = 27;
			this->label24->Text = L"DR Plasma";
			// 
			// nArmDRfire
			// 
			this->nArmDRfire->Location = System::Drawing::Point(291, 59);
			this->nArmDRfire->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDRfire->Name = L"nArmDRfire";
			this->nArmDRfire->Size = System::Drawing::Size(43, 20);
			this->nArmDRfire->TabIndex = 26;
			// 
			// label23
			// 
			this->label23->AutoSize = true;
			this->label23->Location = System::Drawing::Point(206, 61);
			this->label23->Name = L"label23";
			this->label23->Size = System::Drawing::Size(42, 13);
			this->label23->TabIndex = 25;
			this->label23->Text = L"DR Fire";
			// 
			// nArmDRlaser
			// 
			this->nArmDRlaser->Location = System::Drawing::Point(291, 41);
			this->nArmDRlaser->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDRlaser->Name = L"nArmDRlaser";
			this->nArmDRlaser->Size = System::Drawing::Size(43, 20);
			this->nArmDRlaser->TabIndex = 24;
			// 
			// label22
			// 
			this->label22->AutoSize = true;
			this->label22->Location = System::Drawing::Point(206, 43);
			this->label22->Name = L"label22";
			this->label22->Size = System::Drawing::Size(50, 13);
			this->label22->TabIndex = 23;
			this->label22->Text = L"DR Laser";
			this->label22->Click += gcnew System::EventHandler(this, &Form1::label22_Click);
			// 
			// nArmDRnorm
			// 
			this->nArmDRnorm->Location = System::Drawing::Point(291, 22);
			this->nArmDRnorm->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nArmDRnorm->Name = L"nArmDRnorm";
			this->nArmDRnorm->Size = System::Drawing::Size(43, 20);
			this->nArmDRnorm->TabIndex = 20;
			// 
			// nArmPerk
			// 
			this->nArmPerk->Location = System::Drawing::Point(106, 133);
			this->nArmPerk->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->nArmPerk->Name = L"nArmPerk";
			this->nArmPerk->Size = System::Drawing::Size(43, 20);
			this->nArmPerk->TabIndex = 19;
			// 
			// nArmAC
			// 
			this->nArmAC->Location = System::Drawing::Point(106, 114);
			this->nArmAC->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000, 0, 0, 0});
			this->nArmAC->Name = L"nArmAC";
			this->nArmAC->Size = System::Drawing::Size(43, 20);
			this->nArmAC->TabIndex = 18;
			// 
			// nArmTypeFemale
			// 
			this->nArmTypeFemale->Location = System::Drawing::Point(106, 23);
			this->nArmTypeFemale->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->nArmTypeFemale->Name = L"nArmTypeFemale";
			this->nArmTypeFemale->Size = System::Drawing::Size(43, 20);
			this->nArmTypeFemale->TabIndex = 15;
			// 
			// nArmTypeMale
			// 
			this->nArmTypeMale->Location = System::Drawing::Point(106, 4);
			this->nArmTypeMale->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->nArmTypeMale->Name = L"nArmTypeMale";
			this->nArmTypeMale->Size = System::Drawing::Size(43, 20);
			this->nArmTypeMale->TabIndex = 12;
			// 
			// label19
			// 
			this->label19->AutoSize = true;
			this->label19->Location = System::Drawing::Point(3, 25);
			this->label19->Name = L"label19";
			this->label19->Size = System::Drawing::Size(93, 13);
			this->label19->TabIndex = 9;
			this->label19->Text = L"Body type Female";
			// 
			// label18
			// 
			this->label18->AutoSize = true;
			this->label18->Location = System::Drawing::Point(206, 24);
			this->label18->Name = L"label18";
			this->label18->Size = System::Drawing::Size(57, 13);
			this->label18->TabIndex = 8;
			this->label18->Text = L"DR Normal";
			// 
			// label14
			// 
			this->label14->AutoSize = true;
			this->label14->Location = System::Drawing::Point(6, 135);
			this->label14->Name = L"label14";
			this->label14->Size = System::Drawing::Size(60, 13);
			this->label14->TabIndex = 4;
			this->label14->Text = L"Armor perk";
			// 
			// label12
			// 
			this->label12->AutoSize = true;
			this->label12->Location = System::Drawing::Point(6, 117);
			this->label12->Name = L"label12";
			this->label12->Size = System::Drawing::Size(62, 13);
			this->label12->TabIndex = 3;
			this->label12->Text = L"Armor class";
			// 
			// label6
			// 
			this->label6->AutoSize = true;
			this->label6->Location = System::Drawing::Point(3, 5);
			this->label6->Name = L"label6";
			this->label6->Size = System::Drawing::Size(81, 13);
			this->label6->TabIndex = 0;
			this->label6->Text = L"Body type Male";
			// 
			// tDrug
			// 
			this->tDrug->Controls->Add(this->label32);
			this->tDrug->Location = System::Drawing::Point(4, 22);
			this->tDrug->Name = L"tDrug";
			this->tDrug->Size = System::Drawing::Size(660, 169);
			this->tDrug->TabIndex = 2;
			this->tDrug->Text = L"Drug";
			this->tDrug->UseVisualStyleBackColor = true;
			this->tDrug->Click += gcnew System::EventHandler(this, &Form1::tabPage3_Click);
			// 
			// label32
			// 
			this->label32->AutoSize = true;
			this->label32->Location = System::Drawing::Point(33, 28);
			this->label32->Name = L"label32";
			this->label32->Size = System::Drawing::Size(111, 13);
			this->label32->TabIndex = 0;
			this->label32->Text = L"Look drugs.fos script.";
			// 
			// tWeapon
			// 
			this->tWeapon->Controls->Add(this->txtWeapSApic);
			this->tWeapon->Location = System::Drawing::Point(4, 22);
			this->tWeapon->Name = L"tWeapon";
			this->tWeapon->Size = System::Drawing::Size(660, 169);
			this->tWeapon->TabIndex = 3;
			this->tWeapon->Text = L"Weapon";
			this->tWeapon->UseVisualStyleBackColor = true;
			// 
			// txtWeapSApic
			// 
			this->txtWeapSApic->Controls->Add(this->cbWeapMain);
			this->txtWeapSApic->Controls->Add(this->tabPage11);
			this->txtWeapSApic->Controls->Add(this->tabPage12);
			this->txtWeapSApic->Controls->Add(this->tabPage13);
			this->txtWeapSApic->Location = System::Drawing::Point(6, 3);
			this->txtWeapSApic->Name = L"txtWeapSApic";
			this->txtWeapSApic->SelectedIndex = 0;
			this->txtWeapSApic->Size = System::Drawing::Size(651, 155);
			this->txtWeapSApic->TabIndex = 8;
			this->txtWeapSApic->SelectedIndexChanged += gcnew System::EventHandler(this, &Form1::tabControl2_SelectedIndexChanged);
			// 
			// cbWeapMain
			// 
			this->cbWeapMain->Controls->Add(this->cbWeapArmorPiercing);
			this->cbWeapMain->Controls->Add(this->label121);
			this->cbWeapMain->Controls->Add(this->numWeapUnarmedCritBonus);
			this->cbWeapMain->Controls->Add(this->numWeapUnarmedPriory);
			this->cbWeapMain->Controls->Add(this->label120);
			this->cbWeapMain->Controls->Add(this->numWeapMinUnarmed);
			this->cbWeapMain->Controls->Add(this->label119);
			this->cbWeapMain->Controls->Add(this->label118);
			this->cbWeapMain->Controls->Add(this->numWeapMinLevel);
			this->cbWeapMain->Controls->Add(this->label103);
			this->cbWeapMain->Controls->Add(this->numWeapUnarmedTree);
			this->cbWeapMain->Controls->Add(this->cbWeapUnarmed);
			this->cbWeapMain->Controls->Add(this->cbWeapIsNeedAct);
			this->cbWeapMain->Controls->Add(this->numWeapMinAg);
			this->cbWeapMain->Controls->Add(this->label108);
			this->cbWeapMain->Controls->Add(this->numWeapMinSt);
			this->cbWeapMain->Controls->Add(this->label102);
			this->cbWeapMain->Controls->Add(this->numWeapPerk);
			this->cbWeapMain->Controls->Add(this->label105);
			this->cbWeapMain->Controls->Add(this->numWeapDefAmmo);
			this->cbWeapMain->Controls->Add(this->label99);
			this->cbWeapMain->Controls->Add(this->cbWeapNoWear);
			this->cbWeapMain->Controls->Add(this->label40);
			this->cbWeapMain->Controls->Add(this->nWeapCrFail);
			this->cbWeapMain->Controls->Add(this->label47);
			this->cbWeapMain->Controls->Add(this->nWeapAnim1);
			this->cbWeapMain->Controls->Add(this->nWeapCaliber);
			this->cbWeapMain->Controls->Add(this->nWeapHolder);
			this->cbWeapMain->Controls->Add(this->label44);
			this->cbWeapMain->Controls->Add(this->label46);
			this->cbWeapMain->Location = System::Drawing::Point(4, 22);
			this->cbWeapMain->Name = L"cbWeapMain";
			this->cbWeapMain->Size = System::Drawing::Size(643, 129);
			this->cbWeapMain->TabIndex = 3;
			this->cbWeapMain->Text = L"Main";
			this->cbWeapMain->UseVisualStyleBackColor = true;
			// 
			// cbWeapArmorPiercing
			// 
			this->cbWeapArmorPiercing->AutoSize = true;
			this->cbWeapArmorPiercing->Location = System::Drawing::Point(569, 63);
			this->cbWeapArmorPiercing->Name = L"cbWeapArmorPiercing";
			this->cbWeapArmorPiercing->Size = System::Drawing::Size(63, 30);
			this->cbWeapArmorPiercing->TabIndex = 69;
			this->cbWeapArmorPiercing->Text = L"Armor\r\npiercing";
			this->cbWeapArmorPiercing->UseVisualStyleBackColor = true;
			// 
			// label121
			// 
			this->label121->AutoSize = true;
			this->label121->Location = System::Drawing::Point(566, 21);
			this->label121->Name = L"label121";
			this->label121->Size = System::Drawing::Size(71, 13);
			this->label121->TabIndex = 68;
			this->label121->Text = L"Critical bonus";
			// 
			// numWeapUnarmedCritBonus
			// 
			this->numWeapUnarmedCritBonus->Location = System::Drawing::Point(593, 37);
			this->numWeapUnarmedCritBonus->Name = L"numWeapUnarmedCritBonus";
			this->numWeapUnarmedCritBonus->Size = System::Drawing::Size(47, 20);
			this->numWeapUnarmedCritBonus->TabIndex = 67;
			// 
			// numWeapUnarmedPriory
			// 
			this->numWeapUnarmedPriory->Location = System::Drawing::Point(513, 44);
			this->numWeapUnarmedPriory->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {200, 0, 0, 0});
			this->numWeapUnarmedPriory->Name = L"numWeapUnarmedPriory";
			this->numWeapUnarmedPriory->Size = System::Drawing::Size(47, 20);
			this->numWeapUnarmedPriory->TabIndex = 66;
			// 
			// label120
			// 
			this->label120->AutoSize = true;
			this->label120->Location = System::Drawing::Point(423, 47);
			this->label120->Name = L"label120";
			this->label120->Size = System::Drawing::Size(81, 13);
			this->label120->TabIndex = 65;
			this->label120->Text = L"Unarmed priory";
			// 
			// numWeapMinUnarmed
			// 
			this->numWeapMinUnarmed->Location = System::Drawing::Point(513, 85);
			this->numWeapMinUnarmed->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {200, 0, 0, 0});
			this->numWeapMinUnarmed->Name = L"numWeapMinUnarmed";
			this->numWeapMinUnarmed->Size = System::Drawing::Size(47, 20);
			this->numWeapMinUnarmed->TabIndex = 64;
			// 
			// label119
			// 
			this->label119->AutoSize = true;
			this->label119->Location = System::Drawing::Point(423, 108);
			this->label119->Name = L"label119";
			this->label119->Size = System::Drawing::Size(51, 13);
			this->label119->TabIndex = 63;
			this->label119->Text = L"Min Level";
			// 
			// label118
			// 
			this->label118->AutoSize = true;
			this->label118->Location = System::Drawing::Point(423, 87);
			this->label118->Name = L"label118";
			this->label118->Size = System::Drawing::Size(69, 13);
			this->label118->TabIndex = 62;
			this->label118->Text = L"Min Unarmed";
			// 
			// numWeapMinLevel
			// 
			this->numWeapMinLevel->Location = System::Drawing::Point(513, 106);
			this->numWeapMinLevel->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {9999, 0, 0, 0});
			this->numWeapMinLevel->Name = L"numWeapMinLevel";
			this->numWeapMinLevel->Size = System::Drawing::Size(47, 20);
			this->numWeapMinLevel->TabIndex = 61;
			// 
			// label103
			// 
			this->label103->AutoSize = true;
			this->label103->Location = System::Drawing::Point(423, 29);
			this->label103->Name = L"label103";
			this->label103->Size = System::Drawing::Size(73, 13);
			this->label103->TabIndex = 59;
			this->label103->Text = L"Unarmed tree";
			// 
			// numWeapUnarmedTree
			// 
			this->numWeapUnarmedTree->Location = System::Drawing::Point(513, 24);
			this->numWeapUnarmedTree->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {200, 0, 0, 0});
			this->numWeapUnarmedTree->Name = L"numWeapUnarmedTree";
			this->numWeapUnarmedTree->Size = System::Drawing::Size(47, 20);
			this->numWeapUnarmedTree->TabIndex = 58;
			this->numWeapUnarmedTree->ValueChanged += gcnew System::EventHandler(this, &Form1::numUnarmedTree_ValueChanged);
			// 
			// cbWeapUnarmed
			// 
			this->cbWeapUnarmed->AutoSize = true;
			this->cbWeapUnarmed->Location = System::Drawing::Point(458, 5);
			this->cbWeapUnarmed->Name = L"cbWeapUnarmed";
			this->cbWeapUnarmed->Size = System::Drawing::Size(102, 17);
			this->cbWeapUnarmed->TabIndex = 57;
			this->cbWeapUnarmed->Text = L"Unarmed attack";
			this->cbWeapUnarmed->UseVisualStyleBackColor = true;
			// 
			// cbWeapIsNeedAct
			// 
			this->cbWeapIsNeedAct->AutoSize = true;
			this->cbWeapIsNeedAct->Location = System::Drawing::Point(9, 78);
			this->cbWeapIsNeedAct->Name = L"cbWeapIsNeedAct";
			this->cbWeapIsNeedAct->Size = System::Drawing::Size(150, 17);
			this->cbWeapIsNeedAct->TabIndex = 56;
			this->cbWeapIsNeedAct->Text = L"IsNeedAct (for animation)";
			this->cbWeapIsNeedAct->UseVisualStyleBackColor = true;
			// 
			// numWeapMinAg
			// 
			this->numWeapMinAg->Location = System::Drawing::Point(513, 64);
			this->numWeapMinAg->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {200, 0, 0, 0});
			this->numWeapMinAg->Name = L"numWeapMinAg";
			this->numWeapMinAg->Size = System::Drawing::Size(47, 20);
			this->numWeapMinAg->TabIndex = 55;
			// 
			// label108
			// 
			this->label108->AutoSize = true;
			this->label108->Location = System::Drawing::Point(423, 66);
			this->label108->Name = L"label108";
			this->label108->Size = System::Drawing::Size(55, 13);
			this->label108->TabIndex = 54;
			this->label108->Text = L"Min Agility";
			// 
			// numWeapMinSt
			// 
			this->numWeapMinSt->Location = System::Drawing::Point(345, 69);
			this->numWeapMinSt->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {200, 0, 0, 0});
			this->numWeapMinSt->Name = L"numWeapMinSt";
			this->numWeapMinSt->Size = System::Drawing::Size(47, 20);
			this->numWeapMinSt->TabIndex = 53;
			// 
			// label102
			// 
			this->label102->AutoSize = true;
			this->label102->Location = System::Drawing::Point(219, 71);
			this->label102->Name = L"label102";
			this->label102->Size = System::Drawing::Size(68, 13);
			this->label102->TabIndex = 52;
			this->label102->Text = L"Min Strenght";
			// 
			// numWeapPerk
			// 
			this->numWeapPerk->Location = System::Drawing::Point(132, 47);
			this->numWeapPerk->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {10000000, 0, 0, 0});
			this->numWeapPerk->Name = L"numWeapPerk";
			this->numWeapPerk->Size = System::Drawing::Size(47, 20);
			this->numWeapPerk->TabIndex = 51;
			// 
			// label105
			// 
			this->label105->AutoSize = true;
			this->label105->Location = System::Drawing::Point(6, 49);
			this->label105->Name = L"label105";
			this->label105->Size = System::Drawing::Size(28, 13);
			this->label105->TabIndex = 50;
			this->label105->Text = L"Perk";
			// 
			// numWeapDefAmmo
			// 
			this->numWeapDefAmmo->Location = System::Drawing::Point(345, 42);
			this->numWeapDefAmmo->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->numWeapDefAmmo->Name = L"numWeapDefAmmo";
			this->numWeapDefAmmo->Size = System::Drawing::Size(47, 20);
			this->numWeapDefAmmo->TabIndex = 48;
			// 
			// label99
			// 
			this->label99->AutoSize = true;
			this->label99->Location = System::Drawing::Point(219, 44);
			this->label99->Name = L"label99";
			this->label99->Size = System::Drawing::Size(73, 13);
			this->label99->TabIndex = 46;
			this->label99->Text = L"Default ammo";
			// 
			// cbWeapNoWear
			// 
			this->cbWeapNoWear->AutoSize = true;
			this->cbWeapNoWear->Location = System::Drawing::Point(9, 94);
			this->cbWeapNoWear->Name = L"cbWeapNoWear";
			this->cbWeapNoWear->Size = System::Drawing::Size(116, 17);
			this->cbWeapNoWear->TabIndex = 45;
			this->cbWeapNoWear->Text = L"NoWear (grouped)";
			this->cbWeapNoWear->UseVisualStyleBackColor = true;
			// 
			// label40
			// 
			this->label40->AutoSize = true;
			this->label40->Location = System::Drawing::Point(6, 8);
			this->label40->Name = L"label40";
			this->label40->Size = System::Drawing::Size(83, 13);
			this->label40->TabIndex = 0;
			this->label40->Text = L"Animation index";
			// 
			// nWeapCrFail
			// 
			this->nWeapCrFail->Location = System::Drawing::Point(132, 27);
			this->nWeapCrFail->Name = L"nWeapCrFail";
			this->nWeapCrFail->Size = System::Drawing::Size(47, 20);
			this->nWeapCrFail->TabIndex = 44;
			// 
			// label47
			// 
			this->label47->AutoSize = true;
			this->label47->Location = System::Drawing::Point(6, 29);
			this->label47->Name = L"label47";
			this->label47->Size = System::Drawing::Size(56, 13);
			this->label47->TabIndex = 7;
			this->label47->Text = L"Critical fail";
			// 
			// nWeapAnim1
			// 
			this->nWeapAnim1->Location = System::Drawing::Point(132, 6);
			this->nWeapAnim1->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->nWeapAnim1->Name = L"nWeapAnim1";
			this->nWeapAnim1->Size = System::Drawing::Size(47, 20);
			this->nWeapAnim1->TabIndex = 37;
			// 
			// nWeapCaliber
			// 
			this->nWeapCaliber->Location = System::Drawing::Point(345, 24);
			this->nWeapCaliber->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {10000000, 0, 0, 0});
			this->nWeapCaliber->Name = L"nWeapCaliber";
			this->nWeapCaliber->Size = System::Drawing::Size(47, 20);
			this->nWeapCaliber->TabIndex = 41;
			this->nWeapCaliber->ValueChanged += gcnew System::EventHandler(this, &Form1::nWeapCaliber_ValueChanged);
			// 
			// nWeapHolder
			// 
			this->nWeapHolder->Location = System::Drawing::Point(345, 5);
			this->nWeapHolder->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapHolder->Name = L"nWeapHolder";
			this->nWeapHolder->Size = System::Drawing::Size(47, 20);
			this->nWeapHolder->TabIndex = 40;
			// 
			// label44
			// 
			this->label44->AutoSize = true;
			this->label44->Location = System::Drawing::Point(219, 8);
			this->label44->Name = L"label44";
			this->label44->Size = System::Drawing::Size(75, 13);
			this->label44->TabIndex = 4;
			this->label44->Text = L"Holder volume";
			// 
			// label46
			// 
			this->label46->AutoSize = true;
			this->label46->Location = System::Drawing::Point(219, 27);
			this->label46->Name = L"label46";
			this->label46->Size = System::Drawing::Size(113, 13);
			this->label46->TabIndex = 6;
			this->label46->Text = L"Caliber (look in Ammo)";
			// 
			// tabPage11
			// 
			this->tabPage11->Controls->Add(this->txtWeapPApic);
			this->tabPage11->Controls->Add(this->label41);
			this->tabPage11->Controls->Add(this->txtWeapPASoundId);
			this->tabPage11->Controls->Add(this->label104);
			this->tabPage11->Controls->Add(this->nWeapPAtime);
			this->tabPage11->Controls->Add(this->label100);
			this->tabPage11->Controls->Add(this->nWeapPAEffect);
			this->tabPage11->Controls->Add(this->nWeapPAdistmax);
			this->tabPage11->Controls->Add(this->label66);
			this->tabPage11->Controls->Add(this->label65);
			this->tabPage11->Controls->Add(this->nWeapPAdmgmin);
			this->tabPage11->Controls->Add(this->nWeapPAdmgmax);
			this->tabPage11->Controls->Add(this->label64);
			this->tabPage11->Controls->Add(this->label63);
			this->tabPage11->Controls->Add(this->nWeapPAanim2);
			this->tabPage11->Controls->Add(this->label62);
			this->tabPage11->Controls->Add(this->cbWeapPAremove);
			this->tabPage11->Controls->Add(this->cbWeapPAaim);
			this->tabPage11->Controls->Add(this->label61);
			this->tabPage11->Controls->Add(this->nWeapPAround);
			this->tabPage11->Controls->Add(this->label60);
			this->tabPage11->Controls->Add(this->cmbWeapPAtypeattack);
			this->tabPage11->Controls->Add(this->label58);
			this->tabPage11->Controls->Add(this->nWeapPAskill);
			this->tabPage11->Controls->Add(this->label57);
			this->tabPage11->Controls->Add(this->cbWeapPA);
			this->tabPage11->Location = System::Drawing::Point(4, 22);
			this->tabPage11->Name = L"tabPage11";
			this->tabPage11->Padding = System::Windows::Forms::Padding(3);
			this->tabPage11->Size = System::Drawing::Size(643, 129);
			this->tabPage11->TabIndex = 0;
			this->tabPage11->Text = L"Attack 1";
			this->tabPage11->UseVisualStyleBackColor = true;
			// 
			// txtWeapPApic
			// 
			this->txtWeapPApic->Location = System::Drawing::Point(288, 21);
			this->txtWeapPApic->Name = L"txtWeapPApic";
			this->txtWeapPApic->Size = System::Drawing::Size(144, 20);
			this->txtWeapPApic->TabIndex = 61;
			// 
			// label41
			// 
			this->label41->AutoSize = true;
			this->label41->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 80, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->label41->ForeColor = System::Drawing::Color::OliveDrab;
			this->label41->Location = System::Drawing::Point(526, 3);
			this->label41->Name = L"label41";
			this->label41->Size = System::Drawing::Size(111, 120);
			this->label41->TabIndex = 60;
			this->label41->Text = L"1";
			// 
			// txtWeapPASoundId
			// 
			this->txtWeapPASoundId->Location = System::Drawing::Point(240, 43);
			this->txtWeapPASoundId->MaxLength = 1;
			this->txtWeapPASoundId->Name = L"txtWeapPASoundId";
			this->txtWeapPASoundId->Size = System::Drawing::Size(32, 20);
			this->txtWeapPASoundId->TabIndex = 59;
			// 
			// label104
			// 
			this->label104->AutoSize = true;
			this->label104->Location = System::Drawing::Point(187, 46);
			this->label104->Name = L"label104";
			this->label104->Size = System::Drawing::Size(47, 13);
			this->label104->TabIndex = 58;
			this->label104->Text = L"SoundId";
			// 
			// nWeapPAtime
			// 
			this->nWeapPAtime->Location = System::Drawing::Point(375, 63);
			this->nWeapPAtime->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {10000000, 0, 0, 0});
			this->nWeapPAtime->Name = L"nWeapPAtime";
			this->nWeapPAtime->Size = System::Drawing::Size(57, 20);
			this->nWeapPAtime->TabIndex = 55;
			// 
			// label100
			// 
			this->label100->AutoSize = true;
			this->label100->Location = System::Drawing::Point(294, 68);
			this->label100->Name = L"label100";
			this->label100->Size = System::Drawing::Size(54, 13);
			this->label100->TabIndex = 54;
			this->label100->Text = L"Attack AP";
			// 
			// nWeapPAEffect
			// 
			this->nWeapPAEffect->Location = System::Drawing::Point(375, 103);
			this->nWeapPAEffect->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapPAEffect->Name = L"nWeapPAEffect";
			this->nWeapPAEffect->Size = System::Drawing::Size(57, 20);
			this->nWeapPAEffect->TabIndex = 51;
			// 
			// nWeapPAdistmax
			// 
			this->nWeapPAdistmax->Location = System::Drawing::Point(375, 84);
			this->nWeapPAdistmax->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->nWeapPAdistmax->Name = L"nWeapPAdistmax";
			this->nWeapPAdistmax->Size = System::Drawing::Size(47, 20);
			this->nWeapPAdistmax->TabIndex = 50;
			// 
			// label66
			// 
			this->label66->AutoSize = true;
			this->label66->Location = System::Drawing::Point(294, 105);
			this->label66->Name = L"label66";
			this->label66->Size = System::Drawing::Size(53, 13);
			this->label66->TabIndex = 49;
			this->label66->Text = L"Fly effect";
			// 
			// label65
			// 
			this->label65->AutoSize = true;
			this->label65->Location = System::Drawing::Point(294, 86);
			this->label65->Name = L"label65";
			this->label65->Size = System::Drawing::Size(48, 13);
			this->label65->TabIndex = 48;
			this->label65->Text = L"Distance";
			// 
			// nWeapPAdmgmin
			// 
			this->nWeapPAdmgmin->Location = System::Drawing::Point(225, 103);
			this->nWeapPAdmgmin->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapPAdmgmin->Name = L"nWeapPAdmgmin";
			this->nWeapPAdmgmin->Size = System::Drawing::Size(47, 20);
			this->nWeapPAdmgmin->TabIndex = 47;
			// 
			// nWeapPAdmgmax
			// 
			this->nWeapPAdmgmax->Location = System::Drawing::Point(225, 84);
			this->nWeapPAdmgmax->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapPAdmgmax->Name = L"nWeapPAdmgmax";
			this->nWeapPAdmgmax->Size = System::Drawing::Size(47, 20);
			this->nWeapPAdmgmax->TabIndex = 46;
			// 
			// label64
			// 
			this->label64->AutoSize = true;
			this->label64->Location = System::Drawing::Point(153, 105);
			this->label64->Name = L"label64";
			this->label64->Size = System::Drawing::Size(65, 13);
			this->label64->TabIndex = 45;
			this->label64->Text = L"Damage min";
			// 
			// label63
			// 
			this->label63->AutoSize = true;
			this->label63->Location = System::Drawing::Point(153, 86);
			this->label63->Name = L"label63";
			this->label63->Size = System::Drawing::Size(69, 13);
			this->label63->TabIndex = 44;
			this->label63->Text = L"Damage max";
			// 
			// nWeapPAanim2
			// 
			this->nWeapPAanim2->Location = System::Drawing::Point(100, 103);
			this->nWeapPAanim2->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapPAanim2->Name = L"nWeapPAanim2";
			this->nWeapPAanim2->Size = System::Drawing::Size(47, 20);
			this->nWeapPAanim2->TabIndex = 43;
			// 
			// label62
			// 
			this->label62->AutoSize = true;
			this->label62->Location = System::Drawing::Point(2, 105);
			this->label62->Name = L"label62";
			this->label62->Size = System::Drawing::Size(83, 13);
			this->label62->TabIndex = 42;
			this->label62->Text = L"Animation index";
			// 
			// cbWeapPAremove
			// 
			this->cbWeapPAremove->AutoSize = true;
			this->cbWeapPAremove->Location = System::Drawing::Point(99, 25);
			this->cbWeapPAremove->Name = L"cbWeapPAremove";
			this->cbWeapPAremove->Size = System::Drawing::Size(125, 17);
			this->cbWeapPAremove->TabIndex = 40;
			this->cbWeapPAremove->Text = L"Remove after attack";
			this->cbWeapPAremove->UseVisualStyleBackColor = true;
			// 
			// cbWeapPAaim
			// 
			this->cbWeapPAaim->AutoSize = true;
			this->cbWeapPAaim->Location = System::Drawing::Point(99, 5);
			this->cbWeapPAaim->Name = L"cbWeapPAaim";
			this->cbWeapPAaim->Size = System::Drawing::Size(80, 17);
			this->cbWeapPAaim->TabIndex = 39;
			this->cbWeapPAaim->Text = L"Aim aviable";
			this->cbWeapPAaim->UseVisualStyleBackColor = true;
			// 
			// label61
			// 
			this->label61->AutoSize = true;
			this->label61->Location = System::Drawing::Point(294, 5);
			this->label61->Name = L"label61";
			this->label61->Size = System::Drawing::Size(61, 13);
			this->label61->TabIndex = 38;
			this->label61->Text = L"Use picture";
			// 
			// nWeapPAround
			// 
			this->nWeapPAround->Location = System::Drawing::Point(100, 84);
			this->nWeapPAround->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapPAround->Name = L"nWeapPAround";
			this->nWeapPAround->Size = System::Drawing::Size(47, 20);
			this->nWeapPAround->TabIndex = 36;
			// 
			// label60
			// 
			this->label60->AutoSize = true;
			this->label60->Location = System::Drawing::Point(2, 86);
			this->label60->Name = L"label60";
			this->label60->Size = System::Drawing::Size(69, 13);
			this->label60->TabIndex = 35;
			this->label60->Text = L"Bullets round";
			// 
			// cmbWeapPAtypeattack
			// 
			this->cmbWeapPAtypeattack->DisplayMember = L"0";
			this->cmbWeapPAtypeattack->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
			this->cmbWeapPAtypeattack->FormattingEnabled = true;
			this->cmbWeapPAtypeattack->Items->AddRange(gcnew cli::array< System::Object^  >(7) {L"Нормальная", L"Лазер", L"Огонь", L"Плазма", 
				L"Электричество", L"EMP", L"Взрыв"});
			this->cmbWeapPAtypeattack->Location = System::Drawing::Point(79, 43);
			this->cmbWeapPAtypeattack->Name = L"cmbWeapPAtypeattack";
			this->cmbWeapPAtypeattack->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->cmbWeapPAtypeattack->Size = System::Drawing::Size(99, 21);
			this->cmbWeapPAtypeattack->TabIndex = 30;
			// 
			// label58
			// 
			this->label58->AutoSize = true;
			this->label58->Location = System::Drawing::Point(2, 46);
			this->label58->Name = L"label58";
			this->label58->Size = System::Drawing::Size(71, 13);
			this->label58->TabIndex = 29;
			this->label58->Text = L"Damage type";
			// 
			// nWeapPAskill
			// 
			this->nWeapPAskill->Location = System::Drawing::Point(46, 23);
			this->nWeapPAskill->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->nWeapPAskill->Name = L"nWeapPAskill";
			this->nWeapPAskill->Size = System::Drawing::Size(47, 20);
			this->nWeapPAskill->TabIndex = 28;
			// 
			// label57
			// 
			this->label57->AutoSize = true;
			this->label57->Location = System::Drawing::Point(2, 25);
			this->label57->Name = L"label57";
			this->label57->Size = System::Drawing::Size(24, 13);
			this->label57->TabIndex = 27;
			this->label57->Text = L"Skill";
			// 
			// cbWeapPA
			// 
			this->cbWeapPA->AutoSize = true;
			this->cbWeapPA->Location = System::Drawing::Point(2, 5);
			this->cbWeapPA->Name = L"cbWeapPA";
			this->cbWeapPA->Size = System::Drawing::Size(56, 17);
			this->cbWeapPA->TabIndex = 26;
			this->cbWeapPA->Text = L"Active";
			this->cbWeapPA->UseVisualStyleBackColor = true;
			// 
			// tabPage12
			// 
			this->tabPage12->Controls->Add(this->textBox3);
			this->tabPage12->Controls->Add(this->label42);
			this->tabPage12->Controls->Add(this->txtWeapSASoundId);
			this->tabPage12->Controls->Add(this->label106);
			this->tabPage12->Controls->Add(this->nWeapSAtime);
			this->tabPage12->Controls->Add(this->label53);
			this->tabPage12->Controls->Add(this->nWeapSAEffect);
			this->tabPage12->Controls->Add(this->nWeapSAdistmax);
			this->tabPage12->Controls->Add(this->label67);
			this->tabPage12->Controls->Add(this->label68);
			this->tabPage12->Controls->Add(this->nWeapSAdmgmin);
			this->tabPage12->Controls->Add(this->nWeapSAdmgmax);
			this->tabPage12->Controls->Add(this->label69);
			this->tabPage12->Controls->Add(this->label70);
			this->tabPage12->Controls->Add(this->nWeapSAanim2);
			this->tabPage12->Controls->Add(this->label71);
			this->tabPage12->Controls->Add(this->cbWeapSAremove);
			this->tabPage12->Controls->Add(this->cbWeapSAaim);
			this->tabPage12->Controls->Add(this->label72);
			this->tabPage12->Controls->Add(this->nWeapSAround);
			this->tabPage12->Controls->Add(this->label73);
			this->tabPage12->Controls->Add(this->cmbWeapSAtypeattack);
			this->tabPage12->Controls->Add(this->label75);
			this->tabPage12->Controls->Add(this->nWeapSAskill);
			this->tabPage12->Controls->Add(this->label76);
			this->tabPage12->Controls->Add(this->cbWeapSA);
			this->tabPage12->Location = System::Drawing::Point(4, 22);
			this->tabPage12->Name = L"tabPage12";
			this->tabPage12->Padding = System::Windows::Forms::Padding(3);
			this->tabPage12->Size = System::Drawing::Size(643, 129);
			this->tabPage12->TabIndex = 1;
			this->tabPage12->Text = L"Attack 2";
			this->tabPage12->UseVisualStyleBackColor = true;
			// 
			// textBox3
			// 
			this->textBox3->Location = System::Drawing::Point(288, 21);
			this->textBox3->Name = L"textBox3";
			this->textBox3->Size = System::Drawing::Size(144, 20);
			this->textBox3->TabIndex = 62;
			// 
			// label42
			// 
			this->label42->AutoSize = true;
			this->label42->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 80, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->label42->ForeColor = System::Drawing::Color::OliveDrab;
			this->label42->Location = System::Drawing::Point(526, 5);
			this->label42->Name = L"label42";
			this->label42->Size = System::Drawing::Size(111, 120);
			this->label42->TabIndex = 61;
			this->label42->Text = L"2";
			// 
			// txtWeapSASoundId
			// 
			this->txtWeapSASoundId->Location = System::Drawing::Point(240, 43);
			this->txtWeapSASoundId->MaxLength = 1;
			this->txtWeapSASoundId->Name = L"txtWeapSASoundId";
			this->txtWeapSASoundId->Size = System::Drawing::Size(32, 20);
			this->txtWeapSASoundId->TabIndex = 59;
			// 
			// label106
			// 
			this->label106->AutoSize = true;
			this->label106->Location = System::Drawing::Point(187, 46);
			this->label106->Name = L"label106";
			this->label106->Size = System::Drawing::Size(47, 13);
			this->label106->TabIndex = 58;
			this->label106->Text = L"SoundId";
			// 
			// nWeapSAtime
			// 
			this->nWeapSAtime->Location = System::Drawing::Point(375, 63);
			this->nWeapSAtime->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {10000000, 0, 0, 0});
			this->nWeapSAtime->Name = L"nWeapSAtime";
			this->nWeapSAtime->Size = System::Drawing::Size(57, 20);
			this->nWeapSAtime->TabIndex = 55;
			// 
			// label53
			// 
			this->label53->AutoSize = true;
			this->label53->Location = System::Drawing::Point(294, 68);
			this->label53->Name = L"label53";
			this->label53->Size = System::Drawing::Size(54, 13);
			this->label53->TabIndex = 54;
			this->label53->Text = L"Attack AP";
			// 
			// nWeapSAEffect
			// 
			this->nWeapSAEffect->Location = System::Drawing::Point(375, 103);
			this->nWeapSAEffect->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapSAEffect->Name = L"nWeapSAEffect";
			this->nWeapSAEffect->Size = System::Drawing::Size(57, 20);
			this->nWeapSAEffect->TabIndex = 51;
			// 
			// nWeapSAdistmax
			// 
			this->nWeapSAdistmax->Location = System::Drawing::Point(375, 84);
			this->nWeapSAdistmax->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapSAdistmax->Name = L"nWeapSAdistmax";
			this->nWeapSAdistmax->Size = System::Drawing::Size(47, 20);
			this->nWeapSAdistmax->TabIndex = 50;
			// 
			// label67
			// 
			this->label67->AutoSize = true;
			this->label67->Location = System::Drawing::Point(294, 105);
			this->label67->Name = L"label67";
			this->label67->Size = System::Drawing::Size(53, 13);
			this->label67->TabIndex = 49;
			this->label67->Text = L"Fly effect";
			// 
			// label68
			// 
			this->label68->AutoSize = true;
			this->label68->Location = System::Drawing::Point(294, 86);
			this->label68->Name = L"label68";
			this->label68->Size = System::Drawing::Size(48, 13);
			this->label68->TabIndex = 48;
			this->label68->Text = L"Distance";
			// 
			// nWeapSAdmgmin
			// 
			this->nWeapSAdmgmin->Location = System::Drawing::Point(225, 103);
			this->nWeapSAdmgmin->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapSAdmgmin->Name = L"nWeapSAdmgmin";
			this->nWeapSAdmgmin->Size = System::Drawing::Size(47, 20);
			this->nWeapSAdmgmin->TabIndex = 47;
			// 
			// nWeapSAdmgmax
			// 
			this->nWeapSAdmgmax->Location = System::Drawing::Point(225, 84);
			this->nWeapSAdmgmax->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapSAdmgmax->Name = L"nWeapSAdmgmax";
			this->nWeapSAdmgmax->Size = System::Drawing::Size(47, 20);
			this->nWeapSAdmgmax->TabIndex = 46;
			// 
			// label69
			// 
			this->label69->AutoSize = true;
			this->label69->Location = System::Drawing::Point(153, 105);
			this->label69->Name = L"label69";
			this->label69->Size = System::Drawing::Size(65, 13);
			this->label69->TabIndex = 45;
			this->label69->Text = L"Damage min";
			// 
			// label70
			// 
			this->label70->AutoSize = true;
			this->label70->Location = System::Drawing::Point(153, 86);
			this->label70->Name = L"label70";
			this->label70->Size = System::Drawing::Size(69, 13);
			this->label70->TabIndex = 44;
			this->label70->Text = L"Damage max";
			// 
			// nWeapSAanim2
			// 
			this->nWeapSAanim2->Location = System::Drawing::Point(100, 103);
			this->nWeapSAanim2->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapSAanim2->Name = L"nWeapSAanim2";
			this->nWeapSAanim2->Size = System::Drawing::Size(47, 20);
			this->nWeapSAanim2->TabIndex = 43;
			// 
			// label71
			// 
			this->label71->AutoSize = true;
			this->label71->Location = System::Drawing::Point(2, 105);
			this->label71->Name = L"label71";
			this->label71->Size = System::Drawing::Size(83, 13);
			this->label71->TabIndex = 42;
			this->label71->Text = L"Animation index";
			// 
			// cbWeapSAremove
			// 
			this->cbWeapSAremove->AutoSize = true;
			this->cbWeapSAremove->Location = System::Drawing::Point(99, 25);
			this->cbWeapSAremove->Name = L"cbWeapSAremove";
			this->cbWeapSAremove->Size = System::Drawing::Size(125, 17);
			this->cbWeapSAremove->TabIndex = 40;
			this->cbWeapSAremove->Text = L"Remove after attack";
			this->cbWeapSAremove->UseVisualStyleBackColor = true;
			// 
			// cbWeapSAaim
			// 
			this->cbWeapSAaim->AutoSize = true;
			this->cbWeapSAaim->Location = System::Drawing::Point(99, 5);
			this->cbWeapSAaim->Name = L"cbWeapSAaim";
			this->cbWeapSAaim->Size = System::Drawing::Size(80, 17);
			this->cbWeapSAaim->TabIndex = 39;
			this->cbWeapSAaim->Text = L"Aim aviable";
			this->cbWeapSAaim->UseVisualStyleBackColor = true;
			// 
			// label72
			// 
			this->label72->AutoSize = true;
			this->label72->Location = System::Drawing::Point(294, 5);
			this->label72->Name = L"label72";
			this->label72->Size = System::Drawing::Size(61, 13);
			this->label72->TabIndex = 38;
			this->label72->Text = L"Use picture";
			// 
			// nWeapSAround
			// 
			this->nWeapSAround->Location = System::Drawing::Point(100, 84);
			this->nWeapSAround->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapSAround->Name = L"nWeapSAround";
			this->nWeapSAround->Size = System::Drawing::Size(47, 20);
			this->nWeapSAround->TabIndex = 36;
			// 
			// label73
			// 
			this->label73->AutoSize = true;
			this->label73->Location = System::Drawing::Point(2, 86);
			this->label73->Name = L"label73";
			this->label73->Size = System::Drawing::Size(69, 13);
			this->label73->TabIndex = 35;
			this->label73->Text = L"Bullets round";
			// 
			// cmbWeapSAtypeattack
			// 
			this->cmbWeapSAtypeattack->DisplayMember = L"0";
			this->cmbWeapSAtypeattack->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
			this->cmbWeapSAtypeattack->FormattingEnabled = true;
			this->cmbWeapSAtypeattack->Items->AddRange(gcnew cli::array< System::Object^  >(7) {L"Нормальная", L"Лазер", L"Огонь", L"Плазма", 
				L"Электричество", L"EMP", L"Взрыв"});
			this->cmbWeapSAtypeattack->Location = System::Drawing::Point(79, 43);
			this->cmbWeapSAtypeattack->Name = L"cmbWeapSAtypeattack";
			this->cmbWeapSAtypeattack->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->cmbWeapSAtypeattack->Size = System::Drawing::Size(99, 21);
			this->cmbWeapSAtypeattack->TabIndex = 30;
			// 
			// label75
			// 
			this->label75->AutoSize = true;
			this->label75->Location = System::Drawing::Point(2, 46);
			this->label75->Name = L"label75";
			this->label75->Size = System::Drawing::Size(71, 13);
			this->label75->TabIndex = 29;
			this->label75->Text = L"Damage type";
			// 
			// nWeapSAskill
			// 
			this->nWeapSAskill->Location = System::Drawing::Point(46, 23);
			this->nWeapSAskill->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->nWeapSAskill->Name = L"nWeapSAskill";
			this->nWeapSAskill->Size = System::Drawing::Size(47, 20);
			this->nWeapSAskill->TabIndex = 28;
			// 
			// label76
			// 
			this->label76->AutoSize = true;
			this->label76->Location = System::Drawing::Point(2, 25);
			this->label76->Name = L"label76";
			this->label76->Size = System::Drawing::Size(24, 13);
			this->label76->TabIndex = 27;
			this->label76->Text = L"Skill";
			// 
			// cbWeapSA
			// 
			this->cbWeapSA->AutoSize = true;
			this->cbWeapSA->Location = System::Drawing::Point(2, 5);
			this->cbWeapSA->Name = L"cbWeapSA";
			this->cbWeapSA->Size = System::Drawing::Size(56, 17);
			this->cbWeapSA->TabIndex = 26;
			this->cbWeapSA->Text = L"Active";
			this->cbWeapSA->UseVisualStyleBackColor = true;
			// 
			// tabPage13
			// 
			this->tabPage13->Controls->Add(this->txtWeapTApic);
			this->tabPage13->Controls->Add(this->label43);
			this->tabPage13->Controls->Add(this->txtWeapTASoundId);
			this->tabPage13->Controls->Add(this->label107);
			this->tabPage13->Controls->Add(this->nWeapTAtime);
			this->tabPage13->Controls->Add(this->label54);
			this->tabPage13->Controls->Add(this->nWeapTAEffect);
			this->tabPage13->Controls->Add(this->nWeapTAdistmax);
			this->tabPage13->Controls->Add(this->label77);
			this->tabPage13->Controls->Add(this->label78);
			this->tabPage13->Controls->Add(this->nWeapTAdmgmin);
			this->tabPage13->Controls->Add(this->nWeapTAdmgmax);
			this->tabPage13->Controls->Add(this->label79);
			this->tabPage13->Controls->Add(this->label80);
			this->tabPage13->Controls->Add(this->nWeapTAanim2);
			this->tabPage13->Controls->Add(this->label81);
			this->tabPage13->Controls->Add(this->cbWeapTAremove);
			this->tabPage13->Controls->Add(this->cbWeapTAaim);
			this->tabPage13->Controls->Add(this->label82);
			this->tabPage13->Controls->Add(this->nWeapTAround);
			this->tabPage13->Controls->Add(this->label83);
			this->tabPage13->Controls->Add(this->cmbWeapTAtypeattack);
			this->tabPage13->Controls->Add(this->label85);
			this->tabPage13->Controls->Add(this->nWeapTAskill);
			this->tabPage13->Controls->Add(this->label86);
			this->tabPage13->Controls->Add(this->cbWeapTA);
			this->tabPage13->Location = System::Drawing::Point(4, 22);
			this->tabPage13->Name = L"tabPage13";
			this->tabPage13->Size = System::Drawing::Size(643, 129);
			this->tabPage13->TabIndex = 2;
			this->tabPage13->Text = L"Attack 3";
			this->tabPage13->UseVisualStyleBackColor = true;
			// 
			// txtWeapTApic
			// 
			this->txtWeapTApic->Location = System::Drawing::Point(288, 21);
			this->txtWeapTApic->Name = L"txtWeapTApic";
			this->txtWeapTApic->Size = System::Drawing::Size(144, 20);
			this->txtWeapTApic->TabIndex = 62;
			// 
			// label43
			// 
			this->label43->AutoSize = true;
			this->label43->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 80, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->label43->ForeColor = System::Drawing::Color::OliveDrab;
			this->label43->Location = System::Drawing::Point(529, 3);
			this->label43->Name = L"label43";
			this->label43->Size = System::Drawing::Size(111, 120);
			this->label43->TabIndex = 61;
			this->label43->Text = L"3";
			// 
			// txtWeapTASoundId
			// 
			this->txtWeapTASoundId->Location = System::Drawing::Point(240, 43);
			this->txtWeapTASoundId->MaxLength = 1;
			this->txtWeapTASoundId->Name = L"txtWeapTASoundId";
			this->txtWeapTASoundId->Size = System::Drawing::Size(32, 20);
			this->txtWeapTASoundId->TabIndex = 59;
			// 
			// label107
			// 
			this->label107->AutoSize = true;
			this->label107->Location = System::Drawing::Point(187, 46);
			this->label107->Name = L"label107";
			this->label107->Size = System::Drawing::Size(47, 13);
			this->label107->TabIndex = 58;
			this->label107->Text = L"SoundId";
			// 
			// nWeapTAtime
			// 
			this->nWeapTAtime->Location = System::Drawing::Point(375, 63);
			this->nWeapTAtime->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {10000000, 0, 0, 0});
			this->nWeapTAtime->Name = L"nWeapTAtime";
			this->nWeapTAtime->Size = System::Drawing::Size(57, 20);
			this->nWeapTAtime->TabIndex = 55;
			this->nWeapTAtime->ValueChanged += gcnew System::EventHandler(this, &Form1::nWeapTAtime_ValueChanged);
			// 
			// label54
			// 
			this->label54->AutoSize = true;
			this->label54->Location = System::Drawing::Point(294, 68);
			this->label54->Name = L"label54";
			this->label54->Size = System::Drawing::Size(54, 13);
			this->label54->TabIndex = 54;
			this->label54->Text = L"Attack AP";
			// 
			// nWeapTAEffect
			// 
			this->nWeapTAEffect->Location = System::Drawing::Point(375, 103);
			this->nWeapTAEffect->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapTAEffect->Name = L"nWeapTAEffect";
			this->nWeapTAEffect->Size = System::Drawing::Size(57, 20);
			this->nWeapTAEffect->TabIndex = 51;
			// 
			// nWeapTAdistmax
			// 
			this->nWeapTAdistmax->Location = System::Drawing::Point(375, 84);
			this->nWeapTAdistmax->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapTAdistmax->Name = L"nWeapTAdistmax";
			this->nWeapTAdistmax->Size = System::Drawing::Size(47, 20);
			this->nWeapTAdistmax->TabIndex = 50;
			// 
			// label77
			// 
			this->label77->AutoSize = true;
			this->label77->Location = System::Drawing::Point(294, 105);
			this->label77->Name = L"label77";
			this->label77->Size = System::Drawing::Size(53, 13);
			this->label77->TabIndex = 49;
			this->label77->Text = L"Fly effect";
			// 
			// label78
			// 
			this->label78->AutoSize = true;
			this->label78->Location = System::Drawing::Point(294, 86);
			this->label78->Name = L"label78";
			this->label78->Size = System::Drawing::Size(48, 13);
			this->label78->TabIndex = 48;
			this->label78->Text = L"Distance";
			// 
			// nWeapTAdmgmin
			// 
			this->nWeapTAdmgmin->Location = System::Drawing::Point(225, 103);
			this->nWeapTAdmgmin->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapTAdmgmin->Name = L"nWeapTAdmgmin";
			this->nWeapTAdmgmin->Size = System::Drawing::Size(47, 20);
			this->nWeapTAdmgmin->TabIndex = 47;
			// 
			// nWeapTAdmgmax
			// 
			this->nWeapTAdmgmax->Location = System::Drawing::Point(225, 84);
			this->nWeapTAdmgmax->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapTAdmgmax->Name = L"nWeapTAdmgmax";
			this->nWeapTAdmgmax->Size = System::Drawing::Size(47, 20);
			this->nWeapTAdmgmax->TabIndex = 46;
			// 
			// label79
			// 
			this->label79->AutoSize = true;
			this->label79->Location = System::Drawing::Point(153, 105);
			this->label79->Name = L"label79";
			this->label79->Size = System::Drawing::Size(65, 13);
			this->label79->TabIndex = 45;
			this->label79->Text = L"Damage min";
			// 
			// label80
			// 
			this->label80->AutoSize = true;
			this->label80->Location = System::Drawing::Point(153, 86);
			this->label80->Name = L"label80";
			this->label80->Size = System::Drawing::Size(69, 13);
			this->label80->TabIndex = 44;
			this->label80->Text = L"Damage max";
			// 
			// nWeapTAanim2
			// 
			this->nWeapTAanim2->Location = System::Drawing::Point(100, 103);
			this->nWeapTAanim2->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapTAanim2->Name = L"nWeapTAanim2";
			this->nWeapTAanim2->Size = System::Drawing::Size(47, 20);
			this->nWeapTAanim2->TabIndex = 43;
			// 
			// label81
			// 
			this->label81->AutoSize = true;
			this->label81->Location = System::Drawing::Point(2, 105);
			this->label81->Name = L"label81";
			this->label81->Size = System::Drawing::Size(83, 13);
			this->label81->TabIndex = 42;
			this->label81->Text = L"Animation index";
			// 
			// cbWeapTAremove
			// 
			this->cbWeapTAremove->AutoSize = true;
			this->cbWeapTAremove->Location = System::Drawing::Point(99, 25);
			this->cbWeapTAremove->Name = L"cbWeapTAremove";
			this->cbWeapTAremove->Size = System::Drawing::Size(125, 17);
			this->cbWeapTAremove->TabIndex = 40;
			this->cbWeapTAremove->Text = L"Remove after attack";
			this->cbWeapTAremove->UseVisualStyleBackColor = true;
			// 
			// cbWeapTAaim
			// 
			this->cbWeapTAaim->AutoSize = true;
			this->cbWeapTAaim->Location = System::Drawing::Point(99, 5);
			this->cbWeapTAaim->Name = L"cbWeapTAaim";
			this->cbWeapTAaim->Size = System::Drawing::Size(80, 17);
			this->cbWeapTAaim->TabIndex = 39;
			this->cbWeapTAaim->Text = L"Aim aviable";
			this->cbWeapTAaim->UseVisualStyleBackColor = true;
			// 
			// label82
			// 
			this->label82->AutoSize = true;
			this->label82->Location = System::Drawing::Point(294, 5);
			this->label82->Name = L"label82";
			this->label82->Size = System::Drawing::Size(61, 13);
			this->label82->TabIndex = 38;
			this->label82->Text = L"Use picture";
			// 
			// nWeapTAround
			// 
			this->nWeapTAround->Location = System::Drawing::Point(100, 84);
			this->nWeapTAround->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nWeapTAround->Name = L"nWeapTAround";
			this->nWeapTAround->Size = System::Drawing::Size(47, 20);
			this->nWeapTAround->TabIndex = 36;
			this->nWeapTAround->ValueChanged += gcnew System::EventHandler(this, &Form1::nWeapTAround_ValueChanged);
			// 
			// label83
			// 
			this->label83->AutoSize = true;
			this->label83->Location = System::Drawing::Point(2, 86);
			this->label83->Name = L"label83";
			this->label83->Size = System::Drawing::Size(69, 13);
			this->label83->TabIndex = 35;
			this->label83->Text = L"Bullets round";
			// 
			// cmbWeapTAtypeattack
			// 
			this->cmbWeapTAtypeattack->DisplayMember = L"0";
			this->cmbWeapTAtypeattack->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
			this->cmbWeapTAtypeattack->FormattingEnabled = true;
			this->cmbWeapTAtypeattack->Items->AddRange(gcnew cli::array< System::Object^  >(7) {L"Нормальная", L"Лазер", L"Огонь", L"Плазма", 
				L"Электричество", L"EMP", L"Взрыв"});
			this->cmbWeapTAtypeattack->Location = System::Drawing::Point(79, 43);
			this->cmbWeapTAtypeattack->Name = L"cmbWeapTAtypeattack";
			this->cmbWeapTAtypeattack->RightToLeft = System::Windows::Forms::RightToLeft::No;
			this->cmbWeapTAtypeattack->Size = System::Drawing::Size(99, 21);
			this->cmbWeapTAtypeattack->TabIndex = 30;
			// 
			// label85
			// 
			this->label85->AutoSize = true;
			this->label85->Location = System::Drawing::Point(2, 46);
			this->label85->Name = L"label85";
			this->label85->Size = System::Drawing::Size(71, 13);
			this->label85->TabIndex = 29;
			this->label85->Text = L"Damage type";
			// 
			// nWeapTAskill
			// 
			this->nWeapTAskill->Location = System::Drawing::Point(46, 23);
			this->nWeapTAskill->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->nWeapTAskill->Name = L"nWeapTAskill";
			this->nWeapTAskill->Size = System::Drawing::Size(47, 20);
			this->nWeapTAskill->TabIndex = 28;
			// 
			// label86
			// 
			this->label86->AutoSize = true;
			this->label86->Location = System::Drawing::Point(2, 25);
			this->label86->Name = L"label86";
			this->label86->Size = System::Drawing::Size(24, 13);
			this->label86->TabIndex = 27;
			this->label86->Text = L"Skill";
			// 
			// cbWeapTA
			// 
			this->cbWeapTA->AutoSize = true;
			this->cbWeapTA->Location = System::Drawing::Point(2, 5);
			this->cbWeapTA->Name = L"cbWeapTA";
			this->cbWeapTA->Size = System::Drawing::Size(56, 17);
			this->cbWeapTA->TabIndex = 26;
			this->cbWeapTA->Text = L"Active";
			this->cbWeapTA->UseVisualStyleBackColor = true;
			// 
			// tAmmo
			// 
			this->tAmmo->Controls->Add(this->numAmmoDmgDiv);
			this->tAmmo->Controls->Add(this->numAmmoDmgMult);
			this->tAmmo->Controls->Add(this->numAmmoDRMod);
			this->tAmmo->Controls->Add(this->numAmmoACMod);
			this->tAmmo->Controls->Add(this->label93);
			this->tAmmo->Controls->Add(this->label39);
			this->tAmmo->Controls->Add(this->label21);
			this->tAmmo->Controls->Add(this->label20);
			this->tAmmo->Controls->Add(this->label9);
			this->tAmmo->Controls->Add(this->label89);
			this->tAmmo->Controls->Add(this->nAmmoCaliber);
			this->tAmmo->Controls->Add(this->nAmmoSCount);
			this->tAmmo->Controls->Add(this->label88);
			this->tAmmo->Controls->Add(this->label87);
			this->tAmmo->Location = System::Drawing::Point(4, 22);
			this->tAmmo->Name = L"tAmmo";
			this->tAmmo->Size = System::Drawing::Size(660, 169);
			this->tAmmo->TabIndex = 4;
			this->tAmmo->Text = L"Ammo";
			this->tAmmo->UseVisualStyleBackColor = true;
			// 
			// numAmmoDmgDiv
			// 
			this->numAmmoDmgDiv->Location = System::Drawing::Point(69, 135);
			this->numAmmoDmgDiv->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {10000000, 0, 0, 0});
			this->numAmmoDmgDiv->Name = L"numAmmoDmgDiv";
			this->numAmmoDmgDiv->Size = System::Drawing::Size(54, 20);
			this->numAmmoDmgDiv->TabIndex = 13;
			// 
			// numAmmoDmgMult
			// 
			this->numAmmoDmgMult->Location = System::Drawing::Point(69, 109);
			this->numAmmoDmgMult->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {10000000, 0, 0, 0});
			this->numAmmoDmgMult->Name = L"numAmmoDmgMult";
			this->numAmmoDmgMult->Size = System::Drawing::Size(54, 20);
			this->numAmmoDmgMult->TabIndex = 12;
			// 
			// numAmmoDRMod
			// 
			this->numAmmoDRMod->Location = System::Drawing::Point(69, 83);
			this->numAmmoDRMod->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000000, 0, 0, 0});
			this->numAmmoDRMod->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000, 0, 0, System::Int32::MinValue});
			this->numAmmoDRMod->Name = L"numAmmoDRMod";
			this->numAmmoDRMod->Size = System::Drawing::Size(54, 20);
			this->numAmmoDRMod->TabIndex = 11;
			// 
			// numAmmoACMod
			// 
			this->numAmmoACMod->Location = System::Drawing::Point(69, 57);
			this->numAmmoACMod->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000000, 0, 0, 0});
			this->numAmmoACMod->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000, 0, 0, System::Int32::MinValue});
			this->numAmmoACMod->Name = L"numAmmoACMod";
			this->numAmmoACMod->Size = System::Drawing::Size(54, 20);
			this->numAmmoACMod->TabIndex = 10;
			// 
			// label93
			// 
			this->label93->AutoSize = true;
			this->label93->Location = System::Drawing::Point(4, 137);
			this->label93->Name = L"label93";
			this->label93->Size = System::Drawing::Size(43, 13);
			this->label93->TabIndex = 9;
			this->label93->Text = L"DmgDiv";
			// 
			// label39
			// 
			this->label39->AutoSize = true;
			this->label39->Location = System::Drawing::Point(3, 111);
			this->label39->Name = L"label39";
			this->label39->Size = System::Drawing::Size(48, 13);
			this->label39->TabIndex = 8;
			this->label39->Text = L"DmgMult";
			// 
			// label21
			// 
			this->label21->AutoSize = true;
			this->label21->Location = System::Drawing::Point(4, 85);
			this->label21->Name = L"label21";
			this->label21->Size = System::Drawing::Size(41, 13);
			this->label21->TabIndex = 7;
			this->label21->Text = L"DRMod";
			// 
			// label20
			// 
			this->label20->AutoSize = true;
			this->label20->Location = System::Drawing::Point(4, 59);
			this->label20->Name = L"label20";
			this->label20->Size = System::Drawing::Size(41, 13);
			this->label20->TabIndex = 6;
			this->label20->Text = L"ACMod";
			// 
			// label9
			// 
			this->label9->AutoSize = true;
			this->label9->Location = System::Drawing::Point(409, 9);
			this->label9->Name = L"label9";
			this->label9->Size = System::Drawing::Size(124, 117);
			this->label9->TabIndex = 5;
			this->label9->Text = L"{10}{}{14mm}\r\n{11}{}{12-gauge}\r\n{12}{}{9mm}\r\n{13}{}{BB}\r\n{14}{}{.45 cal}\r\n{15}{}{" 
				L"2mm}\r\n{16}{}{4.7mm caseless}\r\n{17}{}{HN needler}\r\n{18}{}{7.62mm}";
			// 
			// label89
			// 
			this->label89->AutoSize = true;
			this->label89->Location = System::Drawing::Point(280, 9);
			this->label89->Name = L"label89";
			this->label89->Size = System::Drawing::Size(132, 130);
			this->label89->TabIndex = 4;
			this->label89->Text = L"{0}{}{None}\r\n{1}{}{Rocket}\r\n{2}{}{Flamethrower Fuel}\r\n{3}{}{C Energy Cell}\r\n{4}{}" 
				L"{D Energy Cell}\r\n{5}{}{.223}\r\n{6}{}{5mm}\r\n{7}{}{.40 cal}\r\n{8}{}{10mm}\r\n{9}{}{.44" 
				L" cal}";
			// 
			// nAmmoCaliber
			// 
			this->nAmmoCaliber->Location = System::Drawing::Point(69, 31);
			this->nAmmoCaliber->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {10000000, 0, 0, 0});
			this->nAmmoCaliber->Name = L"nAmmoCaliber";
			this->nAmmoCaliber->Size = System::Drawing::Size(54, 20);
			this->nAmmoCaliber->TabIndex = 3;
			this->nAmmoCaliber->ValueChanged += gcnew System::EventHandler(this, &Form1::nAmmoCaliber_ValueChanged);
			// 
			// nAmmoSCount
			// 
			this->nAmmoSCount->Location = System::Drawing::Point(69, 7);
			this->nAmmoSCount->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nAmmoSCount->Name = L"nAmmoSCount";
			this->nAmmoSCount->Size = System::Drawing::Size(54, 20);
			this->nAmmoSCount->TabIndex = 2;
			// 
			// label88
			// 
			this->label88->AutoSize = true;
			this->label88->Location = System::Drawing::Point(4, 33);
			this->label88->Name = L"label88";
			this->label88->Size = System::Drawing::Size(40, 13);
			this->label88->TabIndex = 1;
			this->label88->Text = L"Caliber";
			// 
			// label87
			// 
			this->label87->AutoSize = true;
			this->label87->Location = System::Drawing::Point(4, 9);
			this->label87->Name = L"label87";
			this->label87->Size = System::Drawing::Size(61, 13);
			this->label87->TabIndex = 0;
			this->label87->Text = L"Start count";
			// 
			// tContainer
			// 
			this->tContainer->Controls->Add(this->cbContIsNoOpen);
			this->tContainer->Controls->Add(this->cbContCannotPickUp);
			this->tContainer->Controls->Add(this->cbContMagicHandsGrnd);
			this->tContainer->Controls->Add(this->cbContChangeble);
			this->tContainer->Controls->Add(this->label97);
			this->tContainer->Controls->Add(this->nContSize);
			this->tContainer->Location = System::Drawing::Point(4, 22);
			this->tContainer->Name = L"tContainer";
			this->tContainer->Padding = System::Windows::Forms::Padding(3);
			this->tContainer->Size = System::Drawing::Size(660, 169);
			this->tContainer->TabIndex = 1;
			this->tContainer->Text = L"Container";
			this->tContainer->UseVisualStyleBackColor = true;
			// 
			// cbContIsNoOpen
			// 
			this->cbContIsNoOpen->AutoSize = true;
			this->cbContIsNoOpen->Location = System::Drawing::Point(19, 101);
			this->cbContIsNoOpen->Name = L"cbContIsNoOpen";
			this->cbContIsNoOpen->Size = System::Drawing::Size(74, 17);
			this->cbContIsNoOpen->TabIndex = 14;
			this->cbContIsNoOpen->Text = L"IsNoOpen";
			this->cbContIsNoOpen->UseVisualStyleBackColor = true;
			// 
			// cbContCannotPickUp
			// 
			this->cbContCannotPickUp->AutoSize = true;
			this->cbContCannotPickUp->Location = System::Drawing::Point(19, 32);
			this->cbContCannotPickUp->Name = L"cbContCannotPickUp";
			this->cbContCannotPickUp->Size = System::Drawing::Size(92, 17);
			this->cbContCannotPickUp->TabIndex = 13;
			this->cbContCannotPickUp->Text = L"CannotPickUp";
			this->cbContCannotPickUp->UseVisualStyleBackColor = true;
			// 
			// cbContMagicHandsGrnd
			// 
			this->cbContMagicHandsGrnd->AutoSize = true;
			this->cbContMagicHandsGrnd->Location = System::Drawing::Point(19, 55);
			this->cbContMagicHandsGrnd->Name = L"cbContMagicHandsGrnd";
			this->cbContMagicHandsGrnd->Size = System::Drawing::Size(106, 17);
			this->cbContMagicHandsGrnd->TabIndex = 12;
			this->cbContMagicHandsGrnd->Text = L"MagicHandsGrnd";
			this->cbContMagicHandsGrnd->UseVisualStyleBackColor = true;
			// 
			// cbContChangeble
			// 
			this->cbContChangeble->AutoSize = true;
			this->cbContChangeble->Location = System::Drawing::Point(19, 78);
			this->cbContChangeble->Name = L"cbContChangeble";
			this->cbContChangeble->Size = System::Drawing::Size(77, 17);
			this->cbContChangeble->TabIndex = 11;
			this->cbContChangeble->Text = L"Changeble";
			this->cbContChangeble->UseVisualStyleBackColor = true;
			// 
			// label97
			// 
			this->label97->AutoSize = true;
			this->label97->Location = System::Drawing::Point(16, 8);
			this->label97->Name = L"label97";
			this->label97->Size = System::Drawing::Size(26, 13);
			this->label97->TabIndex = 7;
			this->label97->Text = L"Size";
			// 
			// nContSize
			// 
			this->nContSize->Location = System::Drawing::Point(68, 6);
			this->nContSize->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000, 0, 0, 0});
			this->nContSize->Name = L"nContSize";
			this->nContSize->Size = System::Drawing::Size(54, 20);
			this->nContSize->TabIndex = 6;
			// 
			// tMiscEx
			// 
			this->tMiscEx->Controls->Add(this->numMisc2SV3);
			this->tMiscEx->Controls->Add(this->numMisc2SV2);
			this->tMiscEx->Controls->Add(this->groupBox5);
			this->tMiscEx->Controls->Add(this->numMisc2SV1);
			this->tMiscEx->Controls->Add(this->label96);
			this->tMiscEx->Controls->Add(this->label95);
			this->tMiscEx->Controls->Add(this->label94);
			this->tMiscEx->Location = System::Drawing::Point(4, 22);
			this->tMiscEx->Name = L"tMiscEx";
			this->tMiscEx->Size = System::Drawing::Size(660, 169);
			this->tMiscEx->TabIndex = 11;
			this->tMiscEx->Text = L"MiscEx";
			this->tMiscEx->UseVisualStyleBackColor = true;
			// 
			// numMisc2SV3
			// 
			this->numMisc2SV3->Location = System::Drawing::Point(112, 63);
			this->numMisc2SV3->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1410065408, 2, 0, 0});
			this->numMisc2SV3->Name = L"numMisc2SV3";
			this->numMisc2SV3->Size = System::Drawing::Size(116, 20);
			this->numMisc2SV3->TabIndex = 8;
			// 
			// numMisc2SV2
			// 
			this->numMisc2SV2->Location = System::Drawing::Point(112, 37);
			this->numMisc2SV2->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1410065408, 2, 0, 0});
			this->numMisc2SV2->Name = L"numMisc2SV2";
			this->numMisc2SV2->Size = System::Drawing::Size(116, 20);
			this->numMisc2SV2->TabIndex = 7;
			// 
			// groupBox5
			// 
			this->groupBox5->Controls->Add(this->label10);
			this->groupBox5->Controls->Add(this->numMisc2CarEntire);
			this->groupBox5->Controls->Add(this->label117);
			this->groupBox5->Controls->Add(this->label116);
			this->groupBox5->Controls->Add(this->label115);
			this->groupBox5->Controls->Add(this->label114);
			this->groupBox5->Controls->Add(this->label101);
			this->groupBox5->Controls->Add(this->label84);
			this->groupBox5->Controls->Add(this->label11);
			this->groupBox5->Controls->Add(this->numMisc2CarFuelConsumption);
			this->groupBox5->Controls->Add(this->numMisc2CarRunToBreak);
			this->groupBox5->Controls->Add(this->numMisc2CarFuelTank);
			this->groupBox5->Controls->Add(this->numMisc2CarCritCapacity);
			this->groupBox5->Controls->Add(this->numMisc2CarWearConsumption);
			this->groupBox5->Controls->Add(this->numMisc2CarNegotiability);
			this->groupBox5->Controls->Add(this->numMisc2CarSpeed);
			this->groupBox5->Controls->Add(this->rbMisc2CarWater);
			this->groupBox5->Controls->Add(this->rbMisc2CarGround);
			this->groupBox5->Controls->Add(this->rbMisc2CarFly);
			this->groupBox5->Controls->Add(this->txtMisc2CarBag2);
			this->groupBox5->Controls->Add(this->label112);
			this->groupBox5->Controls->Add(this->txtMisc2CarBag1);
			this->groupBox5->Controls->Add(this->label111);
			this->groupBox5->Controls->Add(this->label109);
			this->groupBox5->Controls->Add(this->txtMisc2CarBlocks);
			this->groupBox5->Controls->Add(this->cbMisc2IsCar);
			this->groupBox5->Location = System::Drawing::Point(234, 11);
			this->groupBox5->Name = L"groupBox5";
			this->groupBox5->Size = System::Drawing::Size(423, 155);
			this->groupBox5->TabIndex = 6;
			this->groupBox5->TabStop = false;
			this->groupBox5->Text = L"Car";
			// 
			// label10
			// 
			this->label10->AutoSize = true;
			this->label10->Location = System::Drawing::Point(204, 59);
			this->label10->Name = L"label10";
			this->label10->Size = System::Drawing::Size(35, 13);
			this->label10->TabIndex = 89;
			this->label10->Text = L"Entire";
			// 
			// numMisc2CarEntire
			// 
			this->numMisc2CarEntire->Location = System::Drawing::Point(203, 77);
			this->numMisc2CarEntire->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->numMisc2CarEntire->Name = L"numMisc2CarEntire";
			this->numMisc2CarEntire->Size = System::Drawing::Size(46, 20);
			this->numMisc2CarEntire->TabIndex = 88;
			// 
			// label117
			// 
			this->label117->AutoSize = true;
			this->label117->Location = System::Drawing::Point(329, 14);
			this->label117->Name = L"label117";
			this->label117->Size = System::Drawing::Size(89, 13);
			this->label117->TabIndex = 87;
			this->label117->Text = L"FuelConsumption";
			// 
			// label116
			// 
			this->label116->AutoSize = true;
			this->label116->Location = System::Drawing::Point(211, 14);
			this->label116->Name = L"label116";
			this->label116->Size = System::Drawing::Size(65, 13);
			this->label116->TabIndex = 86;
			this->label116->Text = L"RunToBreak";
			// 
			// label115
			// 
			this->label115->AutoSize = true;
			this->label115->Location = System::Drawing::Point(278, 14);
			this->label115->Name = L"label115";
			this->label115->Size = System::Drawing::Size(50, 13);
			this->label115->TabIndex = 85;
			this->label115->Text = L"FuelTank";
			// 
			// label114
			// 
			this->label114->AutoSize = true;
			this->label114->Location = System::Drawing::Point(278, 95);
			this->label114->Name = L"label114";
			this->label114->Size = System::Drawing::Size(66, 13);
			this->label114->TabIndex = 84;
			this->label114->Text = L"CritCapacity";
			// 
			// label101
			// 
			this->label101->AutoSize = true;
			this->label101->Location = System::Drawing::Point(120, 14);
			this->label101->Name = L"label101";
			this->label101->Size = System::Drawing::Size(95, 13);
			this->label101->TabIndex = 83;
			this->label101->Text = L"WearConsumption";
			// 
			// label84
			// 
			this->label84->AutoSize = true;
			this->label84->Location = System::Drawing::Point(277, 69);
			this->label84->Name = L"label84";
			this->label84->Size = System::Drawing::Size(66, 13);
			this->label84->TabIndex = 82;
			this->label84->Text = L"Negotiability";
			// 
			// label11
			// 
			this->label11->AutoSize = true;
			this->label11->Location = System::Drawing::Point(303, 121);
			this->label11->Name = L"label11";
			this->label11->Size = System::Drawing::Size(37, 13);
			this->label11->TabIndex = 81;
			this->label11->Text = L"Speed";
			// 
			// numMisc2CarFuelConsumption
			// 
			this->numMisc2CarFuelConsumption->Location = System::Drawing::Point(347, 30);
			this->numMisc2CarFuelConsumption->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->numMisc2CarFuelConsumption->Name = L"numMisc2CarFuelConsumption";
			this->numMisc2CarFuelConsumption->Size = System::Drawing::Size(47, 20);
			this->numMisc2CarFuelConsumption->TabIndex = 80;
			// 
			// numMisc2CarRunToBreak
			// 
			this->numMisc2CarRunToBreak->Location = System::Drawing::Point(214, 30);
			this->numMisc2CarRunToBreak->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65535, 0, 0, 0});
			this->numMisc2CarRunToBreak->Name = L"numMisc2CarRunToBreak";
			this->numMisc2CarRunToBreak->Size = System::Drawing::Size(46, 20);
			this->numMisc2CarRunToBreak->TabIndex = 79;
			// 
			// numMisc2CarFuelTank
			// 
			this->numMisc2CarFuelTank->Location = System::Drawing::Point(281, 30);
			this->numMisc2CarFuelTank->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65535, 0, 0, 0});
			this->numMisc2CarFuelTank->Name = L"numMisc2CarFuelTank";
			this->numMisc2CarFuelTank->Size = System::Drawing::Size(47, 20);
			this->numMisc2CarFuelTank->TabIndex = 78;
			// 
			// numMisc2CarCritCapacity
			// 
			this->numMisc2CarCritCapacity->Location = System::Drawing::Point(347, 93);
			this->numMisc2CarCritCapacity->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->numMisc2CarCritCapacity->Name = L"numMisc2CarCritCapacity";
			this->numMisc2CarCritCapacity->Size = System::Drawing::Size(46, 20);
			this->numMisc2CarCritCapacity->TabIndex = 77;
			// 
			// numMisc2CarWearConsumption
			// 
			this->numMisc2CarWearConsumption->Location = System::Drawing::Point(157, 30);
			this->numMisc2CarWearConsumption->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->numMisc2CarWearConsumption->Name = L"numMisc2CarWearConsumption";
			this->numMisc2CarWearConsumption->Size = System::Drawing::Size(46, 20);
			this->numMisc2CarWearConsumption->TabIndex = 76;
			// 
			// numMisc2CarNegotiability
			// 
			this->numMisc2CarNegotiability->Location = System::Drawing::Point(347, 67);
			this->numMisc2CarNegotiability->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->numMisc2CarNegotiability->Name = L"numMisc2CarNegotiability";
			this->numMisc2CarNegotiability->Size = System::Drawing::Size(46, 20);
			this->numMisc2CarNegotiability->TabIndex = 75;
			// 
			// numMisc2CarSpeed
			// 
			this->numMisc2CarSpeed->Location = System::Drawing::Point(347, 119);
			this->numMisc2CarSpeed->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->numMisc2CarSpeed->Name = L"numMisc2CarSpeed";
			this->numMisc2CarSpeed->Size = System::Drawing::Size(46, 20);
			this->numMisc2CarSpeed->TabIndex = 74;
			// 
			// rbMisc2CarWater
			// 
			this->rbMisc2CarWater->AutoSize = true;
			this->rbMisc2CarWater->Location = System::Drawing::Point(62, 38);
			this->rbMisc2CarWater->Name = L"rbMisc2CarWater";
			this->rbMisc2CarWater->Size = System::Drawing::Size(55, 17);
			this->rbMisc2CarWater->TabIndex = 73;
			this->rbMisc2CarWater->Text = L"Water";
			this->rbMisc2CarWater->UseVisualStyleBackColor = true;
			// 
			// rbMisc2CarGround
			// 
			this->rbMisc2CarGround->AutoSize = true;
			this->rbMisc2CarGround->Checked = true;
			this->rbMisc2CarGround->Location = System::Drawing::Point(62, 10);
			this->rbMisc2CarGround->Name = L"rbMisc2CarGround";
			this->rbMisc2CarGround->Size = System::Drawing::Size(60, 17);
			this->rbMisc2CarGround->TabIndex = 71;
			this->rbMisc2CarGround->TabStop = true;
			this->rbMisc2CarGround->Text = L"Ground";
			this->rbMisc2CarGround->UseVisualStyleBackColor = true;
			// 
			// rbMisc2CarFly
			// 
			this->rbMisc2CarFly->AutoSize = true;
			this->rbMisc2CarFly->Location = System::Drawing::Point(62, 24);
			this->rbMisc2CarFly->Name = L"rbMisc2CarFly";
			this->rbMisc2CarFly->Size = System::Drawing::Size(39, 17);
			this->rbMisc2CarFly->TabIndex = 72;
			this->rbMisc2CarFly->Text = L"Fly";
			this->rbMisc2CarFly->UseVisualStyleBackColor = true;
			this->rbMisc2CarFly->CheckedChanged += gcnew System::EventHandler(this, &Form1::radioButton2_CheckedChanged);
			// 
			// txtMisc2CarBag2
			// 
			this->txtMisc2CarBag2->Location = System::Drawing::Point(102, 77);
			this->txtMisc2CarBag2->MaxLength = 12;
			this->txtMisc2CarBag2->Name = L"txtMisc2CarBag2";
			this->txtMisc2CarBag2->Size = System::Drawing::Size(87, 20);
			this->txtMisc2CarBag2->TabIndex = 6;
			// 
			// label112
			// 
			this->label112->AutoSize = true;
			this->label112->Location = System::Drawing::Point(99, 59);
			this->label112->Name = L"label112";
			this->label112->Size = System::Drawing::Size(34, 13);
			this->label112->TabIndex = 5;
			this->label112->Text = L"Bag 2";
			// 
			// txtMisc2CarBag1
			// 
			this->txtMisc2CarBag1->Location = System::Drawing::Point(6, 77);
			this->txtMisc2CarBag1->MaxLength = 12;
			this->txtMisc2CarBag1->Name = L"txtMisc2CarBag1";
			this->txtMisc2CarBag1->Size = System::Drawing::Size(87, 20);
			this->txtMisc2CarBag1->TabIndex = 4;
			// 
			// label111
			// 
			this->label111->AutoSize = true;
			this->label111->Location = System::Drawing::Point(3, 59);
			this->label111->Name = L"label111";
			this->label111->Size = System::Drawing::Size(34, 13);
			this->label111->TabIndex = 3;
			this->label111->Text = L"Bag 1";
			// 
			// label109
			// 
			this->label109->AutoSize = true;
			this->label109->Location = System::Drawing::Point(6, 100);
			this->label109->Name = L"label109";
			this->label109->Size = System::Drawing::Size(36, 13);
			this->label109->TabIndex = 2;
			this->label109->Text = L"Blocks";
			// 
			// txtMisc2CarBlocks
			// 
			this->txtMisc2CarBlocks->Location = System::Drawing::Point(6, 116);
			this->txtMisc2CarBlocks->MaxLength = 80;
			this->txtMisc2CarBlocks->Multiline = true;
			this->txtMisc2CarBlocks->Name = L"txtMisc2CarBlocks";
			this->txtMisc2CarBlocks->Size = System::Drawing::Size(254, 33);
			this->txtMisc2CarBlocks->TabIndex = 1;
			// 
			// cbMisc2IsCar
			// 
			this->cbMisc2IsCar->AutoSize = true;
			this->cbMisc2IsCar->Location = System::Drawing::Point(6, 19);
			this->cbMisc2IsCar->Name = L"cbMisc2IsCar";
			this->cbMisc2IsCar->Size = System::Drawing::Size(52, 17);
			this->cbMisc2IsCar->TabIndex = 0;
			this->cbMisc2IsCar->Text = L"IsCar";
			this->cbMisc2IsCar->UseVisualStyleBackColor = true;
			// 
			// numMisc2SV1
			// 
			this->numMisc2SV1->Location = System::Drawing::Point(112, 11);
			this->numMisc2SV1->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1410065408, 2, 0, 0});
			this->numMisc2SV1->Name = L"numMisc2SV1";
			this->numMisc2SV1->Size = System::Drawing::Size(116, 20);
			this->numMisc2SV1->TabIndex = 3;
			// 
			// label96
			// 
			this->label96->AutoSize = true;
			this->label96->Location = System::Drawing::Point(25, 65);
			this->label96->Name = L"label96";
			this->label96->Size = System::Drawing::Size(69, 13);
			this->label96->TabIndex = 2;
			this->label96->Text = L"Start Value 3";
			// 
			// label95
			// 
			this->label95->AutoSize = true;
			this->label95->Location = System::Drawing::Point(25, 39);
			this->label95->Name = L"label95";
			this->label95->Size = System::Drawing::Size(69, 13);
			this->label95->TabIndex = 1;
			this->label95->Text = L"Start Value 2";
			// 
			// label94
			// 
			this->label94->AutoSize = true;
			this->label94->Location = System::Drawing::Point(25, 13);
			this->label94->Name = L"label94";
			this->label94->Size = System::Drawing::Size(69, 13);
			this->label94->TabIndex = 0;
			this->label94->Text = L"Start Value 1";
			// 
			// tDoor
			// 
			this->tDoor->Controls->Add(this->cbDoorIsNoOpen);
			this->tDoor->Controls->Add(this->cbDoorWalkThru);
			this->tDoor->Location = System::Drawing::Point(4, 22);
			this->tDoor->Name = L"tDoor";
			this->tDoor->Size = System::Drawing::Size(660, 169);
			this->tDoor->TabIndex = 7;
			this->tDoor->Text = L"Door";
			this->tDoor->UseVisualStyleBackColor = true;
			// 
			// cbDoorIsNoOpen
			// 
			this->cbDoorIsNoOpen->AutoSize = true;
			this->cbDoorIsNoOpen->Location = System::Drawing::Point(12, 37);
			this->cbDoorIsNoOpen->Name = L"cbDoorIsNoOpen";
			this->cbDoorIsNoOpen->Size = System::Drawing::Size(74, 17);
			this->cbDoorIsNoOpen->TabIndex = 4;
			this->cbDoorIsNoOpen->Text = L"IsNoOpen";
			this->cbDoorIsNoOpen->UseVisualStyleBackColor = true;
			// 
			// cbDoorWalkThru
			// 
			this->cbDoorWalkThru->AutoSize = true;
			this->cbDoorWalkThru->Location = System::Drawing::Point(12, 14);
			this->cbDoorWalkThru->Name = L"cbDoorWalkThru";
			this->cbDoorWalkThru->Size = System::Drawing::Size(71, 17);
			this->cbDoorWalkThru->TabIndex = 1;
			this->cbDoorWalkThru->Text = L"WalkThru";
			this->cbDoorWalkThru->UseVisualStyleBackColor = true;
			// 
			// tGrid
			// 
			this->tGrid->Controls->Add(this->groupBox3);
			this->tGrid->Location = System::Drawing::Point(4, 22);
			this->tGrid->Name = L"tGrid";
			this->tGrid->Size = System::Drawing::Size(660, 169);
			this->tGrid->TabIndex = 9;
			this->tGrid->Text = L"Grid";
			this->tGrid->UseVisualStyleBackColor = true;
			// 
			// groupBox3
			// 
			this->groupBox3->Controls->Add(this->rbGridElevatr);
			this->groupBox3->Controls->Add(this->rbGridLadderTop);
			this->groupBox3->Controls->Add(this->rbGridLadderBottom);
			this->groupBox3->Controls->Add(this->rbGridStairs);
			this->groupBox3->Controls->Add(this->rbGridExitGrid);
			this->groupBox3->Location = System::Drawing::Point(19, 12);
			this->groupBox3->Name = L"groupBox3";
			this->groupBox3->Size = System::Drawing::Size(132, 135);
			this->groupBox3->TabIndex = 0;
			this->groupBox3->TabStop = false;
			this->groupBox3->Text = L"Type";
			// 
			// rbGridElevatr
			// 
			this->rbGridElevatr->AutoSize = true;
			this->rbGridElevatr->Location = System::Drawing::Point(6, 111);
			this->rbGridElevatr->Name = L"rbGridElevatr";
			this->rbGridElevatr->Size = System::Drawing::Size(65, 17);
			this->rbGridElevatr->TabIndex = 4;
			this->rbGridElevatr->TabStop = true;
			this->rbGridElevatr->Text = L"Elevator";
			this->rbGridElevatr->UseVisualStyleBackColor = true;
			// 
			// rbGridLadderTop
			// 
			this->rbGridLadderTop->AutoSize = true;
			this->rbGridLadderTop->Location = System::Drawing::Point(6, 88);
			this->rbGridLadderTop->Name = L"rbGridLadderTop";
			this->rbGridLadderTop->Size = System::Drawing::Size(76, 17);
			this->rbGridLadderTop->TabIndex = 3;
			this->rbGridLadderTop->TabStop = true;
			this->rbGridLadderTop->Text = L"LadderTop";
			this->rbGridLadderTop->UseVisualStyleBackColor = true;
			// 
			// rbGridLadderBottom
			// 
			this->rbGridLadderBottom->AutoSize = true;
			this->rbGridLadderBottom->Location = System::Drawing::Point(6, 65);
			this->rbGridLadderBottom->Name = L"rbGridLadderBottom";
			this->rbGridLadderBottom->Size = System::Drawing::Size(92, 17);
			this->rbGridLadderBottom->TabIndex = 2;
			this->rbGridLadderBottom->TabStop = true;
			this->rbGridLadderBottom->Text = L"LadderBottom";
			this->rbGridLadderBottom->UseVisualStyleBackColor = true;
			// 
			// rbGridStairs
			// 
			this->rbGridStairs->AutoSize = true;
			this->rbGridStairs->Location = System::Drawing::Point(6, 42);
			this->rbGridStairs->Name = L"rbGridStairs";
			this->rbGridStairs->Size = System::Drawing::Size(52, 17);
			this->rbGridStairs->TabIndex = 1;
			this->rbGridStairs->TabStop = true;
			this->rbGridStairs->Text = L"Stairs";
			this->rbGridStairs->UseVisualStyleBackColor = true;
			// 
			// rbGridExitGrid
			// 
			this->rbGridExitGrid->AutoSize = true;
			this->rbGridExitGrid->Location = System::Drawing::Point(6, 19);
			this->rbGridExitGrid->Name = L"rbGridExitGrid";
			this->rbGridExitGrid->Size = System::Drawing::Size(62, 17);
			this->rbGridExitGrid->TabIndex = 0;
			this->rbGridExitGrid->TabStop = true;
			this->rbGridExitGrid->Text = L"ExitGrid";
			this->rbGridExitGrid->UseVisualStyleBackColor = true;
			// 
			// tMisc
			// 
			this->tMisc->Location = System::Drawing::Point(4, 22);
			this->tMisc->Name = L"tMisc";
			this->tMisc->Size = System::Drawing::Size(660, 169);
			this->tMisc->TabIndex = 5;
			this->tMisc->Text = L"Misc";
			this->tMisc->UseVisualStyleBackColor = true;
			// 
			// tKey
			// 
			this->tKey->Location = System::Drawing::Point(4, 22);
			this->tKey->Name = L"tKey";
			this->tKey->Size = System::Drawing::Size(660, 169);
			this->tKey->TabIndex = 6;
			this->tKey->Text = L"Key";
			this->tKey->UseVisualStyleBackColor = true;
			// 
			// tGeneric
			// 
			this->tGeneric->Location = System::Drawing::Point(4, 22);
			this->tGeneric->Name = L"tGeneric";
			this->tGeneric->Size = System::Drawing::Size(660, 169);
			this->tGeneric->TabIndex = 10;
			this->tGeneric->Text = L"Generic";
			this->tGeneric->UseVisualStyleBackColor = true;
			// 
			// tWall
			// 
			this->tWall->Location = System::Drawing::Point(4, 22);
			this->tWall->Name = L"tWall";
			this->tWall->Size = System::Drawing::Size(660, 169);
			this->tWall->TabIndex = 12;
			this->tWall->Text = L"Wall";
			this->tWall->UseVisualStyleBackColor = true;
			// 
			// txtInfo
			// 
			this->txtInfo->Location = System::Drawing::Point(441, 52);
			this->txtInfo->Name = L"txtInfo";
			this->txtInfo->ReadOnly = true;
			this->txtInfo->Size = System::Drawing::Size(210, 153);
			this->txtInfo->TabIndex = 5;
			this->txtInfo->Text = L"";
			// 
			// txtName
			// 
			this->txtName->Location = System::Drawing::Point(441, 26);
			this->txtName->Name = L"txtName";
			this->txtName->ReadOnly = true;
			this->txtName->Size = System::Drawing::Size(209, 20);
			this->txtName->TabIndex = 6;
			// 
			// label1
			// 
			this->label1->AutoSize = true;
			this->label1->Location = System::Drawing::Point(441, 6);
			this->label1->Name = L"label1";
			this->label1->Size = System::Drawing::Size(61, 13);
			this->label1->TabIndex = 7;
			this->label1->Text = L"Name, Info";
			// 
			// label4
			// 
			this->label4->AutoSize = true;
			this->label4->Location = System::Drawing::Point(2, 84);
			this->label4->Name = L"label4";
			this->label4->Size = System::Drawing::Size(95, 13);
			this->label4->TabIndex = 11;
			this->label4->Text = L"Distantion (hexes)";
			// 
			// label5
			// 
			this->label5->AutoSize = true;
			this->label5->Location = System::Drawing::Point(2, 44);
			this->label5->Name = L"label5";
			this->label5->Size = System::Drawing::Size(93, 13);
			this->label5->TabIndex = 12;
			this->label5->Text = L"Intensity (0..100)";
			// 
			// label7
			// 
			this->label7->AutoSize = true;
			this->label7->Location = System::Drawing::Point(7, 35);
			this->label7->Name = L"label7";
			this->label7->Size = System::Drawing::Size(41, 13);
			this->label7->TabIndex = 14;
			this->label7->Text = L"Volume";
			// 
			// label8
			// 
			this->label8->AutoSize = true;
			this->label8->Location = System::Drawing::Point(7, 16);
			this->label8->Name = L"label8";
			this->label8->Size = System::Drawing::Size(84, 13);
			this->label8->TabIndex = 15;
			this->label8->Text = L"Weight (gramm)";
			// 
			// gmMoveFlags
			// 
			this->gmMoveFlags->Controls->Add(this->cbCached);
			this->gmMoveFlags->Controls->Add(this->cbGag);
			this->gmMoveFlags->Controls->Add(this->cbBigGun);
			this->gmMoveFlags->Controls->Add(this->cbTwoHands);
			this->gmMoveFlags->Controls->Add(this->cbWallTransEnd);
			this->gmMoveFlags->Controls->Add(this->cbLightThru);
			this->gmMoveFlags->Controls->Add(this->cbShootThru);
			this->gmMoveFlags->Controls->Add(this->cbMultiHex);
			this->gmMoveFlags->Controls->Add(this->cbNoBlock);
			this->gmMoveFlags->Controls->Add(this->cbHidden);
			this->gmMoveFlags->Controls->Add(this->cbFlat);
			this->gmMoveFlags->Location = System::Drawing::Point(304, 9);
			this->gmMoveFlags->Name = L"gmMoveFlags";
			this->gmMoveFlags->Size = System::Drawing::Size(119, 249);
			this->gmMoveFlags->TabIndex = 23;
			this->gmMoveFlags->TabStop = false;
			this->gmMoveFlags->Text = L"Flags";
			// 
			// cbCached
			// 
			this->cbCached->AutoSize = true;
			this->cbCached->Location = System::Drawing::Point(6, 219);
			this->cbCached->Name = L"cbCached";
			this->cbCached->Size = System::Drawing::Size(62, 17);
			this->cbCached->TabIndex = 28;
			this->cbCached->Text = L"Cached";
			this->cbCached->UseVisualStyleBackColor = true;
			this->cbCached->Visible = false;
			// 
			// cbGag
			// 
			this->cbGag->AutoSize = true;
			this->cbGag->Location = System::Drawing::Point(6, 141);
			this->cbGag->Name = L"cbGag";
			this->cbGag->Size = System::Drawing::Size(45, 17);
			this->cbGag->TabIndex = 27;
			this->cbGag->Text = L"Gag";
			this->cbGag->UseVisualStyleBackColor = true;
			// 
			// cbBigGun
			// 
			this->cbBigGun->AutoSize = true;
			this->cbBigGun->Location = System::Drawing::Point(6, 126);
			this->cbBigGun->Name = L"cbBigGun";
			this->cbBigGun->Size = System::Drawing::Size(59, 17);
			this->cbBigGun->TabIndex = 26;
			this->cbBigGun->Text = L"BigGun";
			this->cbBigGun->UseVisualStyleBackColor = true;
			// 
			// cbTwoHands
			// 
			this->cbTwoHands->AutoSize = true;
			this->cbTwoHands->Location = System::Drawing::Point(6, 111);
			this->cbTwoHands->Name = L"cbTwoHands";
			this->cbTwoHands->Size = System::Drawing::Size(76, 17);
			this->cbTwoHands->TabIndex = 25;
			this->cbTwoHands->Text = L"TwoHands";
			this->cbTwoHands->UseVisualStyleBackColor = true;
			// 
			// cbWallTransEnd
			// 
			this->cbWallTransEnd->AutoSize = true;
			this->cbWallTransEnd->Location = System::Drawing::Point(6, 203);
			this->cbWallTransEnd->Name = L"cbWallTransEnd";
			this->cbWallTransEnd->Size = System::Drawing::Size(91, 17);
			this->cbWallTransEnd->TabIndex = 24;
			this->cbWallTransEnd->Text = L"WallTransEnd";
			this->cbWallTransEnd->UseVisualStyleBackColor = true;
			this->cbWallTransEnd->Visible = false;
			// 
			// cbLightThru
			// 
			this->cbLightThru->AutoSize = true;
			this->cbLightThru->Location = System::Drawing::Point(6, 95);
			this->cbLightThru->Name = L"cbLightThru";
			this->cbLightThru->Size = System::Drawing::Size(71, 17);
			this->cbLightThru->TabIndex = 23;
			this->cbLightThru->Text = L"LightThru";
			this->cbLightThru->UseVisualStyleBackColor = true;
			// 
			// cbShootThru
			// 
			this->cbShootThru->AutoSize = true;
			this->cbShootThru->Location = System::Drawing::Point(6, 79);
			this->cbShootThru->Name = L"cbShootThru";
			this->cbShootThru->Size = System::Drawing::Size(76, 17);
			this->cbShootThru->TabIndex = 22;
			this->cbShootThru->Text = L"ShootThru";
			this->cbShootThru->UseVisualStyleBackColor = true;
			// 
			// cbMultiHex
			// 
			this->cbMultiHex->AutoSize = true;
			this->cbMultiHex->Location = System::Drawing::Point(6, 64);
			this->cbMultiHex->Name = L"cbMultiHex";
			this->cbMultiHex->Size = System::Drawing::Size(67, 17);
			this->cbMultiHex->TabIndex = 21;
			this->cbMultiHex->Text = L"MultiHex";
			this->cbMultiHex->UseVisualStyleBackColor = true;
			// 
			// cbNoBlock
			// 
			this->cbNoBlock->AutoSize = true;
			this->cbNoBlock->Location = System::Drawing::Point(6, 48);
			this->cbNoBlock->Name = L"cbNoBlock";
			this->cbNoBlock->Size = System::Drawing::Size(63, 17);
			this->cbNoBlock->TabIndex = 20;
			this->cbNoBlock->Text = L"NoBlock";
			this->cbNoBlock->UseVisualStyleBackColor = true;
			// 
			// cbHidden
			// 
			this->cbHidden->AutoSize = true;
			this->cbHidden->Location = System::Drawing::Point(6, 16);
			this->cbHidden->Name = L"cbHidden";
			this->cbHidden->Size = System::Drawing::Size(59, 17);
			this->cbHidden->TabIndex = 3;
			this->cbHidden->Text = L"Hidden";
			this->cbHidden->UseVisualStyleBackColor = true;
			// 
			// cbFlat
			// 
			this->cbFlat->AutoSize = true;
			this->cbFlat->Location = System::Drawing::Point(6, 32);
			this->cbFlat->Name = L"cbFlat";
			this->cbFlat->Size = System::Drawing::Size(44, 17);
			this->cbFlat->TabIndex = 19;
			this->cbFlat->Text = L"Flat";
			this->cbFlat->UseVisualStyleBackColor = true;
			// 
			// gbAlpha
			// 
			this->gbAlpha->Controls->Add(this->cbColorizing);
			this->gbAlpha->Controls->Add(this->label35);
			this->gbAlpha->Controls->Add(this->label34);
			this->gbAlpha->Controls->Add(this->label33);
			this->gbAlpha->Controls->Add(this->numBlue);
			this->gbAlpha->Controls->Add(this->numGreen);
			this->gbAlpha->Controls->Add(this->numRed);
			this->gbAlpha->Controls->Add(this->label110);
			this->gbAlpha->Controls->Add(this->numAlpha);
			this->gbAlpha->Location = System::Drawing::Point(548, 9);
			this->gbAlpha->Name = L"gbAlpha";
			this->gbAlpha->Size = System::Drawing::Size(106, 138);
			this->gbAlpha->TabIndex = 24;
			this->gbAlpha->TabStop = false;
			this->gbAlpha->Text = L"Color (0..255)";
			// 
			// cbColorizing
			// 
			this->cbColorizing->AutoSize = true;
			this->cbColorizing->Location = System::Drawing::Point(8, 18);
			this->cbColorizing->Name = L"cbColorizing";
			this->cbColorizing->Size = System::Drawing::Size(64, 17);
			this->cbColorizing->TabIndex = 30;
			this->cbColorizing->Text = L"Colorize";
			this->cbColorizing->UseVisualStyleBackColor = true;
			// 
			// label35
			// 
			this->label35->AutoSize = true;
			this->label35->Location = System::Drawing::Point(8, 86);
			this->label35->Name = L"label35";
			this->label35->Size = System::Drawing::Size(27, 13);
			this->label35->TabIndex = 17;
			this->label35->Text = L"Blue";
			// 
			// label34
			// 
			this->label34->AutoSize = true;
			this->label34->Location = System::Drawing::Point(7, 66);
			this->label34->Name = L"label34";
			this->label34->Size = System::Drawing::Size(36, 13);
			this->label34->TabIndex = 16;
			this->label34->Text = L"Green";
			// 
			// label33
			// 
			this->label33->AutoSize = true;
			this->label33->Location = System::Drawing::Point(7, 47);
			this->label33->Name = L"label33";
			this->label33->Size = System::Drawing::Size(26, 13);
			this->label33->TabIndex = 15;
			this->label33->Text = L"Red";
			// 
			// numBlue
			// 
			this->numBlue->Location = System::Drawing::Point(47, 85);
			this->numBlue->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->numBlue->Name = L"numBlue";
			this->numBlue->Size = System::Drawing::Size(53, 20);
			this->numBlue->TabIndex = 14;
			// 
			// numGreen
			// 
			this->numGreen->Location = System::Drawing::Point(47, 64);
			this->numGreen->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->numGreen->Name = L"numGreen";
			this->numGreen->Size = System::Drawing::Size(53, 20);
			this->numGreen->TabIndex = 13;
			// 
			// numRed
			// 
			this->numRed->Location = System::Drawing::Point(47, 43);
			this->numRed->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->numRed->Name = L"numRed";
			this->numRed->Size = System::Drawing::Size(53, 20);
			this->numRed->TabIndex = 12;
			// 
			// label110
			// 
			this->label110->AutoSize = true;
			this->label110->Location = System::Drawing::Point(7, 106);
			this->label110->Name = L"label110";
			this->label110->Size = System::Drawing::Size(34, 13);
			this->label110->TabIndex = 11;
			this->label110->Text = L"Alpha";
			// 
			// numAlpha
			// 
			this->numAlpha->Location = System::Drawing::Point(47, 106);
			this->numAlpha->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->numAlpha->Name = L"numAlpha";
			this->numAlpha->Size = System::Drawing::Size(53, 20);
			this->numAlpha->TabIndex = 4;
			this->numAlpha->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			// 
			// gbActionFlags
			// 
			this->gbActionFlags->Controls->Add(this->cbNoSteal);
			this->gbActionFlags->Controls->Add(this->cbNoLoot);
			this->gbActionFlags->Controls->Add(this->cbTrap);
			this->gbActionFlags->Controls->Add(this->cbNoLightInfluence);
			this->gbActionFlags->Controls->Add(this->cbNoHighlight);
			this->gbActionFlags->Controls->Add(this->cbGekk);
			this->gbActionFlags->Controls->Add(this->cbAlwaysView);
			this->gbActionFlags->Controls->Add(this->cbBadItem);
			this->gbActionFlags->Controls->Add(this->cbHasTimer);
			this->gbActionFlags->Controls->Add(this->cbCanLook);
			this->gbActionFlags->Controls->Add(this->cbCanTalk);
			this->gbActionFlags->Controls->Add(this->cbCanUseOnSmtn);
			this->gbActionFlags->Controls->Add(this->cbCanUse);
			this->gbActionFlags->Controls->Add(this->cbCanPickUp);
			this->gbActionFlags->Location = System::Drawing::Point(186, 9);
			this->gbActionFlags->Name = L"gbActionFlags";
			this->gbActionFlags->Size = System::Drawing::Size(112, 249);
			this->gbActionFlags->TabIndex = 25;
			this->gbActionFlags->TabStop = false;
			this->gbActionFlags->Text = L"Flags";
			// 
			// cbNoSteal
			// 
			this->cbNoSteal->AutoSize = true;
			this->cbNoSteal->Location = System::Drawing::Point(6, 219);
			this->cbNoSteal->Name = L"cbNoSteal";
			this->cbNoSteal->Size = System::Drawing::Size(63, 17);
			this->cbNoSteal->TabIndex = 38;
			this->cbNoSteal->Text = L"NoSteal";
			this->cbNoSteal->UseVisualStyleBackColor = true;
			// 
			// cbNoLoot
			// 
			this->cbNoLoot->AutoSize = true;
			this->cbNoLoot->Location = System::Drawing::Point(6, 203);
			this->cbNoLoot->Name = L"cbNoLoot";
			this->cbNoLoot->Size = System::Drawing::Size(60, 17);
			this->cbNoLoot->TabIndex = 37;
			this->cbNoLoot->Text = L"NoLoot";
			this->cbNoLoot->UseVisualStyleBackColor = true;
			// 
			// cbTrap
			// 
			this->cbTrap->AutoSize = true;
			this->cbTrap->Location = System::Drawing::Point(6, 171);
			this->cbTrap->Name = L"cbTrap";
			this->cbTrap->Size = System::Drawing::Size(48, 17);
			this->cbTrap->TabIndex = 35;
			this->cbTrap->Text = L"Trap";
			this->cbTrap->UseVisualStyleBackColor = true;
			// 
			// cbNoLightInfluence
			// 
			this->cbNoLightInfluence->AutoSize = true;
			this->cbNoLightInfluence->Location = System::Drawing::Point(6, 187);
			this->cbNoLightInfluence->Name = L"cbNoLightInfluence";
			this->cbNoLightInfluence->Size = System::Drawing::Size(107, 17);
			this->cbNoLightInfluence->TabIndex = 36;
			this->cbNoLightInfluence->Text = L"NoLightInfluence";
			this->cbNoLightInfluence->UseVisualStyleBackColor = true;
			// 
			// cbNoHighlight
			// 
			this->cbNoHighlight->AutoSize = true;
			this->cbNoHighlight->Location = System::Drawing::Point(6, 156);
			this->cbNoHighlight->Name = L"cbNoHighlight";
			this->cbNoHighlight->Size = System::Drawing::Size(78, 17);
			this->cbNoHighlight->TabIndex = 34;
			this->cbNoHighlight->Text = L"NoContour";
			this->cbNoHighlight->UseVisualStyleBackColor = true;
			// 
			// cbGekk
			// 
			this->cbGekk->AutoSize = true;
			this->cbGekk->Location = System::Drawing::Point(6, 141);
			this->cbGekk->Name = L"cbGekk";
			this->cbGekk->Size = System::Drawing::Size(49, 17);
			this->cbGekk->TabIndex = 8;
			this->cbGekk->Text = L"Geck";
			this->cbGekk->UseVisualStyleBackColor = true;
			// 
			// cbAlwaysView
			// 
			this->cbAlwaysView->AutoSize = true;
			this->cbAlwaysView->Location = System::Drawing::Point(6, 126);
			this->cbAlwaysView->Name = L"cbAlwaysView";
			this->cbAlwaysView->Size = System::Drawing::Size(82, 17);
			this->cbAlwaysView->TabIndex = 7;
			this->cbAlwaysView->Text = L"AlwaysView";
			this->cbAlwaysView->UseVisualStyleBackColor = true;
			// 
			// cbBadItem
			// 
			this->cbBadItem->AutoSize = true;
			this->cbBadItem->Location = System::Drawing::Point(6, 111);
			this->cbBadItem->Name = L"cbBadItem";
			this->cbBadItem->Size = System::Drawing::Size(66, 17);
			this->cbBadItem->TabIndex = 6;
			this->cbBadItem->Text = L"BadItem";
			this->cbBadItem->UseVisualStyleBackColor = true;
			// 
			// cbHasTimer
			// 
			this->cbHasTimer->AutoSize = true;
			this->cbHasTimer->Location = System::Drawing::Point(6, 95);
			this->cbHasTimer->Name = L"cbHasTimer";
			this->cbHasTimer->Size = System::Drawing::Size(70, 17);
			this->cbHasTimer->TabIndex = 5;
			this->cbHasTimer->Text = L"HasTimer";
			this->cbHasTimer->UseVisualStyleBackColor = true;
			// 
			// cbCanLook
			// 
			this->cbCanLook->AutoSize = true;
			this->cbCanLook->Location = System::Drawing::Point(6, 62);
			this->cbCanLook->Name = L"cbCanLook";
			this->cbCanLook->Size = System::Drawing::Size(67, 17);
			this->cbCanLook->TabIndex = 4;
			this->cbCanLook->Text = L"CanLook";
			this->cbCanLook->UseVisualStyleBackColor = true;
			// 
			// cbCanTalk
			// 
			this->cbCanTalk->AutoSize = true;
			this->cbCanTalk->Location = System::Drawing::Point(6, 79);
			this->cbCanTalk->Name = L"cbCanTalk";
			this->cbCanTalk->Size = System::Drawing::Size(64, 17);
			this->cbCanTalk->TabIndex = 3;
			this->cbCanTalk->Text = L"CanTalk";
			this->cbCanTalk->UseVisualStyleBackColor = true;
			// 
			// cbCanUseOnSmtn
			// 
			this->cbCanUseOnSmtn->AutoSize = true;
			this->cbCanUseOnSmtn->Location = System::Drawing::Point(6, 46);
			this->cbCanUseOnSmtn->Name = L"cbCanUseOnSmtn";
			this->cbCanUseOnSmtn->Size = System::Drawing::Size(101, 17);
			this->cbCanUseOnSmtn->TabIndex = 2;
			this->cbCanUseOnSmtn->Text = L"CanUseOnSmth";
			this->cbCanUseOnSmtn->UseVisualStyleBackColor = true;
			// 
			// cbCanUse
			// 
			this->cbCanUse->AutoSize = true;
			this->cbCanUse->Location = System::Drawing::Point(6, 14);
			this->cbCanUse->Name = L"cbCanUse";
			this->cbCanUse->Size = System::Drawing::Size(63, 17);
			this->cbCanUse->TabIndex = 1;
			this->cbCanUse->Text = L"CanUse";
			this->cbCanUse->UseVisualStyleBackColor = true;
			// 
			// cbCanPickUp
			// 
			this->cbCanPickUp->AutoSize = true;
			this->cbCanPickUp->Location = System::Drawing::Point(6, 30);
			this->cbCanPickUp->Name = L"cbCanPickUp";
			this->cbCanPickUp->Size = System::Drawing::Size(76, 17);
			this->cbCanPickUp->TabIndex = 0;
			this->cbCanPickUp->Text = L"CanPickUp";
			this->cbCanPickUp->UseVisualStyleBackColor = true;
			// 
			// nLightDist
			// 
			this->nLightDist->Location = System::Drawing::Point(9, 100);
			this->nLightDist->Name = L"nLightDist";
			this->nLightDist->Size = System::Drawing::Size(67, 20);
			this->nLightDist->TabIndex = 28;
			// 
			// nLightInt
			// 
			this->nLightInt->Location = System::Drawing::Point(9, 60);
			this->nLightInt->Name = L"nLightInt";
			this->nLightInt->Size = System::Drawing::Size(67, 20);
			this->nLightInt->TabIndex = 29;
			// 
			// nWeight
			// 
			this->nWeight->Location = System::Drawing::Point(96, 14);
			this->nWeight->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000000, 0, 0, 0});
			this->nWeight->Name = L"nWeight";
			this->nWeight->Size = System::Drawing::Size(67, 20);
			this->nWeight->TabIndex = 30;
			// 
			// nSize
			// 
			this->nSize->Location = System::Drawing::Point(96, 32);
			this->nSize->Name = L"nSize";
			this->nSize->Size = System::Drawing::Size(67, 20);
			this->nSize->TabIndex = 31;
			// 
			// nCost
			// 
			this->nCost->Location = System::Drawing::Point(96, 51);
			this->nCost->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000000000, 0, 0, 0});
			this->nCost->Name = L"nCost";
			this->nCost->Size = System::Drawing::Size(67, 20);
			this->nCost->TabIndex = 35;
			// 
			// label13
			// 
			this->label13->AutoSize = true;
			this->label13->Location = System::Drawing::Point(7, 54);
			this->label13->Name = L"label13";
			this->label13->Size = System::Drawing::Size(53, 13);
			this->label13->TabIndex = 41;
			this->label13->Text = L"Base cost";
			this->label13->Click += gcnew System::EventHandler(this, &Form1::label13_Click);
			// 
			// gbMaterial
			// 
			this->gbMaterial->Controls->Add(this->rbLeather);
			this->gbMaterial->Controls->Add(this->rbCement);
			this->gbMaterial->Controls->Add(this->rbStone);
			this->gbMaterial->Controls->Add(this->rbDirt);
			this->gbMaterial->Controls->Add(this->rbWood);
			this->gbMaterial->Controls->Add(this->rbPlastic);
			this->gbMaterial->Controls->Add(this->rbMetal);
			this->gbMaterial->Controls->Add(this->rbGlass);
			this->gbMaterial->Location = System::Drawing::Point(429, 177);
			this->gbMaterial->Name = L"gbMaterial";
			this->gbMaterial->Size = System::Drawing::Size(129, 81);
			this->gbMaterial->TabIndex = 42;
			this->gbMaterial->TabStop = false;
			this->gbMaterial->Text = L"Material";
			// 
			// rbLeather
			// 
			this->rbLeather->AutoSize = true;
			this->rbLeather->Location = System::Drawing::Point(64, 60);
			this->rbLeather->Name = L"rbLeather";
			this->rbLeather->Size = System::Drawing::Size(62, 17);
			this->rbLeather->TabIndex = 7;
			this->rbLeather->Text = L"Leather";
			this->rbLeather->UseVisualStyleBackColor = true;
			// 
			// rbCement
			// 
			this->rbCement->AutoSize = true;
			this->rbCement->Location = System::Drawing::Point(64, 45);
			this->rbCement->Name = L"rbCement";
			this->rbCement->Size = System::Drawing::Size(62, 17);
			this->rbCement->TabIndex = 6;
			this->rbCement->Text = L"Cement";
			this->rbCement->UseVisualStyleBackColor = true;
			// 
			// rbStone
			// 
			this->rbStone->AutoSize = true;
			this->rbStone->Location = System::Drawing::Point(64, 29);
			this->rbStone->Name = L"rbStone";
			this->rbStone->Size = System::Drawing::Size(53, 17);
			this->rbStone->TabIndex = 5;
			this->rbStone->Text = L"Stone";
			this->rbStone->UseVisualStyleBackColor = true;
			// 
			// rbDirt
			// 
			this->rbDirt->AutoSize = true;
			this->rbDirt->Location = System::Drawing::Point(64, 13);
			this->rbDirt->Name = L"rbDirt";
			this->rbDirt->Size = System::Drawing::Size(42, 17);
			this->rbDirt->TabIndex = 4;
			this->rbDirt->Text = L"Dirt";
			this->rbDirt->UseVisualStyleBackColor = true;
			this->rbDirt->CheckedChanged += gcnew System::EventHandler(this, &Form1::radioButton7_CheckedChanged);
			// 
			// rbWood
			// 
			this->rbWood->AutoSize = true;
			this->rbWood->Location = System::Drawing::Point(6, 60);
			this->rbWood->Name = L"rbWood";
			this->rbWood->Size = System::Drawing::Size(53, 17);
			this->rbWood->TabIndex = 3;
			this->rbWood->Text = L"Wood";
			this->rbWood->UseVisualStyleBackColor = true;
			// 
			// rbPlastic
			// 
			this->rbPlastic->AutoSize = true;
			this->rbPlastic->Location = System::Drawing::Point(6, 45);
			this->rbPlastic->Name = L"rbPlastic";
			this->rbPlastic->Size = System::Drawing::Size(55, 17);
			this->rbPlastic->TabIndex = 2;
			this->rbPlastic->Text = L"Plastic";
			this->rbPlastic->UseVisualStyleBackColor = true;
			// 
			// rbMetal
			// 
			this->rbMetal->AutoSize = true;
			this->rbMetal->Checked = true;
			this->rbMetal->Location = System::Drawing::Point(6, 29);
			this->rbMetal->Name = L"rbMetal";
			this->rbMetal->Size = System::Drawing::Size(51, 17);
			this->rbMetal->TabIndex = 1;
			this->rbMetal->TabStop = true;
			this->rbMetal->Text = L"Metal";
			this->rbMetal->UseVisualStyleBackColor = true;
			// 
			// rbGlass
			// 
			this->rbGlass->AutoSize = true;
			this->rbGlass->Location = System::Drawing::Point(6, 13);
			this->rbGlass->Name = L"rbGlass";
			this->rbGlass->Size = System::Drawing::Size(50, 17);
			this->rbGlass->TabIndex = 0;
			this->rbGlass->Text = L"Glass";
			this->rbGlass->UseVisualStyleBackColor = true;
			// 
			// bSave
			// 
			this->bSave->Location = System::Drawing::Point(488, 238);
			this->bSave->Name = L"bSave";
			this->bSave->Size = System::Drawing::Size(77, 20);
			this->bSave->TabIndex = 44;
			this->bSave->Text = L"Save all";
			this->bSave->UseVisualStyleBackColor = true;
			this->bSave->Click += gcnew System::EventHandler(this, &Form1::bSave_Click);
			// 
			// label90
			// 
			this->label90->AutoSize = true;
			this->label90->Location = System::Drawing::Point(312, 9);
			this->label90->Name = L"label90";
			this->label90->Size = System::Drawing::Size(48, 13);
			this->label90->TabIndex = 45;
			this->label90->Text = L"Number:";
			// 
			// nPid
			// 
			this->nPid->Location = System::Drawing::Point(365, 7);
			this->nPid->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65000, 0, 0, 0});
			this->nPid->Name = L"nPid";
			this->nPid->Size = System::Drawing::Size(66, 20);
			this->nPid->TabIndex = 46;
			// 
			// label55
			// 
			this->label55->AutoSize = true;
			this->label55->Location = System::Drawing::Point(276, 56);
			this->label55->Name = L"label55";
			this->label55->Size = System::Drawing::Size(92, 13);
			this->label55->TabIndex = 48;
			this->label55->Text = L"Picture on ground";
			// 
			// label91
			// 
			this->label91->AutoSize = true;
			this->label91->Location = System::Drawing::Point(276, 93);
			this->label91->Name = L"label91";
			this->label91->Size = System::Drawing::Size(100, 13);
			this->label91->TabIndex = 49;
			this->label91->Text = L"Picture in inventory";
			// 
			// cbFix
			// 
			this->cbFix->AutoSize = true;
			this->cbFix->Location = System::Drawing::Point(639, 308);
			this->cbFix->Name = L"cbFix";
			this->cbFix->Size = System::Drawing::Size(40, 17);
			this->cbFix->TabIndex = 52;
			this->cbFix->Text = L"Fix";
			this->cbFix->UseVisualStyleBackColor = true;
			this->cbFix->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbFix_CheckedChanged);
			// 
			// lbList
			// 
			this->lbList->FormattingEnabled = true;
			this->lbList->HorizontalScrollbar = true;
			this->lbList->Location = System::Drawing::Point(6, 6);
			this->lbList->Name = L"lbList";
			this->lbList->Size = System::Drawing::Size(267, 251);
			this->lbList->TabIndex = 54;
			this->lbList->SelectedIndexChanged += gcnew System::EventHandler(this, &Form1::lbList_SelectedIndexChanged);
			this->lbList->DoubleClick += gcnew System::EventHandler(this, &Form1::lbList_DoubleClick);
			// 
			// bAdd
			// 
			this->bAdd->Location = System::Drawing::Point(279, 33);
			this->bAdd->Name = L"bAdd";
			this->bAdd->Size = System::Drawing::Size(152, 20);
			this->bAdd->TabIndex = 55;
			this->bAdd->Text = L"Add object";
			this->bAdd->UseVisualStyleBackColor = true;
			this->bAdd->Click += gcnew System::EventHandler(this, &Form1::bAdd_Click);
			// 
			// groupBox1
			// 
			this->groupBox1->Controls->Add(this->cbDisableDir4);
			this->groupBox1->Controls->Add(this->cbDisableDir5);
			this->groupBox1->Controls->Add(this->cbDisableDir3);
			this->groupBox1->Controls->Add(this->cbDisableDir2);
			this->groupBox1->Controls->Add(this->cbDisableDir1);
			this->groupBox1->Controls->Add(this->cbDisableDir0);
			this->groupBox1->Controls->Add(this->cbLightGlobal);
			this->groupBox1->Controls->Add(this->cbLightInverse);
			this->groupBox1->Controls->Add(this->cbLight);
			this->groupBox1->Controls->Add(this->label4);
			this->groupBox1->Controls->Add(this->label5);
			this->groupBox1->Controls->Add(this->nLightDist);
			this->groupBox1->Controls->Add(this->nLightInt);
			this->groupBox1->Location = System::Drawing::Point(6, 88);
			this->groupBox1->Name = L"groupBox1";
			this->groupBox1->Size = System::Drawing::Size(176, 170);
			this->groupBox1->TabIndex = 56;
			this->groupBox1->TabStop = false;
			this->groupBox1->Text = L"Light (color taked from Color)";
			// 
			// cbDisableDir4
			// 
			this->cbDisableDir4->AutoSize = true;
			this->cbDisableDir4->Location = System::Drawing::Point(95, 129);
			this->cbDisableDir4->Name = L"cbDisableDir4";
			this->cbDisableDir4->Size = System::Drawing::Size(79, 17);
			this->cbDisableDir4->TabIndex = 49;
			this->cbDisableDir4->Text = L"DisableDir4";
			this->cbDisableDir4->UseVisualStyleBackColor = true;
			// 
			// cbDisableDir5
			// 
			this->cbDisableDir5->AutoSize = true;
			this->cbDisableDir5->Location = System::Drawing::Point(95, 145);
			this->cbDisableDir5->Name = L"cbDisableDir5";
			this->cbDisableDir5->Size = System::Drawing::Size(79, 17);
			this->cbDisableDir5->TabIndex = 48;
			this->cbDisableDir5->Text = L"DisableDir5";
			this->cbDisableDir5->UseVisualStyleBackColor = true;
			// 
			// cbDisableDir3
			// 
			this->cbDisableDir3->AutoSize = true;
			this->cbDisableDir3->Location = System::Drawing::Point(95, 112);
			this->cbDisableDir3->Name = L"cbDisableDir3";
			this->cbDisableDir3->Size = System::Drawing::Size(79, 17);
			this->cbDisableDir3->TabIndex = 47;
			this->cbDisableDir3->Text = L"DisableDir3";
			this->cbDisableDir3->UseVisualStyleBackColor = true;
			// 
			// cbDisableDir2
			// 
			this->cbDisableDir2->AutoSize = true;
			this->cbDisableDir2->Location = System::Drawing::Point(95, 96);
			this->cbDisableDir2->Name = L"cbDisableDir2";
			this->cbDisableDir2->Size = System::Drawing::Size(79, 17);
			this->cbDisableDir2->TabIndex = 46;
			this->cbDisableDir2->Text = L"DisableDir2";
			this->cbDisableDir2->UseVisualStyleBackColor = true;
			// 
			// cbDisableDir1
			// 
			this->cbDisableDir1->AutoSize = true;
			this->cbDisableDir1->Location = System::Drawing::Point(95, 80);
			this->cbDisableDir1->Name = L"cbDisableDir1";
			this->cbDisableDir1->Size = System::Drawing::Size(79, 17);
			this->cbDisableDir1->TabIndex = 45;
			this->cbDisableDir1->Text = L"DisableDir1";
			this->cbDisableDir1->UseVisualStyleBackColor = true;
			// 
			// cbDisableDir0
			// 
			this->cbDisableDir0->AutoSize = true;
			this->cbDisableDir0->Location = System::Drawing::Point(95, 64);
			this->cbDisableDir0->Name = L"cbDisableDir0";
			this->cbDisableDir0->Size = System::Drawing::Size(79, 17);
			this->cbDisableDir0->TabIndex = 44;
			this->cbDisableDir0->Text = L"DisableDir0";
			this->cbDisableDir0->UseVisualStyleBackColor = true;
			// 
			// cbLightGlobal
			// 
			this->cbLightGlobal->AutoSize = true;
			this->cbLightGlobal->Location = System::Drawing::Point(101, 38);
			this->cbLightGlobal->Name = L"cbLightGlobal";
			this->cbLightGlobal->Size = System::Drawing::Size(55, 17);
			this->cbLightGlobal->TabIndex = 43;
			this->cbLightGlobal->Text = L"Global";
			this->cbLightGlobal->UseVisualStyleBackColor = true;
			// 
			// cbLightInverse
			// 
			this->cbLightInverse->AutoSize = true;
			this->cbLightInverse->Location = System::Drawing::Point(101, 22);
			this->cbLightInverse->Name = L"cbLightInverse";
			this->cbLightInverse->Size = System::Drawing::Size(63, 17);
			this->cbLightInverse->TabIndex = 42;
			this->cbLightInverse->Text = L"Inverse";
			this->cbLightInverse->UseVisualStyleBackColor = true;
			// 
			// cbLight
			// 
			this->cbLight->AutoSize = true;
			this->cbLight->Location = System::Drawing::Point(11, 20);
			this->cbLight->Name = L"cbLight";
			this->cbLight->Size = System::Drawing::Size(49, 17);
			this->cbLight->TabIndex = 40;
			this->cbLight->Text = L"Light";
			this->cbLight->UseVisualStyleBackColor = true;
			// 
			// groupBox2
			// 
			this->groupBox2->Controls->Add(this->label7);
			this->groupBox2->Controls->Add(this->label8);
			this->groupBox2->Controls->Add(this->nWeight);
			this->groupBox2->Controls->Add(this->nSize);
			this->groupBox2->Controls->Add(this->nCost);
			this->groupBox2->Controls->Add(this->label13);
			this->groupBox2->Location = System::Drawing::Point(6, 6);
			this->groupBox2->Name = L"groupBox2";
			this->groupBox2->Size = System::Drawing::Size(168, 78);
			this->groupBox2->TabIndex = 57;
			this->groupBox2->TabStop = false;
			this->groupBox2->Text = L"Main";
			// 
			// tabControl1
			// 
			this->tabControl1->Controls->Add(this->tabPage1);
			this->tabControl1->Controls->Add(this->tabPage2);
			this->tabControl1->Controls->Add(this->tabPage3);
			this->tabControl1->Location = System::Drawing::Point(12, 12);
			this->tabControl1->Name = L"tabControl1";
			this->tabControl1->SelectedIndex = 0;
			this->tabControl1->Size = System::Drawing::Size(668, 290);
			this->tabControl1->TabIndex = 58;
			// 
			// tabPage1
			// 
			this->tabPage1->Controls->Add(this->txtPicInv);
			this->tabPage1->Controls->Add(this->txtPicMap);
			this->tabPage1->Controls->Add(this->bLoad);
			this->tabPage1->Controls->Add(this->bSaveList);
			this->tabPage1->Controls->Add(this->bLoad1);
			this->tabPage1->Controls->Add(this->bSave1);
			this->tabPage1->Controls->Add(this->cbShowWall);
			this->tabPage1->Controls->Add(this->cbShowGeneric);
			this->tabPage1->Controls->Add(this->cbShowGrid);
			this->tabPage1->Controls->Add(this->cbShowDoor);
			this->tabPage1->Controls->Add(this->cbShowContainer);
			this->tabPage1->Controls->Add(this->cbShowKey);
			this->tabPage1->Controls->Add(this->cbShowMiscEx);
			this->tabPage1->Controls->Add(this->cbShowMisc);
			this->tabPage1->Controls->Add(this->cbShowAmmo);
			this->tabPage1->Controls->Add(this->cbShowWeapon);
			this->tabPage1->Controls->Add(this->cbShowDrug);
			this->tabPage1->Controls->Add(this->cbShowArmor);
			this->tabPage1->Controls->Add(this->lbList);
			this->tabPage1->Controls->Add(this->bSave);
			this->tabPage1->Controls->Add(this->bAdd);
			this->tabPage1->Controls->Add(this->txtInfo);
			this->tabPage1->Controls->Add(this->txtName);
			this->tabPage1->Controls->Add(this->label1);
			this->tabPage1->Controls->Add(this->label91);
			this->tabPage1->Controls->Add(this->label90);
			this->tabPage1->Controls->Add(this->label55);
			this->tabPage1->Controls->Add(this->nPid);
			this->tabPage1->Location = System::Drawing::Point(4, 22);
			this->tabPage1->Name = L"tabPage1";
			this->tabPage1->Padding = System::Windows::Forms::Padding(3);
			this->tabPage1->Size = System::Drawing::Size(660, 264);
			this->tabPage1->TabIndex = 0;
			this->tabPage1->Text = L"Main info";
			this->tabPage1->UseVisualStyleBackColor = true;
			// 
			// txtPicInv
			// 
			this->txtPicInv->Location = System::Drawing::Point(279, 109);
			this->txtPicInv->Name = L"txtPicInv";
			this->txtPicInv->Size = System::Drawing::Size(159, 20);
			this->txtPicInv->TabIndex = 74;
			// 
			// txtPicMap
			// 
			this->txtPicMap->Location = System::Drawing::Point(279, 70);
			this->txtPicMap->Name = L"txtPicMap";
			this->txtPicMap->Size = System::Drawing::Size(159, 20);
			this->txtPicMap->TabIndex = 73;
			// 
			// bLoad
			// 
			this->bLoad->Location = System::Drawing::Point(405, 211);
			this->bLoad->Name = L"bLoad";
			this->bLoad->Size = System::Drawing::Size(129, 21);
			this->bLoad->TabIndex = 72;
			this->bLoad->Text = L"Load list";
			this->bLoad->UseVisualStyleBackColor = true;
			this->bLoad->Click += gcnew System::EventHandler(this, &Form1::bLoad_Click);
			// 
			// bSaveList
			// 
			this->bSaveList->Location = System::Drawing::Point(405, 238);
			this->bSaveList->Name = L"bSaveList";
			this->bSaveList->Size = System::Drawing::Size(77, 20);
			this->bSaveList->TabIndex = 71;
			this->bSaveList->Text = L"Save list";
			this->bSaveList->UseVisualStyleBackColor = true;
			this->bSaveList->Click += gcnew System::EventHandler(this, &Form1::bSaveList_Click);
			// 
			// bLoad1
			// 
			this->bLoad1->Location = System::Drawing::Point(544, 211);
			this->bLoad1->Name = L"bLoad1";
			this->bLoad1->Size = System::Drawing::Size(107, 21);
			this->bLoad1->TabIndex = 69;
			this->bLoad1->Text = L"Load single";
			this->bLoad1->UseVisualStyleBackColor = true;
			this->bLoad1->Click += gcnew System::EventHandler(this, &Form1::bLoad1_Click);
			// 
			// bSave1
			// 
			this->bSave1->Location = System::Drawing::Point(570, 238);
			this->bSave1->Name = L"bSave1";
			this->bSave1->Size = System::Drawing::Size(80, 20);
			this->bSave1->TabIndex = 70;
			this->bSave1->Text = L"Save single";
			this->bSave1->UseVisualStyleBackColor = true;
			this->bSave1->Click += gcnew System::EventHandler(this, &Form1::bSave1_Click);
			// 
			// cbShowWall
			// 
			this->cbShowWall->AutoSize = true;
			this->cbShowWall->Checked = true;
			this->cbShowWall->CheckState = System::Windows::Forms::CheckState::Checked;
			this->cbShowWall->Location = System::Drawing::Point(349, 196);
			this->cbShowWall->Name = L"cbShowWall";
			this->cbShowWall->Size = System::Drawing::Size(46, 17);
			this->cbShowWall->TabIndex = 67;
			this->cbShowWall->Text = L"Wall";
			this->cbShowWall->UseVisualStyleBackColor = true;
			this->cbShowWall->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbShowArmor_CheckedChanged);
			// 
			// cbShowGeneric
			// 
			this->cbShowGeneric->AutoSize = true;
			this->cbShowGeneric->Checked = true;
			this->cbShowGeneric->CheckState = System::Windows::Forms::CheckState::Checked;
			this->cbShowGeneric->Location = System::Drawing::Point(349, 181);
			this->cbShowGeneric->Name = L"cbShowGeneric";
			this->cbShowGeneric->Size = System::Drawing::Size(62, 17);
			this->cbShowGeneric->TabIndex = 66;
			this->cbShowGeneric->Text = L"Generic";
			this->cbShowGeneric->UseVisualStyleBackColor = true;
			this->cbShowGeneric->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbShowArmor_CheckedChanged);
			// 
			// cbShowGrid
			// 
			this->cbShowGrid->AutoSize = true;
			this->cbShowGrid->Checked = true;
			this->cbShowGrid->CheckState = System::Windows::Forms::CheckState::Checked;
			this->cbShowGrid->Location = System::Drawing::Point(349, 165);
			this->cbShowGrid->Name = L"cbShowGrid";
			this->cbShowGrid->Size = System::Drawing::Size(45, 17);
			this->cbShowGrid->TabIndex = 65;
			this->cbShowGrid->Text = L"Grid";
			this->cbShowGrid->UseVisualStyleBackColor = true;
			this->cbShowGrid->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbShowArmor_CheckedChanged);
			// 
			// cbShowDoor
			// 
			this->cbShowDoor->AutoSize = true;
			this->cbShowDoor->Checked = true;
			this->cbShowDoor->CheckState = System::Windows::Forms::CheckState::Checked;
			this->cbShowDoor->Location = System::Drawing::Point(349, 150);
			this->cbShowDoor->Name = L"cbShowDoor";
			this->cbShowDoor->Size = System::Drawing::Size(49, 17);
			this->cbShowDoor->TabIndex = 64;
			this->cbShowDoor->Text = L"Door";
			this->cbShowDoor->UseVisualStyleBackColor = true;
			this->cbShowDoor->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbShowArmor_CheckedChanged);
			// 
			// cbShowContainer
			// 
			this->cbShowContainer->AutoSize = true;
			this->cbShowContainer->Checked = true;
			this->cbShowContainer->CheckState = System::Windows::Forms::CheckState::Checked;
			this->cbShowContainer->Location = System::Drawing::Point(349, 135);
			this->cbShowContainer->Name = L"cbShowContainer";
			this->cbShowContainer->Size = System::Drawing::Size(73, 17);
			this->cbShowContainer->TabIndex = 63;
			this->cbShowContainer->Text = L"Container";
			this->cbShowContainer->UseVisualStyleBackColor = true;
			this->cbShowContainer->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbShowArmor_CheckedChanged);
			// 
			// cbShowKey
			// 
			this->cbShowKey->AutoSize = true;
			this->cbShowKey->Checked = true;
			this->cbShowKey->CheckState = System::Windows::Forms::CheckState::Checked;
			this->cbShowKey->Location = System::Drawing::Point(279, 226);
			this->cbShowKey->Name = L"cbShowKey";
			this->cbShowKey->Size = System::Drawing::Size(44, 17);
			this->cbShowKey->TabIndex = 62;
			this->cbShowKey->Text = L"Key";
			this->cbShowKey->UseVisualStyleBackColor = true;
			this->cbShowKey->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbShowArmor_CheckedChanged);
			// 
			// cbShowMiscEx
			// 
			this->cbShowMiscEx->AutoSize = true;
			this->cbShowMiscEx->Checked = true;
			this->cbShowMiscEx->CheckState = System::Windows::Forms::CheckState::Checked;
			this->cbShowMiscEx->Location = System::Drawing::Point(279, 211);
			this->cbShowMiscEx->Name = L"cbShowMiscEx";
			this->cbShowMiscEx->Size = System::Drawing::Size(58, 17);
			this->cbShowMiscEx->TabIndex = 61;
			this->cbShowMiscEx->Text = L"MiscEx";
			this->cbShowMiscEx->UseVisualStyleBackColor = true;
			this->cbShowMiscEx->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbShowArmor_CheckedChanged);
			// 
			// cbShowMisc
			// 
			this->cbShowMisc->AutoSize = true;
			this->cbShowMisc->Checked = true;
			this->cbShowMisc->CheckState = System::Windows::Forms::CheckState::Checked;
			this->cbShowMisc->Location = System::Drawing::Point(279, 196);
			this->cbShowMisc->Name = L"cbShowMisc";
			this->cbShowMisc->Size = System::Drawing::Size(46, 17);
			this->cbShowMisc->TabIndex = 60;
			this->cbShowMisc->Text = L"Misc";
			this->cbShowMisc->UseVisualStyleBackColor = true;
			this->cbShowMisc->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbShowArmor_CheckedChanged);
			// 
			// cbShowAmmo
			// 
			this->cbShowAmmo->AutoSize = true;
			this->cbShowAmmo->Checked = true;
			this->cbShowAmmo->CheckState = System::Windows::Forms::CheckState::Checked;
			this->cbShowAmmo->Location = System::Drawing::Point(279, 180);
			this->cbShowAmmo->Name = L"cbShowAmmo";
			this->cbShowAmmo->Size = System::Drawing::Size(55, 17);
			this->cbShowAmmo->TabIndex = 59;
			this->cbShowAmmo->Text = L"Ammo";
			this->cbShowAmmo->UseVisualStyleBackColor = true;
			this->cbShowAmmo->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbShowArmor_CheckedChanged);
			// 
			// cbShowWeapon
			// 
			this->cbShowWeapon->AutoSize = true;
			this->cbShowWeapon->Checked = true;
			this->cbShowWeapon->CheckState = System::Windows::Forms::CheckState::Checked;
			this->cbShowWeapon->Location = System::Drawing::Point(279, 165);
			this->cbShowWeapon->Name = L"cbShowWeapon";
			this->cbShowWeapon->Size = System::Drawing::Size(66, 17);
			this->cbShowWeapon->TabIndex = 58;
			this->cbShowWeapon->Text = L"Weapon";
			this->cbShowWeapon->UseVisualStyleBackColor = true;
			this->cbShowWeapon->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbShowArmor_CheckedChanged);
			// 
			// cbShowDrug
			// 
			this->cbShowDrug->AutoSize = true;
			this->cbShowDrug->Checked = true;
			this->cbShowDrug->CheckState = System::Windows::Forms::CheckState::Checked;
			this->cbShowDrug->Location = System::Drawing::Point(279, 150);
			this->cbShowDrug->Name = L"cbShowDrug";
			this->cbShowDrug->Size = System::Drawing::Size(49, 17);
			this->cbShowDrug->TabIndex = 57;
			this->cbShowDrug->Text = L"Drug";
			this->cbShowDrug->UseVisualStyleBackColor = true;
			this->cbShowDrug->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbShowArmor_CheckedChanged);
			// 
			// cbShowArmor
			// 
			this->cbShowArmor->AutoSize = true;
			this->cbShowArmor->Checked = true;
			this->cbShowArmor->CheckState = System::Windows::Forms::CheckState::Checked;
			this->cbShowArmor->Location = System::Drawing::Point(279, 135);
			this->cbShowArmor->Name = L"cbShowArmor";
			this->cbShowArmor->Size = System::Drawing::Size(55, 17);
			this->cbShowArmor->TabIndex = 56;
			this->cbShowArmor->Text = L"Armor";
			this->cbShowArmor->UseVisualStyleBackColor = true;
			this->cbShowArmor->CheckedChanged += gcnew System::EventHandler(this, &Form1::cbShowArmor_CheckedChanged);
			// 
			// tabPage2
			// 
			this->tabPage2->Controls->Add(this->groupBox11);
			this->tabPage2->Controls->Add(this->groupBox10);
			this->tabPage2->Controls->Add(this->groupBox9);
			this->tabPage2->Controls->Add(this->groupBox7);
			this->tabPage2->Controls->Add(this->groupBox4);
			this->tabPage2->Controls->Add(this->groupBox2);
			this->tabPage2->Controls->Add(this->gmMoveFlags);
			this->tabPage2->Controls->Add(this->groupBox1);
			this->tabPage2->Controls->Add(this->gbAlpha);
			this->tabPage2->Controls->Add(this->gbActionFlags);
			this->tabPage2->Controls->Add(this->gbMaterial);
			this->tabPage2->Location = System::Drawing::Point(4, 22);
			this->tabPage2->Name = L"tabPage2";
			this->tabPage2->Padding = System::Windows::Forms::Padding(3);
			this->tabPage2->Size = System::Drawing::Size(660, 264);
			this->tabPage2->TabIndex = 1;
			this->tabPage2->Text = L"Ext info";
			this->tabPage2->UseVisualStyleBackColor = true;
			this->tabPage2->Click += gcnew System::EventHandler(this, &Form1::tabPage2_Click);
			// 
			// groupBox11
			// 
			this->groupBox11->Controls->Add(this->rbLightSouth);
			this->groupBox11->Controls->Add(this->rbLightEast);
			this->groupBox11->Controls->Add(this->rbLightNorth);
			this->groupBox11->Controls->Add(this->rbLightWest);
			this->groupBox11->Controls->Add(this->rbLightWestEast);
			this->groupBox11->Controls->Add(this->rbLightNorthSouth);
			this->groupBox11->Location = System::Drawing::Point(429, 55);
			this->groupBox11->Name = L"groupBox11";
			this->groupBox11->Size = System::Drawing::Size(113, 116);
			this->groupBox11->TabIndex = 64;
			this->groupBox11->TabStop = false;
			this->groupBox11->Text = L"Corner";
			// 
			// rbLightSouth
			// 
			this->rbLightSouth->AutoSize = true;
			this->rbLightSouth->Location = System::Drawing::Point(14, 90);
			this->rbLightSouth->Name = L"rbLightSouth";
			this->rbLightSouth->Size = System::Drawing::Size(53, 17);
			this->rbLightSouth->TabIndex = 45;
			this->rbLightSouth->Text = L"South";
			this->rbLightSouth->UseVisualStyleBackColor = true;
			// 
			// rbLightEast
			// 
			this->rbLightEast->AutoSize = true;
			this->rbLightEast->Location = System::Drawing::Point(14, 75);
			this->rbLightEast->Name = L"rbLightEast";
			this->rbLightEast->Size = System::Drawing::Size(46, 17);
			this->rbLightEast->TabIndex = 44;
			this->rbLightEast->Text = L"East";
			this->rbLightEast->UseVisualStyleBackColor = true;
			// 
			// rbLightNorth
			// 
			this->rbLightNorth->AutoSize = true;
			this->rbLightNorth->Location = System::Drawing::Point(14, 60);
			this->rbLightNorth->Name = L"rbLightNorth";
			this->rbLightNorth->Size = System::Drawing::Size(52, 17);
			this->rbLightNorth->TabIndex = 43;
			this->rbLightNorth->Text = L"North";
			this->rbLightNorth->UseVisualStyleBackColor = true;
			// 
			// rbLightWest
			// 
			this->rbLightWest->AutoSize = true;
			this->rbLightWest->Location = System::Drawing::Point(14, 45);
			this->rbLightWest->Name = L"rbLightWest";
			this->rbLightWest->Size = System::Drawing::Size(50, 17);
			this->rbLightWest->TabIndex = 42;
			this->rbLightWest->Text = L"West";
			this->rbLightWest->UseVisualStyleBackColor = true;
			// 
			// rbLightWestEast
			// 
			this->rbLightWestEast->AutoSize = true;
			this->rbLightWestEast->Location = System::Drawing::Point(14, 30);
			this->rbLightWestEast->Name = L"rbLightWestEast";
			this->rbLightWestEast->Size = System::Drawing::Size(75, 17);
			this->rbLightWestEast->TabIndex = 41;
			this->rbLightWestEast->Text = L"West/East";
			this->rbLightWestEast->UseVisualStyleBackColor = true;
			// 
			// rbLightNorthSouth
			// 
			this->rbLightNorthSouth->AutoSize = true;
			this->rbLightNorthSouth->Checked = true;
			this->rbLightNorthSouth->Location = System::Drawing::Point(14, 15);
			this->rbLightNorthSouth->Name = L"rbLightNorthSouth";
			this->rbLightNorthSouth->Size = System::Drawing::Size(84, 17);
			this->rbLightNorthSouth->TabIndex = 40;
			this->rbLightNorthSouth->TabStop = true;
			this->rbLightNorthSouth->Text = L"North/South";
			this->rbLightNorthSouth->UseVisualStyleBackColor = true;
			// 
			// groupBox10
			// 
			this->groupBox10->Controls->Add(this->cbDisableEgg);
			this->groupBox10->Location = System::Drawing::Point(429, 9);
			this->groupBox10->Name = L"groupBox10";
			this->groupBox10->Size = System::Drawing::Size(113, 45);
			this->groupBox10->TabIndex = 63;
			this->groupBox10->TabStop = false;
			this->groupBox10->Text = L"Transparent egg";
			// 
			// cbDisableEgg
			// 
			this->cbDisableEgg->AutoSize = true;
			this->cbDisableEgg->Location = System::Drawing::Point(7, 19);
			this->cbDisableEgg->Name = L"cbDisableEgg";
			this->cbDisableEgg->Size = System::Drawing::Size(78, 17);
			this->cbDisableEgg->TabIndex = 29;
			this->cbDisableEgg->Text = L"DisableEgg";
			this->cbDisableEgg->UseVisualStyleBackColor = true;
			// 
			// groupBox9
			// 
			this->groupBox9->Controls->Add(this->numSlot);
			this->groupBox9->Location = System::Drawing::Point(602, 147);
			this->groupBox9->Name = L"groupBox9";
			this->groupBox9->Size = System::Drawing::Size(52, 36);
			this->groupBox9->TabIndex = 62;
			this->groupBox9->TabStop = false;
			this->groupBox9->Text = L"Slot";
			// 
			// numSlot
			// 
			this->numSlot->Location = System::Drawing::Point(9, 12);
			this->numSlot->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {255, 0, 0, 0});
			this->numSlot->Name = L"numSlot";
			this->numSlot->Size = System::Drawing::Size(37, 20);
			this->numSlot->TabIndex = 0;
			// 
			// groupBox7
			// 
			this->groupBox7->Controls->Add(this->numDrawPosOffsY);
			this->groupBox7->Location = System::Drawing::Point(602, 222);
			this->groupBox7->Name = L"groupBox7";
			this->groupBox7->Size = System::Drawing::Size(52, 36);
			this->groupBox7->TabIndex = 60;
			this->groupBox7->TabStop = false;
			this->groupBox7->Text = L"DrawY";
			// 
			// numDrawPosOffsY
			// 
			this->numDrawPosOffsY->Location = System::Drawing::Point(8, 13);
			this->numDrawPosOffsY->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {100000, 0, 0, 0});
			this->numDrawPosOffsY->Name = L"numDrawPosOffsY";
			this->numDrawPosOffsY->Size = System::Drawing::Size(37, 20);
			this->numDrawPosOffsY->TabIndex = 3;
			// 
			// groupBox4
			// 
			this->groupBox4->Controls->Add(this->txtSoundId);
			this->groupBox4->Location = System::Drawing::Point(602, 184);
			this->groupBox4->Name = L"groupBox4";
			this->groupBox4->Size = System::Drawing::Size(52, 36);
			this->groupBox4->TabIndex = 58;
			this->groupBox4->TabStop = false;
			this->groupBox4->Text = L"Sound";
			// 
			// txtSoundId
			// 
			this->txtSoundId->Location = System::Drawing::Point(5, 11);
			this->txtSoundId->MaxLength = 1;
			this->txtSoundId->Name = L"txtSoundId";
			this->txtSoundId->Size = System::Drawing::Size(42, 20);
			this->txtSoundId->TabIndex = 1;
			// 
			// tabPage3
			// 
			this->tabPage3->Controls->Add(this->groupBox8);
			this->tabPage3->Controls->Add(this->groupBox6);
			this->tabPage3->Location = System::Drawing::Point(4, 22);
			this->tabPage3->Name = L"tabPage3";
			this->tabPage3->Size = System::Drawing::Size(660, 264);
			this->tabPage3->TabIndex = 2;
			this->tabPage3->Text = L"More ext info";
			this->tabPage3->UseVisualStyleBackColor = true;
			// 
			// groupBox8
			// 
			this->groupBox8->Controls->Add(this->label3);
			this->groupBox8->Controls->Add(this->label2);
			this->groupBox8->Controls->Add(this->txtScriptFunc);
			this->groupBox8->Controls->Add(this->txtScriptModule);
			this->groupBox8->Location = System::Drawing::Point(9, 7);
			this->groupBox8->Name = L"groupBox8";
			this->groupBox8->Size = System::Drawing::Size(223, 95);
			this->groupBox8->TabIndex = 63;
			this->groupBox8->TabStop = false;
			this->groupBox8->Text = L"Script";
			// 
			// label3
			// 
			this->label3->AutoSize = true;
			this->label3->Location = System::Drawing::Point(43, 47);
			this->label3->Name = L"label3";
			this->label3->Size = System::Drawing::Size(48, 13);
			this->label3->TabIndex = 3;
			this->label3->Text = L"Function";
			// 
			// label2
			// 
			this->label2->AutoSize = true;
			this->label2->Location = System::Drawing::Point(46, 12);
			this->label2->Name = L"label2";
			this->label2->Size = System::Drawing::Size(41, 13);
			this->label2->TabIndex = 2;
			this->label2->Text = L"Module";
			// 
			// txtScriptFunc
			// 
			this->txtScriptFunc->Location = System::Drawing::Point(6, 61);
			this->txtScriptFunc->Name = L"txtScriptFunc";
			this->txtScriptFunc->Size = System::Drawing::Size(210, 20);
			this->txtScriptFunc->TabIndex = 1;
			// 
			// txtScriptModule
			// 
			this->txtScriptModule->Location = System::Drawing::Point(6, 27);
			this->txtScriptModule->Name = L"txtScriptModule";
			this->txtScriptModule->Size = System::Drawing::Size(210, 20);
			this->txtScriptModule->TabIndex = 0;
			// 
			// groupBox6
			// 
			this->groupBox6->Controls->Add(this->label74);
			this->groupBox6->Controls->Add(this->label59);
			this->groupBox6->Controls->Add(this->label45);
			this->groupBox6->Controls->Add(this->numHideAnim1);
			this->groupBox6->Controls->Add(this->numHideAnim0);
			this->groupBox6->Controls->Add(this->numShowAnim1);
			this->groupBox6->Controls->Add(this->numShowAnim0);
			this->groupBox6->Controls->Add(this->numStayAnim1);
			this->groupBox6->Controls->Add(this->numStayAnim0);
			this->groupBox6->Controls->Add(this->cbShowAnimExt);
			this->groupBox6->Controls->Add(this->numAnimWaitRndMax);
			this->groupBox6->Controls->Add(this->numAnimWaitRndMin);
			this->groupBox6->Controls->Add(this->label113);
			this->groupBox6->Controls->Add(this->numAnimWaitBase);
			this->groupBox6->Controls->Add(this->cbShowAnim);
			this->groupBox6->Location = System::Drawing::Point(513, 7);
			this->groupBox6->Name = L"groupBox6";
			this->groupBox6->Size = System::Drawing::Size(144, 251);
			this->groupBox6->TabIndex = 62;
			this->groupBox6->TabStop = false;
			this->groupBox6->Text = L"Animation";
			// 
			// label74
			// 
			this->label74->AutoSize = true;
			this->label74->Location = System::Drawing::Point(27, 196);
			this->label74->Name = L"label74";
			this->label74->Size = System::Drawing::Size(64, 13);
			this->label74->TabIndex = 15;
			this->label74->Text = L"Hide frames";
			// 
			// label59
			// 
			this->label59->AutoSize = true;
			this->label59->Location = System::Drawing::Point(25, 160);
			this->label59->Name = L"label59";
			this->label59->Size = System::Drawing::Size(69, 13);
			this->label59->TabIndex = 14;
			this->label59->Text = L"Show frames";
			// 
			// label45
			// 
			this->label45->AutoSize = true;
			this->label45->Location = System::Drawing::Point(25, 121);
			this->label45->Name = L"label45";
			this->label45->Size = System::Drawing::Size(65, 13);
			this->label45->TabIndex = 13;
			this->label45->Text = L"Stay frames";
			// 
			// numHideAnim1
			// 
			this->numHideAnim1->Location = System::Drawing::Point(59, 212);
			this->numHideAnim1->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65000, 0, 0, 0});
			this->numHideAnim1->Name = L"numHideAnim1";
			this->numHideAnim1->Size = System::Drawing::Size(44, 20);
			this->numHideAnim1->TabIndex = 12;
			// 
			// numHideAnim0
			// 
			this->numHideAnim0->Location = System::Drawing::Point(18, 212);
			this->numHideAnim0->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65000, 0, 0, 0});
			this->numHideAnim0->Name = L"numHideAnim0";
			this->numHideAnim0->Size = System::Drawing::Size(44, 20);
			this->numHideAnim0->TabIndex = 11;
			// 
			// numShowAnim1
			// 
			this->numShowAnim1->Location = System::Drawing::Point(59, 175);
			this->numShowAnim1->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65000, 0, 0, 0});
			this->numShowAnim1->Name = L"numShowAnim1";
			this->numShowAnim1->Size = System::Drawing::Size(44, 20);
			this->numShowAnim1->TabIndex = 10;
			// 
			// numShowAnim0
			// 
			this->numShowAnim0->Location = System::Drawing::Point(18, 175);
			this->numShowAnim0->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65000, 0, 0, 0});
			this->numShowAnim0->Name = L"numShowAnim0";
			this->numShowAnim0->Size = System::Drawing::Size(44, 20);
			this->numShowAnim0->TabIndex = 9;
			// 
			// numStayAnim1
			// 
			this->numStayAnim1->Location = System::Drawing::Point(59, 138);
			this->numStayAnim1->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65000, 0, 0, 0});
			this->numStayAnim1->Name = L"numStayAnim1";
			this->numStayAnim1->Size = System::Drawing::Size(44, 20);
			this->numStayAnim1->TabIndex = 8;
			// 
			// numStayAnim0
			// 
			this->numStayAnim0->Location = System::Drawing::Point(18, 138);
			this->numStayAnim0->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65000, 0, 0, 0});
			this->numStayAnim0->Name = L"numStayAnim0";
			this->numStayAnim0->Size = System::Drawing::Size(44, 20);
			this->numStayAnim0->TabIndex = 7;
			// 
			// cbShowAnimExt
			// 
			this->cbShowAnimExt->AutoSize = true;
			this->cbShowAnimExt->Location = System::Drawing::Point(11, 104);
			this->cbShowAnimExt->Name = L"cbShowAnimExt";
			this->cbShowAnimExt->Size = System::Drawing::Size(91, 17);
			this->cbShowAnimExt->TabIndex = 6;
			this->cbShowAnimExt->Text = L"ShowAnimExt";
			this->cbShowAnimExt->UseVisualStyleBackColor = true;
			// 
			// numAnimWaitRndMax
			// 
			this->numAnimWaitRndMax->Location = System::Drawing::Point(95, 75);
			this->numAnimWaitRndMax->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65000, 0, 0, 0});
			this->numAnimWaitRndMax->Name = L"numAnimWaitRndMax";
			this->numAnimWaitRndMax->Size = System::Drawing::Size(44, 20);
			this->numAnimWaitRndMax->TabIndex = 5;
			// 
			// numAnimWaitRndMin
			// 
			this->numAnimWaitRndMin->Location = System::Drawing::Point(50, 75);
			this->numAnimWaitRndMin->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65000, 0, 0, 0});
			this->numAnimWaitRndMin->Name = L"numAnimWaitRndMin";
			this->numAnimWaitRndMin->Size = System::Drawing::Size(44, 20);
			this->numAnimWaitRndMin->TabIndex = 3;
			// 
			// label113
			// 
			this->label113->AutoSize = true;
			this->label113->Location = System::Drawing::Point(14, 35);
			this->label113->Name = L"label113";
			this->label113->Size = System::Drawing::Size(125, 39);
			this->label113->TabIndex = 2;
			this->label113->Text = L"Base+Random(Min,Max)\r\nin 1/100 second\r\nBase    Min         Max";
			// 
			// numAnimWaitBase
			// 
			this->numAnimWaitBase->Location = System::Drawing::Point(5, 75);
			this->numAnimWaitBase->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65000, 0, 0, 0});
			this->numAnimWaitBase->Name = L"numAnimWaitBase";
			this->numAnimWaitBase->Size = System::Drawing::Size(44, 20);
			this->numAnimWaitBase->TabIndex = 1;
			// 
			// cbShowAnim
			// 
			this->cbShowAnim->AutoSize = true;
			this->cbShowAnim->Location = System::Drawing::Point(10, 17);
			this->cbShowAnim->Name = L"cbShowAnim";
			this->cbShowAnim->Size = System::Drawing::Size(75, 17);
			this->cbShowAnim->TabIndex = 0;
			this->cbShowAnim->Text = L"ShowAnim";
			this->cbShowAnim->UseVisualStyleBackColor = true;
			// 
			// cmbLog
			// 
			this->cmbLog->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
			this->cmbLog->FormattingEnabled = true;
			this->cmbLog->Location = System::Drawing::Point(12, 505);
			this->cmbLog->Name = L"cmbLog";
			this->cmbLog->Size = System::Drawing::Size(668, 21);
			this->cmbLog->TabIndex = 47;
			// 
			// Form1
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(689, 532);
			this->Controls->Add(this->cbFix);
			this->Controls->Add(this->tabControl1);
			this->Controls->Add(this->cmbLog);
			this->Controls->Add(this->tcType);
			this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedSingle;
			this->Icon = (cli::safe_cast<System::Drawing::Icon^  >(resources->GetObject(L"$this.Icon")));
			this->MaximizeBox = false;
			this->Name = L"Form1";
			this->Text = L"====";
			this->Load += gcnew System::EventHandler(this, &Form1::Form1_Load);
			this->tcType->ResumeLayout(false);
			this->tArmor->ResumeLayout(false);
			this->tArmor->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTexpl))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTemp))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTelectr))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTplasma))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTfire))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTlaser))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDTnorm))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRexpl))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRemp))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRelectr))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRplasma))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRfire))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRlaser))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmDRnorm))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmPerk))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmAC))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmTypeFemale))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nArmTypeMale))->EndInit();
			this->tDrug->ResumeLayout(false);
			this->tDrug->PerformLayout();
			this->tWeapon->ResumeLayout(false);
			this->txtWeapSApic->ResumeLayout(false);
			this->cbWeapMain->ResumeLayout(false);
			this->cbWeapMain->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapUnarmedCritBonus))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapUnarmedPriory))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapMinUnarmed))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapMinLevel))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapUnarmedTree))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapMinAg))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapMinSt))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapPerk))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numWeapDefAmmo))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapCrFail))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapAnim1))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapCaliber))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapHolder))->EndInit();
			this->tabPage11->ResumeLayout(false);
			this->tabPage11->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAtime))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAEffect))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAdistmax))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAdmgmin))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAdmgmax))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAanim2))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAround))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapPAskill))->EndInit();
			this->tabPage12->ResumeLayout(false);
			this->tabPage12->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAtime))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAEffect))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAdistmax))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAdmgmin))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAdmgmax))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAanim2))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAround))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapSAskill))->EndInit();
			this->tabPage13->ResumeLayout(false);
			this->tabPage13->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAtime))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAEffect))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAdistmax))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAdmgmin))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAdmgmax))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAanim2))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAround))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeapTAskill))->EndInit();
			this->tAmmo->ResumeLayout(false);
			this->tAmmo->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAmmoDmgDiv))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAmmoDmgMult))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAmmoDRMod))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAmmoACMod))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nAmmoCaliber))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nAmmoSCount))->EndInit();
			this->tContainer->ResumeLayout(false);
			this->tContainer->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nContSize))->EndInit();
			this->tMiscEx->ResumeLayout(false);
			this->tMiscEx->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2SV3))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2SV2))->EndInit();
			this->groupBox5->ResumeLayout(false);
			this->groupBox5->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarEntire))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarFuelConsumption))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarRunToBreak))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarFuelTank))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarCritCapacity))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarWearConsumption))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarNegotiability))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2CarSpeed))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMisc2SV1))->EndInit();
			this->tDoor->ResumeLayout(false);
			this->tDoor->PerformLayout();
			this->tGrid->ResumeLayout(false);
			this->groupBox3->ResumeLayout(false);
			this->groupBox3->PerformLayout();
			this->gmMoveFlags->ResumeLayout(false);
			this->gmMoveFlags->PerformLayout();
			this->gbAlpha->ResumeLayout(false);
			this->gbAlpha->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numBlue))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numGreen))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numRed))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAlpha))->EndInit();
			this->gbActionFlags->ResumeLayout(false);
			this->gbActionFlags->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nLightDist))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nLightInt))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWeight))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nSize))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nCost))->EndInit();
			this->gbMaterial->ResumeLayout(false);
			this->gbMaterial->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPid))->EndInit();
			this->groupBox1->ResumeLayout(false);
			this->groupBox1->PerformLayout();
			this->groupBox2->ResumeLayout(false);
			this->groupBox2->PerformLayout();
			this->tabControl1->ResumeLayout(false);
			this->tabPage1->ResumeLayout(false);
			this->tabPage1->PerformLayout();
			this->tabPage2->ResumeLayout(false);
			this->groupBox11->ResumeLayout(false);
			this->groupBox11->PerformLayout();
			this->groupBox10->ResumeLayout(false);
			this->groupBox10->PerformLayout();
			this->groupBox9->ResumeLayout(false);
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numSlot))->EndInit();
			this->groupBox7->ResumeLayout(false);
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numDrawPosOffsY))->EndInit();
			this->groupBox4->ResumeLayout(false);
			this->groupBox4->PerformLayout();
			this->tabPage3->ResumeLayout(false);
			this->groupBox8->ResumeLayout(false);
			this->groupBox8->PerformLayout();
			this->groupBox6->ResumeLayout(false);
			this->groupBox6->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numHideAnim1))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numHideAnim0))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numShowAnim1))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numShowAnim0))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numStayAnim1))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numStayAnim0))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAnimWaitRndMax))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAnimWaitRndMin))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numAnimWaitBase))->EndInit();
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion

private: System::Void label13_Click(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void radioButton7_CheckedChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void tabPage1_Click(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void label22_Click(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void label16_Click(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void tabPage3_Click(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void numericUpDown35_ValueChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void label37_Click(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void nDrugAmount2s0_ValueChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void nDrugAddiction_ValueChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void numericUpDown64_ValueChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void numericUpDown65_ValueChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void tabControl2_SelectedIndexChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void radioButton24_CheckedChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void nWeapCaliber_ValueChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void nWeapTAround_ValueChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void nWeapTAtime_ValueChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void nAmmoCaliber_ValueChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void cbFix_CheckedChanged(System::Object^  sender, System::EventArgs^  e){}
private: System::Void tcType_SelectedIndexChanged(System::Object^  sender, System::EventArgs^  e){}
private: System::Void tcType_Selected(System::Object^  sender, System::Windows::Forms::TabControlEventArgs^  e){}
private: System::Void label27_Click(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void numericUpDown29_ValueChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void radioButton11_CheckedChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void radioButton13_CheckedChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void radioButton15_CheckedChanged(System::Object^  sender, System::EventArgs^  e) {}

#pragma endregion

#pragma region Main

private: System::Void Form1_Load(System::Object^  sender, System::EventArgs^  e)
{
	LogToFile(".\\ObjectEditor.log");
	srand(GetTickCount());
	setlocale(LC_ALL,"Russian");

	// About
	this->Text="FOnline Proto Object Editor "+String(OBJECT_EDITOR_VERSION_STR).ToString();
	if(!ItemMngr.Init()) Close();
	char path[2048];

	// FOnames
	Log("Names...");
	//GetPrivateProfileString("OPTIONS","NamesFolder",".\\data\\",path,2048,".\\ObjectEditor.cfg");
	FONames::GenerateFoNames(PT_SERVER_DATA);

	// Msg
	Log("foobj.msg...");
	GetPrivateProfileString("OPTIONS","FoobjMsg","foobj.msg",path,2048,".\\ObjectEditor.cfg");
	if(MsgObj.LoadMsgFile(path)<0)
	{
		MessageBox::Show("FOOBJ.MSG not found");
		Close();
	}

	cbFix->Checked=false;

	/*DirectoryInfo^ dir=gcnew DirectoryInfo(".\\protos\\");
	if(!dir->Exists) return;

	array<FileInfo^>^diArr = dir->GetFiles();
	ProtoItemVec protosAll;
	Collections::IEnumerator^ myEnum = diArr->GetEnumerator();
	while ( myEnum->MoveNext() )
	{
		FileInfo^ dri = safe_cast<FileInfo^>(myEnum->Current);
		String^ name=dri->DirectoryName+"\\"+dri->Name;

		ProtoItemVec protos;
		if(!ItemMngr.LoadProtosNewFormat(protos,ToAnsi(name)))
		{
			Log("File not found.");
			return;
		}
		for(int i=0;i<protos.size();i++) protosAll.push_back(protos[i]);
	}

	ItemMngr.SaveProtosNewFormat(protosAll,"2238.txt");

	Close();*/

	Log("Load complete.");
}

private: System::Void tcType_Selecting(System::Object^  sender, System::Windows::Forms::TabControlCancelEventArgs^  e)
{
	if(cbFix->Checked==true) e->Cancel=true;
}

private: System::Void lbList_DoubleClick(System::Object^  sender, System::EventArgs^  e)
{
	int count=lbList->SelectedIndex;
	if(count==-1) return;

	ProtoItem* proto=NULL;

#define LIST_PARSE(comp,proto_type,str) \
	if(comp->Checked==true)\
	{\
		if(count>=ItemMngr.GetProtos(proto_type).size()) count-=ItemMngr.GetProtos(proto_type).size();\
		else {proto=&ItemMngr.GetProtos(proto_type)[count]; break;}\
	}

	BREAK_BEGIN
		LIST_PARSE(cbShowArmor,ITEM_TYPE_ARMOR,"Armor ");
		LIST_PARSE(cbShowDrug,ITEM_TYPE_DRUG,"Drug ");
		LIST_PARSE(cbShowWeapon,ITEM_TYPE_WEAPON,"Weapon ");
		LIST_PARSE(cbShowAmmo,ITEM_TYPE_AMMO,"Ammo ");
		LIST_PARSE(cbShowMisc,ITEM_TYPE_MISC,"Misc ");
		LIST_PARSE(cbShowMiscEx,ITEM_TYPE_MISC_EX,"MiscEx ");
		LIST_PARSE(cbShowKey,ITEM_TYPE_KEY,"Key ");
		LIST_PARSE(cbShowContainer,ITEM_TYPE_CONTAINER,"Container ");
		LIST_PARSE(cbShowDoor,ITEM_TYPE_DOOR,"Door ");
		LIST_PARSE(cbShowGrid,ITEM_TYPE_GRID,"Grid ");
		LIST_PARSE(cbShowGeneric,ITEM_TYPE_GENERIC,"Generic ");
		LIST_PARSE(cbShowWall,ITEM_TYPE_WALL,"Wall ");
	BREAK_END;

	ShowObject(proto);
}

void RefreshList()
{
	Int32 si=lbList->SelectedIndex;
	lbList->Items->Clear();

//===============================================================================================
#define LIST_PARSE2(comp,proto_type,str) \
	do{if(comp->Checked==true)\
	{\
		for(int i=0;i<ItemMngr.GetProtos(proto_type).size();i++)\
		{\
			WORD pid=ItemMngr.GetProtos(proto_type)[i].GetPid();\
			char* s=(char*)MsgObj.GetStr(pid*100);\
			lbList->Items->Add(str+(s?String(s).ToString():"No name")+"   "+pid.ToString());\
		}\
	}}while(0)
//===============================================================================================
	LIST_PARSE2(cbShowArmor,ITEM_TYPE_ARMOR,"Armor ");
	LIST_PARSE2(cbShowDrug,ITEM_TYPE_DRUG,"Drug ");
	LIST_PARSE2(cbShowWeapon,ITEM_TYPE_WEAPON,"Weapon ");
	LIST_PARSE2(cbShowAmmo,ITEM_TYPE_AMMO,"Ammo ");
	LIST_PARSE2(cbShowMisc,ITEM_TYPE_MISC,"Misc ");
	LIST_PARSE2(cbShowMiscEx,ITEM_TYPE_MISC_EX,"MiscEx ");
	LIST_PARSE2(cbShowKey,ITEM_TYPE_KEY,"Key ");
	LIST_PARSE2(cbShowContainer,ITEM_TYPE_CONTAINER,"Container ");
	LIST_PARSE2(cbShowDoor,ITEM_TYPE_DOOR,"Door ");
	LIST_PARSE2(cbShowGrid,ITEM_TYPE_GRID,"Grid ");
	LIST_PARSE2(cbShowGeneric,ITEM_TYPE_GENERIC,"Generic ");
	LIST_PARSE2(cbShowWall,ITEM_TYPE_WALL,"Wall ");

	if(si>=0 && si<lbList->Items->Count) lbList->SelectedIndex=si;
}

private: System::Void cbShowArmor_CheckedChanged(System::Object^  sender, System::EventArgs^  e)
{
	RefreshList();
}

private: System::Void bAdd_Click(System::Object^  sender, System::EventArgs^  e)
{
	if(nPid->Value==0 || nPid->Value>=MAX_ITEM_PROTOTYPES)
	{
		Log("Invalid pid.");
		return;
	}

	WORD pid=(WORD)nPid->Value;
	if(ItemMngr.IsInitProto(pid))
	{
		if(MessageBox::Show("Rewrite proto?","Object Editor",\
			MessageBoxButtons::OKCancel,MessageBoxIcon::Information)==\
			System::Windows::Forms::DialogResult::Cancel) return;

		ItemMngr.ClearProto(pid);
	}

	ProtoItem* proto=CompileObject();
	if(!proto) return;

	ProtoItemVec protos;
	protos.push_back(*proto);

	ItemMngr.ParseProtos(protos);
	RefreshList();
	cbFix->Checked=true;
	Log("Proto saved successfully.");
}

#pragma endregion

#pragma region Load One

private: System::Void bSave_Click(System::Object^  sender, System::EventArgs^  e)
{
	SaveFileDialog^ dlg=gcnew SaveFileDialog;
	//dlg->InitialDirectory=String(path_dir).ToString()+String(path_scripts).ToString();
	dlg->Filter="FO proto object files|*.fopro";
	dlg->RestoreDirectory=true;

	if(dlg->ShowDialog()!=System::Windows::Forms::DialogResult::OK) return;

	ItemMngr.SaveProtos(ToAnsi(dlg->FileName));
	Log("Protos saved successfully.");
}

private: System::Void bSaveList_Click(System::Object^  sender, System::EventArgs^  e)
{
	SaveFileDialog^ dlg=gcnew SaveFileDialog;
	//dlg->InitialDirectory=String(path_dir).ToString()+String(path_scripts).ToString();
	dlg->Filter="FO proto object files|*.fopro";
	dlg->RestoreDirectory=true;

	if(dlg->ShowDialog()!=System::Windows::Forms::DialogResult::OK) return;

//===============================================================================================
#define SAVE_LIST(comp,obj_type) \
	do{if(comp->Checked==true)\
	{\
		ProtoItemVec add=ItemMngr.GetProtos(obj_type);\
		for(size_t i=0,j=add.size();i<j;i++) protos.push_back(add[i]);\
	}}while(0)
//===============================================================================================
	ProtoItemVec protos;
	SAVE_LIST(cbShowArmor,ITEM_TYPE_ARMOR);
	SAVE_LIST(cbShowDrug,ITEM_TYPE_DRUG);
	SAVE_LIST(cbShowWeapon,ITEM_TYPE_WEAPON);
	SAVE_LIST(cbShowAmmo,ITEM_TYPE_AMMO);
	SAVE_LIST(cbShowMisc,ITEM_TYPE_MISC);
	SAVE_LIST(cbShowMiscEx,ITEM_TYPE_MISC_EX);
	SAVE_LIST(cbShowKey,ITEM_TYPE_KEY);
	SAVE_LIST(cbShowContainer,ITEM_TYPE_CONTAINER);
	SAVE_LIST(cbShowDoor,ITEM_TYPE_DOOR);
	SAVE_LIST(cbShowGrid,ITEM_TYPE_GRID);
	SAVE_LIST(cbShowGeneric,ITEM_TYPE_GENERIC);
	SAVE_LIST(cbShowWall,ITEM_TYPE_WALL);

	if(protos.size()!=0)
	{
		ItemMngr.SaveProtos(protos,ToAnsi(dlg->FileName));
		Log("Protos saved successfully.");
	}
}

private: System::Void bSave1_Click(System::Object^  sender, System::EventArgs^  e)
{
	ProtoItem* obj=CompileObject();
	if(!obj) return;

	SaveFileDialog^ dlg=gcnew SaveFileDialog;

	//dlg->InitialDirectory=String(path_dir).ToString()+String(path_scripts).ToString();
	dlg->Filter="FO single proto object|*.fopro";
	dlg->RestoreDirectory=true;

	if(dlg->ShowDialog()!=System::Windows::Forms::DialogResult::OK) return;

	ProtoItemVec protos;
	protos.push_back(*obj);

	ItemMngr.SaveProtos(protos,ToAnsi(dlg->FileName));
	Log("Save success.");
}

private: System::Void bLoad_Click(System::Object^  sender, System::EventArgs^  e)
{
	OpenFileDialog^ dlg=gcnew OpenFileDialog;
	//dlg->InitialDirectory=String(path_dir).ToString()+String(path_scripts).ToString();
	dlg->Filter="FO proto object files|*.fopro";
	dlg->RestoreDirectory=true;

	if(dlg->ShowDialog()!=System::Windows::Forms::DialogResult::OK) return;

	ProtoItemVec protos;
	if(!ItemMngr.LoadProtos(protos,ToAnsi(dlg->FileName)))
	{
		Log("Unable to load protos.");
		return;
	}

	//ItemMngr.ClearProtos();
	ItemMngr.ParseProtos(protos);
	RefreshList();
	Log("Proto load successfully.");
}

private: System::Void bLoad1_Click(System::Object^  sender, System::EventArgs^  e)
{
	ProtoItem* obj=new ProtoItem;
	OpenFileDialog^ dlg=gcnew OpenFileDialog;
	//dlg->InitialDirectory=String(path_dir).ToString()+String(path_scripts).ToString();
	dlg->Filter="FO single proto object|*.fopro";
	dlg->RestoreDirectory=true;

	if(dlg->ShowDialog()!=System::Windows::Forms::DialogResult::OK) return;

	ProtoItemVec protos;
	if(!ItemMngr.LoadProtos(protos,ToAnsi(dlg->FileName)))
	{
		Log("File not found.");
		return;
	}

	ShowObject(&protos[0]);
	Log("Load success.");
}

#pragma endregion

#pragma region ShowObject

void ShowObject(ProtoItem* proto)
{
	cbFix->Checked=false;
	UINT pid=proto->GetPid();

	nPid->Value=(UINT)proto->GetPid();
	txtName->Text=String(MsgObj.GetStr(pid*100)).ToString();
	txtInfo->Text=String(MsgObj.GetStr(pid*100+1)).ToString();
	numSlot->Value=(UINT)proto->Slot;

	// Light
	cbLight->Checked=FLAG(proto->Flags,ITEM_LIGHT);
	cbColorizing->Checked=FLAG(proto->Flags,ITEM_COLORIZE);
	nLightDist->Value=(UINT)proto->LightDistance;
	nLightInt->Value=(UINT)(proto->LightIntensity>100?50:proto->LightIntensity);
	numAlpha->Value=(UINT)(proto->LightColor>>24)&0xFF;
	numRed->Value=(UINT)(proto->LightColor>>16)&0xFF;
	numGreen->Value=(UINT)(proto->LightColor>>8)&0xFF;
	numBlue->Value=(UINT)(proto->LightColor)&0xFF;
	cbDisableDir0->Checked=FLAG(proto->LightFlags,LIGHT_DISABLE_DIR(0));
	cbDisableDir1->Checked=FLAG(proto->LightFlags,LIGHT_DISABLE_DIR(1));
	cbDisableDir2->Checked=FLAG(proto->LightFlags,LIGHT_DISABLE_DIR(2));
	cbDisableDir3->Checked=FLAG(proto->LightFlags,LIGHT_DISABLE_DIR(3));
	cbDisableDir4->Checked=FLAG(proto->LightFlags,LIGHT_DISABLE_DIR(4));
	cbDisableDir5->Checked=FLAG(proto->LightFlags,LIGHT_DISABLE_DIR(5));
	cbLightGlobal->Checked=FLAG(proto->LightFlags,LIGHT_GLOBAL);
	cbLightInverse->Checked=FLAG(proto->LightFlags,LIGHT_INVERSE);

	switch(proto->Corner)
	{
	default:
	case CORNER_NORTH_SOUTH: rbLightNorthSouth->Checked=true; break;
	case CORNER_WEST: rbLightWest->Checked=true; break;
	case CORNER_EAST: rbLightEast->Checked=true; break;
	case CORNER_SOUTH: rbLightSouth->Checked=true; break;
	case CORNER_NORTH: rbLightNorth->Checked=true; break;
	case CORNER_EAST_WEST: rbLightWestEast->Checked=true; break;
	}

	cbDisableEgg->Checked=proto->DisableEgg;

	// Flags
	cbNoHighlight->Checked=FLAG(proto->Flags,ITEM_NO_HIGHLIGHT);
	cbHidden->Checked=FLAG(proto->Flags,ITEM_HIDDEN);
	cbFlat->Checked=FLAG(proto->Flags,ITEM_FLAT);
	cbNoBlock->Checked=FLAG(proto->Flags,ITEM_NO_BLOCK);
	cbMultiHex->Checked=FLAG(proto->Flags,ITEM_MULTI_HEX);
	cbShootThru->Checked=FLAG(proto->Flags,ITEM_SHOOT_THRU);
	cbLightThru->Checked=FLAG(proto->Flags,ITEM_LIGHT_THRU);
	cbWallTransEnd->Checked=FLAG(proto->Flags,ITEM_WALL_TRANS_END);
	cbTwoHands->Checked=FLAG(proto->Flags,ITEM_TWO_HANDS);
	cbBigGun->Checked=FLAG(proto->Flags,ITEM_BIG_GUN);
	cbAlwaysView->Checked=FLAG(proto->Flags,ITEM_ALWAYS_VIEW);
	cbGekk->Checked=FLAG(proto->Flags,ITEM_GECK);
	cbTrap->Checked=FLAG(proto->Flags,ITEM_TRAP);
	cbNoLightInfluence->Checked=FLAG(proto->Flags,ITEM_NO_LIGHT_INFLUENCE);
	cbNoLoot->Checked=FLAG(proto->Flags,ITEM_NO_LOOT);
	cbNoSteal->Checked=FLAG(proto->Flags,ITEM_NO_STEAL);
	cbGag->Checked=FLAG(proto->Flags,ITEM_GAG);
	cbCached->Checked=FLAG(proto->Flags,ITEM_CACHED);

	// Action Flags
	cbCanUse->Checked=FLAG(proto->Flags,ITEM_CAN_USE);
	cbCanPickUp->Checked=FLAG(proto->Flags,ITEM_CAN_PICKUP);
	cbCanUseOnSmtn->Checked=FLAG(proto->Flags,ITEM_CAN_USE_ON_SMTH);
	cbCanLook->Checked=FLAG(proto->Flags,ITEM_CAN_LOOK);
	cbCanTalk->Checked=FLAG(proto->Flags,ITEM_CAN_TALK);
	cbHasTimer->Checked=FLAG(proto->Flags,ITEM_HAS_TIMER);
	cbBadItem->Checked=FLAG(proto->Flags,ITEM_BAD_ITEM);
	nWeight->Value=(UINT)proto->Weight;
	nSize->Value=(UINT)proto->Volume;
	txtPicMap->Text=ToClrString(proto->PicMapStr);
	txtPicInv->Text=ToClrString(proto->PicInvStr);
	txtSoundId->Text=ByteToString(proto->SoundId);
	nCost->Value=(UINT)proto->Cost;

	switch(proto->Material)
	{
	default:
	case MATERIAL_GLASS: rbGlass->Checked=true; break;
	case MATERIAL_METAL: rbMetal->Checked=true; break;
	case MATERIAL_PLASTIC: rbPlastic->Checked=true; break;
	case MATERIAL_WOOD: rbWood->Checked=true; break;
	case MATERIAL_DIRT: rbDirt->Checked=true; break;
	case MATERIAL_STONE: rbStone->Checked=true; break;
	case MATERIAL_CEMENT: rbCement->Checked=true; break;
	case MATERIAL_LEATHER: rbLeather->Checked=true; break;
	}

	// Animation
	cbShowAnim->Checked=FLAG(proto->Flags,ITEM_SHOW_ANIM);
	numAnimWaitBase->Value=(UINT)proto->AnimWaitBase;
	numAnimWaitRndMin->Value=(UINT)proto->AnimWaitRndMin;
	numAnimWaitRndMax->Value=(UINT)proto->AnimWaitRndMax;
	cbShowAnimExt->Checked=FLAG(proto->Flags,ITEM_SHOW_ANIM_EXT);
	numStayAnim0->Value=(UINT)proto->AnimStay[0];
	numStayAnim1->Value=(UINT)proto->AnimStay[1];
	numShowAnim0->Value=(UINT)proto->AnimShow[0];
	numShowAnim1->Value=(UINT)proto->AnimShow[1];
	numHideAnim0->Value=(UINT)proto->AnimHide[0];
	numHideAnim1->Value=(UINT)proto->AnimHide[1];
	numDrawPosOffsY->Value=(char)proto->DrawPosOffsY;

	// Script
	txtScriptModule->Text=ToClrString(proto->ScriptModule);
	txtScriptFunc->Text=ToClrString(proto->ScriptFunc);

	switch(proto->GetType())
	{
	case ITEM_TYPE_ARMOR:
		tcType->SelectTab("tArmor");

		nArmTypeMale->Value=(UINT)proto->Armor.Anim0Male;
		nArmTypeFemale->Value=(UINT)proto->Armor.Anim0Female;

		nArmAC->Value=(UINT)proto->Armor.AC;
		nArmPerk->Value=(UINT)proto->Armor.Perk;
		
		nArmDRnorm->Value=(UINT)proto->Armor.DRNormal;
		nArmDRlaser->Value=(UINT)proto->Armor.DRLaser;
		nArmDRfire->Value=(UINT)proto->Armor.DRFire;
		nArmDRplasma->Value=(UINT)proto->Armor.DRPlasma;
		nArmDRelectr->Value=(UINT)proto->Armor.DRElectr;
		nArmDRemp->Value=(UINT)proto->Armor.DREmp;
		nArmDRexpl->Value=(UINT)proto->Armor.DRExplode;
		
		nArmDTnorm->Value=(UINT)proto->Armor.DTNormal;
		nArmDTlaser->Value=(UINT)proto->Armor.DTLaser;
		nArmDTfire->Value=(UINT)proto->Armor.DTFire;
		nArmDTplasma->Value=(UINT)proto->Armor.DTPlasma;
		nArmDTelectr->Value=(UINT)proto->Armor.DTElectr;
		nArmDTemp->Value=(UINT)proto->Armor.DTEmp;
		nArmDTexpl->Value=(UINT)proto->Armor.DTExplode;
		break;
	case ITEM_TYPE_DRUG:
		tcType->SelectTab("tDrug");
		break;
	case ITEM_TYPE_WEAPON:
		tcType->SelectTab("tWeapon");

		cbWeapUnarmed->Checked=proto->Weapon.IsUnarmed;
		numWeapUnarmedTree->Value=(UINT)proto->Weapon.UnarmedTree;
		numWeapUnarmedPriory->Value=(UINT)proto->Weapon.UnarmedPriority;
		numWeapMinAg->Value=(UINT)proto->Weapon.UnarmedMinAgility;
		numWeapMinUnarmed->Value=(UINT)proto->Weapon.UnarmedMinUnarmed;
		numWeapMinLevel->Value=(UINT)proto->Weapon.UnarmedMinLevel;
		numWeapUnarmedCritBonus->Value=(UINT)proto->Weapon.UnarmedCriticalBonus;
		cbWeapArmorPiercing->Checked=proto->Weapon.UnarmedArmorPiercing;

		cbWeapIsNeedAct->Checked=proto->Weapon.IsNeedAct;
		nWeapAnim1->Value=(UINT)proto->Weapon.Anim1;
		nWeapHolder->Value=(UINT)proto->Weapon.VolHolder;
		nWeapCaliber->Value=(UINT)proto->Weapon.Caliber;
		numWeapDefAmmo->Value=(UINT)proto->Weapon.DefAmmo;
		nWeapCrFail->Value=(UINT)proto->Weapon.CrFailture;
		numWeapMinSt->Value=(UINT)proto->Weapon.MinSt;
		numWeapPerk->Value=(int)proto->Weapon.Perk;
		cbWeapPA->Checked=(proto->Weapon.CountAttack & 1)!=0;
		cbWeapSA->Checked=(proto->Weapon.CountAttack & 2)!=0;
		cbWeapTA->Checked=(proto->Weapon.CountAttack & 4)!=0;

		// 1
		nWeapPAskill->Value=(UINT)proto->Weapon.Skill[0];
		cmbWeapPAtypeattack->SelectedIndex=(UINT)proto->Weapon.DmgType[0]-1;
		txtWeapPApic->Text=ToClrString(proto->WeaponPicStr[0]);
		nWeapPAdmgmin->Value=(UINT)proto->Weapon.DmgMin[0];
		nWeapPAdmgmax->Value=(UINT)proto->Weapon.DmgMax[0];
		nWeapPAdistmax->Value=(UINT)proto->Weapon.MaxDist[0];
		nWeapPAEffect->Value=(UINT)proto->Weapon.Effect[0];
		nWeapPAanim2->Value=(UINT)proto->Weapon.Anim2[0];
		nWeapPAtime->Value=(UINT)proto->Weapon.Time[0];
		cbWeapPAaim->Checked=proto->Weapon.Aim[0]?true:false;
		nWeapPAround->Value=(UINT)proto->Weapon.Round[0];
		cbWeapPAremove->Checked=proto->Weapon.Remove[0]?true:false;
		txtWeapPASoundId->Text=ByteToString(proto->Weapon.SoundId[0]);

		// 2
		nWeapSAskill->Value=(UINT)proto->Weapon.Skill[1];
		cmbWeapSAtypeattack->SelectedIndex=(UINT)proto->Weapon.DmgType[1]-1;
		txtWeapSApic->Text=ToClrString(proto->WeaponPicStr[1]);
		nWeapSAdmgmin->Value=(UINT)proto->Weapon.DmgMin[1];
		nWeapSAdmgmax->Value=(UINT)proto->Weapon.DmgMax[1];
		nWeapSAdistmax->Value=(UINT)proto->Weapon.MaxDist[1];
		nWeapSAEffect->Value=(UINT)proto->Weapon.Effect[1];
		nWeapSAanim2->Value=(UINT)proto->Weapon.Anim2[1];
		nWeapSAtime->Value=(UINT)proto->Weapon.Time[1];
		cbWeapSAaim->Checked=proto->Weapon.Aim[1]?true:false;
		nWeapSAround->Value=(UINT)proto->Weapon.Round[1];
		cbWeapSAremove->Checked=proto->Weapon.Remove[1]?true:false;
		txtWeapSASoundId->Text=ByteToString(proto->Weapon.SoundId[1]);

		// 3
		nWeapTAskill->Value=(UINT)proto->Weapon.Skill[2];
		cmbWeapTAtypeattack->SelectedIndex=(UINT)proto->Weapon.DmgType[2]-1;
		txtWeapTApic->Text=ToClrString(proto->WeaponPicStr[2]);
		nWeapTAdmgmin->Value=(UINT)proto->Weapon.DmgMin[2];
		nWeapTAdmgmax->Value=(UINT)proto->Weapon.DmgMax[2];
		nWeapTAdistmax->Value=(UINT)proto->Weapon.MaxDist[2];
		nWeapTAEffect->Value=(UINT)proto->Weapon.Effect[2];
		nWeapTAanim2->Value=(UINT)proto->Weapon.Anim2[2];
		nWeapTAtime->Value=(UINT)proto->Weapon.Time[2];
		cbWeapTAaim->Checked=proto->Weapon.Aim[2]?true:false;
		nWeapTAround->Value=(UINT)proto->Weapon.Round[2];
		cbWeapTAremove->Checked=proto->Weapon.Remove[2]?true:false;
		txtWeapTASoundId->Text=ByteToString(proto->Weapon.SoundId[2]);

		cbWeapNoWear->Checked=proto->Weapon.NoWear?true:false;
		break;
	case ITEM_TYPE_AMMO:
		tcType->SelectTab("tAmmo");

		nAmmoSCount->Value=(UINT)proto->Ammo.StartCount;
		nAmmoCaliber->Value=(UINT)proto->Ammo.Caliber;
		numAmmoACMod->Value=proto->Ammo.ACMod;
		numAmmoDRMod->Value=proto->Ammo.DRMod;
		numAmmoDmgMult->Value=(UINT)proto->Ammo.DmgMult;
		numAmmoDmgDiv->Value=(UINT)proto->Ammo.DmgDiv;
		break;
	case ITEM_TYPE_MISC:
		tcType->SelectTab("tMisc");
		break;
	case ITEM_TYPE_MISC_EX:
		tcType->SelectTab("tMiscEx");

		numMisc2SV1->Value=(UINT)proto->MiscEx.StartVal1;
		numMisc2SV2->Value=(UINT)proto->MiscEx.StartVal2;
		numMisc2SV3->Value=(UINT)proto->MiscEx.StartVal3;

		// Car
		cbMisc2IsCar->Checked=proto->MiscEx.IsCar;
		numMisc2CarEntire->Value=(UINT)proto->MiscEx.Car.Entire;
		numMisc2CarSpeed->Value=(UINT)proto->MiscEx.Car.Speed;
		numMisc2CarNegotiability->Value=(UINT)proto->MiscEx.Car.Negotiability;
		numMisc2CarWearConsumption->Value=(UINT)proto->MiscEx.Car.WearConsumption;
		numMisc2CarCritCapacity->Value=(UINT)proto->MiscEx.Car.CritCapacity;
		numMisc2CarFuelTank->Value=(UINT)proto->MiscEx.Car.FuelTank;
		numMisc2CarRunToBreak->Value=(UINT)proto->MiscEx.Car.RunToBreak;
		numMisc2CarFuelConsumption->Value=(UINT)proto->MiscEx.Car.FuelConsumption;
		if(proto->MiscEx.Car.WalkType==GM_WALK_FLY) rbMisc2CarFly->Checked=true;
		else if(proto->MiscEx.Car.WalkType==GM_WALK_WATER) rbMisc2CarWater->Checked=true;
		else rbMisc2CarGround->Checked=true;

#define PARSE_CAR_BLOCKS(comp,func,max) \
	comp->Text="";\
	for(int i=0;i<max;i++)\
	{\
		BYTE dir=proto->MiscEx.Car.func(i);\
		if(dir==0xF) break;\
		comp->Text+=Int32(dir).ToString();\
	}
		PARSE_CAR_BLOCKS(txtMisc2CarBlocks,GetBlockDir,CAR_MAX_BLOCKS);
		PARSE_CAR_BLOCKS(txtMisc2CarBag1,GetBag0Dir,CAR_MAX_BAG_POSITION);
		PARSE_CAR_BLOCKS(txtMisc2CarBag2,GetBag1Dir,CAR_MAX_BAG_POSITION);
		break;
	case ITEM_TYPE_KEY:
		tcType->SelectTab("tKey");
		break;
	case ITEM_TYPE_CONTAINER:
		tcType->SelectTab("tContainer");

		nContSize->Value=(UINT)proto->Container.ContVolume;

		cbContCannotPickUp->Checked=proto->Container.CannotPickUp?true:false;
		cbContMagicHandsGrnd->Checked=proto->Container.MagicHandsGrnd?true:false;
		cbContChangeble->Checked=proto->Container.Changeble?true:false;
		cbContIsNoOpen->Checked=proto->Container.IsNoOpen?true:false;
		break;
	case ITEM_TYPE_DOOR:
		tcType->SelectTab("tDoor");

		cbDoorWalkThru->Checked=proto->Door.WalkThru?true:false;
		cbDoorIsNoOpen->Checked=proto->Door.IsNoOpen?true:false;
		break;
	case ITEM_TYPE_GRID:
		tcType->SelectTab("tGrid");

		if(proto->Grid.Type==GRID_EXITGRID) rbGridExitGrid->Checked=true;
		else if(proto->Grid.Type==GRID_STAIRS) rbGridStairs->Checked=true;
		else if(proto->Grid.Type==GRID_LADDERBOT) rbGridLadderBottom->Checked=true;
		else if(proto->Grid.Type==GRID_LADDERTOP) rbGridLadderTop->Checked=true;
		else if(proto->Grid.Type==GRID_ELEVATOR) rbGridElevatr->Checked=true;
		else rbGridExitGrid->Checked=true;
		break;
	case ITEM_TYPE_GENERIC:
		tcType->SelectTab("tGeneric");
		break;
	case ITEM_TYPE_WALL:
		tcType->SelectTab("tWall");
		break;
	default:
		break;
	}

	cbFix->Checked=true;
}

#pragma endregion

#pragma region CompileObject

ProtoItem* CompileObject()
{
	ProtoItem* proto=new ProtoItem();
	WORD pid=(WORD)nPid->Value;
	proto->SetPid(pid);
	proto->Slot=(BYTE)numSlot->Value;

	// Light
	if(cbLight->Checked) proto->Flags|=ITEM_LIGHT;
	if(cbColorizing->Checked) proto->Flags|=ITEM_COLORIZE;
	proto->LightDistance=(UINT)nLightDist->Value;
	proto->LightIntensity=(UINT)nLightInt->Value;
	proto->LightColor=((UINT)numAlpha->Value<<24)|((UINT)numRed->Value<<16)|((UINT)numGreen->Value<<8)|((UINT)numBlue->Value);

	if(rbLightWest->Checked==true) proto->Corner=CORNER_WEST;
	else if(rbLightEast->Checked==true) proto->Corner=CORNER_EAST;
	else if(rbLightSouth->Checked==true) proto->Corner=CORNER_SOUTH;
	else if(rbLightNorth->Checked==true) proto->Corner=CORNER_NORTH;
	else if(rbLightWestEast->Checked==true) proto->Corner=CORNER_EAST_WEST;
	else proto->Corner=CORNER_NORTH_SOUTH;
	
	nLightDist->Value=(UINT)proto->LightDistance;
	nLightInt->Value=(UINT)(proto->LightIntensity>100?50:proto->LightIntensity);
	numAlpha->Value=(UINT)(proto->LightColor>>24)&0xFF;
	numRed->Value=(UINT)(proto->LightColor>>16)&0xFF;
	numGreen->Value=(UINT)(proto->LightColor>>8)&0xFF;
	numBlue->Value=(UINT)(proto->LightColor)&0xFF;
	if(cbDisableDir0->Checked) proto->LightFlags|=LIGHT_DISABLE_DIR(0);
	if(cbDisableDir1->Checked) proto->LightFlags|=LIGHT_DISABLE_DIR(1);
	if(cbDisableDir2->Checked) proto->LightFlags|=LIGHT_DISABLE_DIR(2);
	if(cbDisableDir3->Checked) proto->LightFlags|=LIGHT_DISABLE_DIR(3);
	if(cbDisableDir4->Checked) proto->LightFlags|=LIGHT_DISABLE_DIR(4);
	if(cbDisableDir5->Checked) proto->LightFlags|=LIGHT_DISABLE_DIR(5);
	if(cbLightGlobal->Checked) proto->LightFlags|=LIGHT_GLOBAL;
	if(cbLightInverse->Checked) proto->LightFlags|=LIGHT_INVERSE;

	proto->DisableEgg=cbDisableEgg->Checked;

	// Flags
	if(cbNoHighlight->Checked) proto->Flags|=ITEM_NO_HIGHLIGHT;
	if(cbHidden->Checked) proto->Flags|=ITEM_HIDDEN;
	if(cbFlat->Checked) proto->Flags|=ITEM_FLAT;
	if(cbNoBlock->Checked) proto->Flags|=ITEM_NO_BLOCK;
	if(cbMultiHex->Checked) proto->Flags|=ITEM_MULTI_HEX;
	if(cbShootThru->Checked) proto->Flags|=ITEM_SHOOT_THRU;
	if(cbLightThru->Checked) proto->Flags|=ITEM_LIGHT_THRU;
	if(cbWallTransEnd->Checked) proto->Flags|=ITEM_WALL_TRANS_END;
	if(cbTwoHands->Checked) proto->Flags|=ITEM_TWO_HANDS;
	if(cbBigGun->Checked) proto->Flags|=ITEM_BIG_GUN;
	if(cbAlwaysView->Checked) proto->Flags|=ITEM_ALWAYS_VIEW;
	if(cbGekk->Checked) proto->Flags|=ITEM_GECK;
	if(cbTrap->Checked) proto->Flags|=ITEM_TRAP;
	if(cbNoLightInfluence->Checked) proto->Flags|=ITEM_NO_LIGHT_INFLUENCE;
	if(cbNoLoot->Checked) proto->Flags|=ITEM_NO_LOOT;
	if(cbNoSteal->Checked) proto->Flags|=ITEM_NO_STEAL;
	if(cbGag->Checked) proto->Flags|=ITEM_GAG;
	if(cbCached->Checked) proto->Flags|=ITEM_CACHED;

	// Action Flags
	if(cbCanUse->Checked) proto->Flags|=ITEM_CAN_USE;
	if(cbCanPickUp->Checked) proto->Flags|=ITEM_CAN_PICKUP;
	if(cbCanUseOnSmtn->Checked) proto->Flags|=ITEM_CAN_USE_ON_SMTH;
	if(cbCanLook->Checked) proto->Flags|=ITEM_CAN_LOOK;
	if(cbCanTalk->Checked) proto->Flags|=ITEM_CAN_TALK;
	if(cbHasTimer->Checked) proto->Flags|=ITEM_HAS_TIMER;
	if(cbBadItem->Checked) proto->Flags|=ITEM_BAD_ITEM;

	proto->Weight=(UINT)nWeight->Value;
	proto->Volume=(BYTE)nSize->Value;

	proto->PicMapStr=ToAnsi(txtPicMap->Text);
	proto->PicInvStr=ToAnsi(txtPicInv->Text);
	proto->SoundId=(BYTE)ToAnsi(txtSoundId->Text)[0];

	proto->Cost=(UINT)nCost->Value;

	if(rbGlass->Checked==true) proto->Material=MATERIAL_GLASS;
	else if(rbMetal->Checked==true) proto->Material=MATERIAL_METAL;
	else if(rbPlastic->Checked==true) proto->Material=MATERIAL_PLASTIC;
	else if(rbWood->Checked==true) proto->Material=MATERIAL_WOOD;
	else if(rbDirt->Checked==true) proto->Material=MATERIAL_DIRT;
	else if(rbStone->Checked==true) proto->Material=MATERIAL_STONE;
	else if(rbCement->Checked==true) proto->Material=MATERIAL_CEMENT;
	else if(rbLeather->Checked==true) proto->Material=MATERIAL_LEATHER;
	else proto->Material=MATERIAL_GLASS;

	// Animation
	if(cbShowAnim->Checked) proto->Flags|=ITEM_SHOW_ANIM;
	proto->AnimWaitBase=(UINT)numAnimWaitBase->Value;
	proto->AnimWaitRndMin=(UINT)numAnimWaitRndMin->Value;
	proto->AnimWaitRndMax=(UINT)numAnimWaitRndMax->Value;
	if(cbShowAnimExt->Checked) proto->Flags|=ITEM_SHOW_ANIM_EXT;
	proto->AnimStay[0]=(UINT)numStayAnim0->Value;
	proto->AnimStay[1]=(UINT)numStayAnim1->Value;
	proto->AnimShow[0]=(UINT)numShowAnim0->Value;
	proto->AnimShow[1]=(UINT)numShowAnim1->Value;
	proto->AnimHide[0]=(UINT)numHideAnim0->Value;
	proto->AnimHide[1]=(UINT)numHideAnim1->Value;
	proto->DrawPosOffsY=(int)numDrawPosOffsY->Value;

	// Script
	proto->ScriptModule=ToAnsi(txtScriptModule->Text);
	proto->ScriptFunc=ToAnsi(txtScriptFunc->Text);

	if(tcType->SelectedTab->Name=="tArmor")
	{
		proto->SetType(ITEM_TYPE_ARMOR);

		proto->Armor.Anim0Male=(BYTE)nArmTypeMale->Value;
		proto->Armor.Anim0Female=(BYTE)nArmTypeFemale->Value;

		proto->Armor.AC=(WORD)nArmAC->Value;
		proto->Armor.Perk=(BYTE)nArmPerk->Value;

		proto->Armor.DRNormal=(WORD)nArmDRnorm->Value;
		proto->Armor.DRLaser=(WORD)nArmDRlaser->Value;
		proto->Armor.DRFire=(WORD)nArmDRfire->Value;
		proto->Armor.DRPlasma=(WORD)nArmDRplasma->Value;
		proto->Armor.DRElectr=(WORD)nArmDRelectr->Value;
		proto->Armor.DREmp=(WORD)nArmDRemp->Value;
		proto->Armor.DRExplode=(WORD)nArmDRexpl->Value;

		proto->Armor.DTNormal=(WORD)nArmDTnorm->Value;
		proto->Armor.DTLaser=(WORD)nArmDTlaser->Value;
		proto->Armor.DTFire=(WORD)nArmDTfire->Value;
		proto->Armor.DTPlasma=(WORD)nArmDTplasma->Value;
		proto->Armor.DTElectr=(WORD)nArmDTelectr->Value;
		proto->Armor.DTEmp=(WORD)nArmDTemp->Value;
		proto->Armor.DTExplode=(WORD)nArmDTexpl->Value;
	}
	else if(tcType->SelectedTab->Name=="tDrug")
	{
		proto->SetType(ITEM_TYPE_DRUG);
	}
	else if(tcType->SelectedTab->Name=="tWeapon")
	{
		proto->SetType(ITEM_TYPE_WEAPON);

		proto->Weapon.IsUnarmed=cbWeapUnarmed->Checked;
		proto->Weapon.UnarmedTree=(BYTE)numWeapUnarmedTree->Value;
		proto->Weapon.UnarmedPriority=(BYTE)numWeapUnarmedPriory->Value;
		proto->Weapon.UnarmedMinAgility=(BYTE)numWeapMinAg->Value;
		proto->Weapon.UnarmedMinUnarmed=(BYTE)numWeapMinUnarmed->Value;
		proto->Weapon.UnarmedMinLevel=(BYTE)numWeapMinLevel->Value;
		proto->Weapon.UnarmedCriticalBonus=(BYTE)numWeapUnarmedCritBonus->Value;
		proto->Weapon.UnarmedArmorPiercing=cbWeapArmorPiercing->Checked;

		proto->Weapon.IsNeedAct=cbWeapIsNeedAct->Checked;
		proto->Weapon.Anim1=(BYTE)nWeapAnim1->Value;
		proto->Weapon.VolHolder=(WORD)nWeapHolder->Value;
		proto->Weapon.Caliber=(UINT)nWeapCaliber->Value;
		proto->Weapon.DefAmmo=(WORD)numWeapDefAmmo->Value;
		proto->Weapon.CrFailture=(BYTE)nWeapCrFail->Value;
		proto->Weapon.MinSt=(BYTE)numWeapMinSt->Value;
		proto->Weapon.Perk=(BYTE)numWeapPerk->Value;

		proto->Weapon.CountAttack=0;
		if(cbWeapPA->Checked) proto->Weapon.CountAttack+=1;
		if(cbWeapSA->Checked) proto->Weapon.CountAttack+=2;
		if(cbWeapTA->Checked) proto->Weapon.CountAttack+=4;

		// 1
		proto->Weapon.Skill[0]=(BYTE)nWeapPAskill->Value;
		proto->Weapon.DmgType[0]=(BYTE)cmbWeapPAtypeattack->SelectedIndex+1;
		proto->WeaponPicStr[0]=ToAnsi(txtWeapPApic->Text);
		proto->Weapon.DmgMin[0]=(WORD)nWeapPAdmgmin->Value;
		proto->Weapon.DmgMax[0]=(WORD)nWeapPAdmgmax->Value;
		proto->Weapon.MaxDist[0]=(WORD)nWeapPAdistmax->Value;
		proto->Weapon.Effect[0]=(WORD)nWeapPAEffect->Value;
		proto->Weapon.Anim2[0]=(BYTE)nWeapPAanim2->Value;
		proto->Weapon.Time[0]=(UINT)nWeapPAtime->Value;
		proto->Weapon.Aim[0]=(BYTE)cbWeapPAaim->Checked;
		proto->Weapon.Round[0]=(WORD)nWeapPAround->Value;
		proto->Weapon.Remove[0]=(BYTE)cbWeapPAremove->Checked;
		proto->Weapon.SoundId[0]=(BYTE)ToAnsi(txtWeapPASoundId->Text)[0];

		// 2
		proto->Weapon.Skill[1]=(BYTE)nWeapSAskill->Value;
		proto->Weapon.DmgType[1]=(BYTE)cmbWeapSAtypeattack->SelectedIndex+1;
		proto->WeaponPicStr[1]=ToAnsi(txtWeapSApic->Text);
		proto->Weapon.DmgMin[1]=(WORD)nWeapSAdmgmin->Value;
		proto->Weapon.DmgMax[1]=(WORD)nWeapSAdmgmax->Value;
		proto->Weapon.MaxDist[1]=(WORD)nWeapSAdistmax->Value;
		proto->Weapon.Effect[1]=(WORD)nWeapSAEffect->Value;
		proto->Weapon.Anim2[1]=(BYTE)nWeapSAanim2->Value;
		proto->Weapon.Time[1]=(UINT)nWeapSAtime->Value;
		proto->Weapon.Aim[1]=(BYTE)cbWeapSAaim->Checked;
		proto->Weapon.Round[1]=(WORD)nWeapSAround->Value;
		proto->Weapon.Remove[1]=(BYTE)cbWeapSAremove->Checked;
		proto->Weapon.SoundId[1]=(BYTE)ToAnsi(txtWeapSASoundId->Text)[0];

		// 3
		proto->Weapon.Skill[2]=(BYTE)nWeapTAskill->Value;
		proto->Weapon.DmgType[2]=(BYTE)cmbWeapTAtypeattack->SelectedIndex+1;
		proto->WeaponPicStr[2]=ToAnsi(txtWeapTApic->Text);
		proto->Weapon.DmgMin[2]=(WORD)nWeapTAdmgmin->Value;
		proto->Weapon.DmgMax[2]=(WORD)nWeapTAdmgmax->Value;
		proto->Weapon.MaxDist[2]=(WORD)nWeapTAdistmax->Value;
		proto->Weapon.Effect[2]=(WORD)nWeapTAEffect->Value;
		proto->Weapon.Anim2[2]=(BYTE)nWeapTAanim2->Value;
		proto->Weapon.Time[2]=(UINT)nWeapTAtime->Value;
		proto->Weapon.Aim[2]=(BYTE)cbWeapTAaim->Checked;
		proto->Weapon.Round[2]=(WORD)nWeapTAround->Value;
		proto->Weapon.Remove[2]=(BYTE)cbWeapTAremove->Checked;
		proto->Weapon.SoundId[2]=(BYTE)ToAnsi(txtWeapTASoundId->Text)[0];

		proto->Weapon.NoWear=(BYTE)cbWeapNoWear->Checked;
	}
	else if(tcType->SelectedTab->Name=="tAmmo")
	{
		proto->SetType(ITEM_TYPE_AMMO);

		proto->Ammo.StartCount=(WORD)nAmmoSCount->Value;
		proto->Ammo.Caliber=(UINT)nAmmoCaliber->Value;

		proto->Ammo.ACMod=(int)numAmmoACMod->Value;
		proto->Ammo.DRMod=(int)numAmmoDRMod->Value;
		proto->Ammo.DmgMult=(UINT)numAmmoDmgMult->Value;
		proto->Ammo.DmgDiv=(UINT)numAmmoDmgDiv->Value;
	}
	else if(tcType->SelectedTab->Name=="tMisc")
	{
		proto->SetType(ITEM_TYPE_MISC);
	}
	else if(tcType->SelectedTab->Name=="tMiscEx")
	{
		proto->SetType(ITEM_TYPE_MISC_EX);

		proto->MiscEx.StartVal1=(UINT)numMisc2SV1->Value;
		proto->MiscEx.StartVal2=(UINT)numMisc2SV2->Value;
		proto->MiscEx.StartVal3=(UINT)numMisc2SV3->Value;

		proto->MiscEx.IsCar=cbMisc2IsCar->Checked?1:0;
		proto->MiscEx.Car.Entire=(BYTE)numMisc2CarEntire->Value;
		proto->MiscEx.Car.Speed=(BYTE)numMisc2CarSpeed->Value;
		proto->MiscEx.Car.Negotiability=(BYTE)numMisc2CarNegotiability->Value;
		proto->MiscEx.Car.WearConsumption=(BYTE)numMisc2CarWearConsumption->Value;
		proto->MiscEx.Car.CritCapacity=(BYTE)numMisc2CarCritCapacity->Value;
		proto->MiscEx.Car.FuelTank=(WORD)numMisc2CarFuelTank->Value;
		proto->MiscEx.Car.RunToBreak=(WORD)numMisc2CarRunToBreak->Value;
		proto->MiscEx.Car.FuelConsumption=(BYTE)numMisc2CarFuelConsumption->Value;
		if(rbMisc2CarGround->Checked) proto->MiscEx.Car.WalkType=GM_WALK_GROUND;
		else if(rbMisc2CarFly->Checked) proto->MiscEx.Car.WalkType=GM_WALK_FLY;
		else if(rbMisc2CarWater->Checked) proto->MiscEx.Car.WalkType=GM_WALK_WATER;

#define PARSE_CAR_BLOCKS__(comp,data,max) \
	strcpy(str,ToAnsi(comp->Text));\
	memset(proto->MiscEx.Car.data,0xFF,sizeof(proto->MiscEx.Car.data));\
	for(int i=0;i<max;i++)\
	{\
		if(!str[i]) break;\
		BYTE dir=str[i]-'0';\
		if(dir==0xF) break;\
		if(i%2) proto->MiscEx.Car.data[i/2]=(proto->MiscEx.Car.data[i/2]&0xF0)|dir;\
		else proto->MiscEx.Car.data[i/2]=(proto->MiscEx.Car.data[i/2]&0x0F)|(dir<<4);\
	}
		char str[128];
		PARSE_CAR_BLOCKS__(txtMisc2CarBlocks,Blocks,CAR_MAX_BLOCKS);
		PARSE_CAR_BLOCKS__(txtMisc2CarBag1,Bag0,CAR_MAX_BAG_POSITION);
		PARSE_CAR_BLOCKS__(txtMisc2CarBag2,Bag1,CAR_MAX_BAG_POSITION);
	}
	else if(tcType->SelectedTab->Name=="tKey")
	{
		proto->SetType(ITEM_TYPE_KEY);
	}
	else if(tcType->SelectedTab->Name=="tContainer")
	{
		proto->SetType(ITEM_TYPE_CONTAINER);

		proto->Container.ContVolume=(WORD)nContSize->Value;
		proto->Container.CannotPickUp=cbContCannotPickUp->Checked;
		proto->Container.MagicHandsGrnd=cbContMagicHandsGrnd->Checked;
		proto->Container.Changeble=cbContChangeble->Checked;
		proto->Container.IsNoOpen=cbContIsNoOpen->Checked;
	}
	else if(tcType->SelectedTab->Name=="tDoor")
	{
		proto->SetType(ITEM_TYPE_DOOR);

		proto->Door.WalkThru=cbDoorWalkThru->Checked;
		//txtDoorUnknown->Text=Int32(proto->Door.Unknown).ToString();
		proto->Door.IsNoOpen=cbDoorIsNoOpen->Checked;
	}
	else if(tcType->SelectedTab->Name=="tGrid")
	{
		proto->SetType(ITEM_TYPE_GRID);

		if(rbGridExitGrid->Checked==true) proto->Grid.Type=GRID_EXITGRID;
		else if(rbGridStairs->Checked==true) proto->Grid.Type=GRID_STAIRS;
		else if(rbGridLadderBottom->Checked==true) proto->Grid.Type=GRID_LADDERBOT;
		else if(rbGridLadderTop->Checked==true) proto->Grid.Type=GRID_LADDERTOP;
		else if(rbGridElevatr->Checked==true) proto->Grid.Type=GRID_ELEVATOR;
		else proto->Grid.Type=GRID_EXITGRID;
	}
	else if(tcType->SelectedTab->Name=="tGeneric")
	{
		proto->SetType(ITEM_TYPE_GENERIC);
	}
	else if(tcType->SelectedTab->Name=="tWall")
	{
		proto->SetType(ITEM_TYPE_WALL);
	}

	return proto;
}

#pragma endregion

private: System::Void tabPage2_Click(System::Object^  sender, System::EventArgs^  e) {
		 }
private: System::Void radioButton2_CheckedChanged(System::Object^  sender, System::EventArgs^  e) {
		 }
private: System::Void lbList_SelectedIndexChanged(System::Object^  sender, System::EventArgs^  e) {
		 }
private: System::Void numUnarmedTree_ValueChanged(System::Object^  sender, System::EventArgs^  e) {
		 }

};
}

