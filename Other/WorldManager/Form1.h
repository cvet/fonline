/********************************************************************
	created:	22:05:2007   19:39

	author:		Anton Tvetinsky aka Cvet
	
	purpose:	
*********************************************************************/

#pragma once

#include "common.h"
#include "CFileMngr.h"
#include "FOProtoLocation.h"
#include "SQL.h"

CFileMngr fm;
CProtoLocation loc;
CSQL sql;

namespace World_manager {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

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
		//	WriteLog("%s\n",str.c_str());

		//	String^ nstr=(String^)str;

			cmbLog->Items->Add(str);
			cmbLog->Text=str;
		};

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
	private: System::Windows::Forms::TabControl^  tabControl1;
	protected: 
	private: System::Windows::Forms::TabPage^  tabPage1;
	private: System::Windows::Forms::TabPage^  tabPage2;
	private: System::Windows::Forms::Label^  label1;
	private: System::Windows::Forms::Panel^  pMap;

	private: System::Windows::Forms::Button^  bRight;
	private: System::Windows::Forms::Button^  bDown;
	private: System::Windows::Forms::Button^  bLeft;
	private: System::Windows::Forms::Button^  bUp;
	private: System::Windows::Forms::Button^  bUpload;
	private: System::Windows::Forms::ListBox^  lbLoc;
	private: System::Windows::Forms::ComboBox^  cmbLog;

	private: System::Windows::Forms::Label^  label3;
	private: System::Windows::Forms::Label^  label2;
	private: System::Windows::Forms::Button^  bAddCity;

	private: System::Windows::Forms::ListBox^  lbProtoLoc;
	private: System::Windows::Forms::Label^  label5;
	private: System::Windows::Forms::Label^  label4;
	private: System::Windows::Forms::NumericUpDown^  nWy;
	private: System::Windows::Forms::NumericUpDown^  nWx;
	private: System::Windows::Forms::Label^  label6;
	private: System::Windows::Forms::ListBox^  lbPProtoMaps;
	private: System::Windows::Forms::TabControl^  tabsPLoc;
	private: System::Windows::Forms::TabPage^  tCity;
	private: System::Windows::Forms::TabPage^  tEncaunter;
	private: System::Windows::Forms::TabPage^  tSpecial;
	private: System::Windows::Forms::TabPage^  tQuest;









	private: System::Windows::Forms::Label^  label9;
	private: System::Windows::Forms::Label^  label8;
	private: System::Windows::Forms::Label^  label7;
	private: System::Windows::Forms::Button^  button4;
	private: System::Windows::Forms::Button^  button3;
	private: System::Windows::Forms::Button^  button2;
	private: System::Windows::Forms::Button^  button1;
	private: System::Windows::Forms::Label^  label10;
	private: System::Windows::Forms::ListBox^  lbPLocMap;
	private: System::Windows::Forms::TextBox^  tPName;




	private: System::Windows::Forms::Button^  button7;
	private: System::Windows::Forms::Button^  button5;
	private: System::Windows::Forms::ComboBox^  cmbPRadius;
	private: System::Windows::Forms::RichTextBox^  tPInfo;
	private: System::Windows::Forms::NumericUpDown^  nPPid;

	private: System::Windows::Forms::Label^  label11;
	private: System::Windows::Forms::Label^  label14;
	private: System::Windows::Forms::Label^  label13;
	private: System::Windows::Forms::Label^  label12;
	private: System::Windows::Forms::NumericUpDown^  nPCLifeLevel;

	private: System::Windows::Forms::NumericUpDown^  nPCPopulation;

	private: System::Windows::Forms::NumericUpDown^  nPCSteal;
	private: System::Windows::Forms::NumericUpDown^  nPCStartMaps;

	private: System::Windows::Forms::Label^  label15;



private: System::Windows::Forms::Label^  label17;
private: System::Windows::Forms::NumericUpDown^  nPEMaxGroups;
private: System::Windows::Forms::GroupBox^  groupBox1;
private: System::Windows::Forms::CheckBox^  cbPEOcean;
private: System::Windows::Forms::CheckBox^  cbPEMountain;
private: System::Windows::Forms::CheckBox^  cbPEForest;
private: System::Windows::Forms::CheckBox^  cbPEWasteland;
private: System::Windows::Forms::Button^  button6;











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
			this->tabControl1 = (gcnew System::Windows::Forms::TabControl());
			this->tabPage1 = (gcnew System::Windows::Forms::TabPage());
			this->button6 = (gcnew System::Windows::Forms::Button());
			this->label5 = (gcnew System::Windows::Forms::Label());
			this->label4 = (gcnew System::Windows::Forms::Label());
			this->nWy = (gcnew System::Windows::Forms::NumericUpDown());
			this->nWx = (gcnew System::Windows::Forms::NumericUpDown());
			this->label3 = (gcnew System::Windows::Forms::Label());
			this->label2 = (gcnew System::Windows::Forms::Label());
			this->bAddCity = (gcnew System::Windows::Forms::Button());
			this->lbProtoLoc = (gcnew System::Windows::Forms::ListBox());
			this->bUpload = (gcnew System::Windows::Forms::Button());
			this->bRight = (gcnew System::Windows::Forms::Button());
			this->lbLoc = (gcnew System::Windows::Forms::ListBox());
			this->bLeft = (gcnew System::Windows::Forms::Button());
			this->label1 = (gcnew System::Windows::Forms::Label());
			this->bDown = (gcnew System::Windows::Forms::Button());
			this->pMap = (gcnew System::Windows::Forms::Panel());
			this->bUp = (gcnew System::Windows::Forms::Button());
			this->tabPage2 = (gcnew System::Windows::Forms::TabPage());
			this->nPPid = (gcnew System::Windows::Forms::NumericUpDown());
			this->label11 = (gcnew System::Windows::Forms::Label());
			this->cmbPRadius = (gcnew System::Windows::Forms::ComboBox());
			this->tPInfo = (gcnew System::Windows::Forms::RichTextBox());
			this->tPName = (gcnew System::Windows::Forms::TextBox());
			this->button7 = (gcnew System::Windows::Forms::Button());
			this->button5 = (gcnew System::Windows::Forms::Button());
			this->button4 = (gcnew System::Windows::Forms::Button());
			this->button3 = (gcnew System::Windows::Forms::Button());
			this->button2 = (gcnew System::Windows::Forms::Button());
			this->button1 = (gcnew System::Windows::Forms::Button());
			this->label10 = (gcnew System::Windows::Forms::Label());
			this->lbPLocMap = (gcnew System::Windows::Forms::ListBox());
			this->label9 = (gcnew System::Windows::Forms::Label());
			this->label8 = (gcnew System::Windows::Forms::Label());
			this->label7 = (gcnew System::Windows::Forms::Label());
			this->label6 = (gcnew System::Windows::Forms::Label());
			this->lbPProtoMaps = (gcnew System::Windows::Forms::ListBox());
			this->tabsPLoc = (gcnew System::Windows::Forms::TabControl());
			this->tCity = (gcnew System::Windows::Forms::TabPage());
			this->nPCStartMaps = (gcnew System::Windows::Forms::NumericUpDown());
			this->label15 = (gcnew System::Windows::Forms::Label());
			this->nPCLifeLevel = (gcnew System::Windows::Forms::NumericUpDown());
			this->nPCPopulation = (gcnew System::Windows::Forms::NumericUpDown());
			this->nPCSteal = (gcnew System::Windows::Forms::NumericUpDown());
			this->label14 = (gcnew System::Windows::Forms::Label());
			this->label13 = (gcnew System::Windows::Forms::Label());
			this->label12 = (gcnew System::Windows::Forms::Label());
			this->tEncaunter = (gcnew System::Windows::Forms::TabPage());
			this->groupBox1 = (gcnew System::Windows::Forms::GroupBox());
			this->cbPEOcean = (gcnew System::Windows::Forms::CheckBox());
			this->cbPEMountain = (gcnew System::Windows::Forms::CheckBox());
			this->cbPEForest = (gcnew System::Windows::Forms::CheckBox());
			this->cbPEWasteland = (gcnew System::Windows::Forms::CheckBox());
			this->nPEMaxGroups = (gcnew System::Windows::Forms::NumericUpDown());
			this->label17 = (gcnew System::Windows::Forms::Label());
			this->tSpecial = (gcnew System::Windows::Forms::TabPage());
			this->tQuest = (gcnew System::Windows::Forms::TabPage());
			this->cmbLog = (gcnew System::Windows::Forms::ComboBox());
			this->tabControl1->SuspendLayout();
			this->tabPage1->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWy))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWx))->BeginInit();
			this->tabPage2->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPPid))->BeginInit();
			this->tabsPLoc->SuspendLayout();
			this->tCity->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPCStartMaps))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPCLifeLevel))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPCPopulation))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPCSteal))->BeginInit();
			this->tEncaunter->SuspendLayout();
			this->groupBox1->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPEMaxGroups))->BeginInit();
			this->SuspendLayout();
			// 
			// tabControl1
			// 
			this->tabControl1->Controls->Add(this->tabPage1);
			this->tabControl1->Controls->Add(this->tabPage2);
			this->tabControl1->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tabControl1->Location = System::Drawing::Point(0, 0);
			this->tabControl1->Name = L"tabControl1";
			this->tabControl1->SelectedIndex = 0;
			this->tabControl1->Size = System::Drawing::Size(792, 571);
			this->tabControl1->TabIndex = 0;
			// 
			// tabPage1
			// 
			this->tabPage1->Controls->Add(this->button6);
			this->tabPage1->Controls->Add(this->label5);
			this->tabPage1->Controls->Add(this->label4);
			this->tabPage1->Controls->Add(this->nWy);
			this->tabPage1->Controls->Add(this->nWx);
			this->tabPage1->Controls->Add(this->label3);
			this->tabPage1->Controls->Add(this->label2);
			this->tabPage1->Controls->Add(this->bAddCity);
			this->tabPage1->Controls->Add(this->lbProtoLoc);
			this->tabPage1->Controls->Add(this->bUpload);
			this->tabPage1->Controls->Add(this->bRight);
			this->tabPage1->Controls->Add(this->lbLoc);
			this->tabPage1->Controls->Add(this->bLeft);
			this->tabPage1->Controls->Add(this->label1);
			this->tabPage1->Controls->Add(this->bDown);
			this->tabPage1->Controls->Add(this->pMap);
			this->tabPage1->Controls->Add(this->bUp);
			this->tabPage1->Location = System::Drawing::Point(4, 22);
			this->tabPage1->Name = L"tabPage1";
			this->tabPage1->Padding = System::Windows::Forms::Padding(3);
			this->tabPage1->Size = System::Drawing::Size(784, 545);
			this->tabPage1->TabIndex = 0;
			this->tabPage1->Text = L"Менеджер мира";
			this->tabPage1->UseVisualStyleBackColor = true;
			// 
			// button6
			// 
			this->button6->Location = System::Drawing::Point(658, 407);
			this->button6->Name = L"button6";
			this->button6->Size = System::Drawing::Size(120, 23);
			this->button6->TabIndex = 15;
			this->button6->Text = L"Обновить";
			this->button6->UseVisualStyleBackColor = true;
			this->button6->Click += gcnew System::EventHandler(this, &Form1::button6_Click);
			// 
			// label5
			// 
			this->label5->AutoSize = true;
			this->label5->Location = System::Drawing::Point(549, 61);
			this->label5->Name = L"label5";
			this->label5->Size = System::Drawing::Size(79, 13);
			this->label5->TabIndex = 14;
			this->label5->Text = L"Глобальный Y";
			// 
			// label4
			// 
			this->label4->AutoSize = true;
			this->label4->Location = System::Drawing::Point(549, 22);
			this->label4->Name = L"label4";
			this->label4->Size = System::Drawing::Size(79, 13);
			this->label4->TabIndex = 13;
			this->label4->Text = L"Глобальный X";
			// 
			// nWy
			// 
			this->nWy->Location = System::Drawing::Point(552, 77);
			this->nWy->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {2100, 0, 0, 0});
			this->nWy->Name = L"nWy";
			this->nWy->Size = System::Drawing::Size(48, 20);
			this->nWy->TabIndex = 12;
			// 
			// nWx
			// 
			this->nWx->Location = System::Drawing::Point(552, 38);
			this->nWx->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {2850, 0, 0, 0});
			this->nWx->Name = L"nWx";
			this->nWx->Size = System::Drawing::Size(48, 20);
			this->nWx->TabIndex = 11;
			// 
			// label3
			// 
			this->label3->AutoSize = true;
			this->label3->Location = System::Drawing::Point(655, 6);
			this->label3->Name = L"label3";
			this->label3->Size = System::Drawing::Size(110, 13);
			this->label3->TabIndex = 10;
			this->label3->Text = L"Прототипы городов:";
			// 
			// label2
			// 
			this->label2->AutoSize = true;
			this->label2->Location = System::Drawing::Point(410, 6);
			this->label2->Name = L"label2";
			this->label2->Size = System::Drawing::Size(93, 13);
			this->label2->TabIndex = 9;
			this->label2->Text = L"Текущие города:";
			// 
			// bAddCity
			// 
			this->bAddCity->Location = System::Drawing::Point(536, 370);
			this->bAddCity->Name = L"bAddCity";
			this->bAddCity->Size = System::Drawing::Size(116, 33);
			this->bAddCity->TabIndex = 8;
			this->bAddCity->Text = L"<<< Добавить <<<";
			this->bAddCity->UseVisualStyleBackColor = true;
			this->bAddCity->Click += gcnew System::EventHandler(this, &Form1::bAddCity_Click);
			// 
			// lbProtoLoc
			// 
			this->lbProtoLoc->FormattingEnabled = true;
			this->lbProtoLoc->Location = System::Drawing::Point(658, 22);
			this->lbProtoLoc->Name = L"lbProtoLoc";
			this->lbProtoLoc->Size = System::Drawing::Size(120, 381);
			this->lbProtoLoc->TabIndex = 7;
			// 
			// bUpload
			// 
			this->bUpload->Location = System::Drawing::Point(6, 501);
			this->bUpload->Name = L"bUpload";
			this->bUpload->Size = System::Drawing::Size(112, 21);
			this->bUpload->TabIndex = 6;
			this->bUpload->Text = L"Загрузить мир";
			this->bUpload->UseVisualStyleBackColor = true;
			this->bUpload->Click += gcnew System::EventHandler(this, &Form1::bUpload_Click);
			// 
			// bRight
			// 
			this->bRight->Location = System::Drawing::Point(290, 438);
			this->bRight->Name = L"bRight";
			this->bRight->Size = System::Drawing::Size(53, 56);
			this->bRight->TabIndex = 5;
			this->bRight->Text = L"> > >";
			this->bRight->UseVisualStyleBackColor = true;
			// 
			// lbLoc
			// 
			this->lbLoc->FormattingEnabled = true;
			this->lbLoc->Location = System::Drawing::Point(410, 22);
			this->lbLoc->Name = L"lbLoc";
			this->lbLoc->Size = System::Drawing::Size(120, 381);
			this->lbLoc->TabIndex = 2;
			// 
			// bLeft
			// 
			this->bLeft->Location = System::Drawing::Point(172, 438);
			this->bLeft->Name = L"bLeft";
			this->bLeft->Size = System::Drawing::Size(53, 56);
			this->bLeft->TabIndex = 3;
			this->bLeft->Text = L"< < <";
			this->bLeft->UseVisualStyleBackColor = true;
			// 
			// label1
			// 
			this->label1->AutoSize = true;
			this->label1->Location = System::Drawing::Point(6, 407);
			this->label1->Name = L"label1";
			this->label1->Size = System::Drawing::Size(115, 91);
			this->label1->TabIndex = 1;
			this->label1->Text = L"1 пиксель = 1 км\r\nШирина = 2100 км\r\nВысота = 2850 км\r\n\r\nЗона = 50 км X 50 км\r\nШир" 
				L"ина = 42 зоны\r\nВысота = 57 зон";
			// 
			// bDown
			// 
			this->bDown->Location = System::Drawing::Point(231, 466);
			this->bDown->Name = L"bDown";
			this->bDown->Size = System::Drawing::Size(53, 56);
			this->bDown->TabIndex = 4;
			this->bDown->Text = L"\\/ \\/ \\/";
			this->bDown->UseVisualStyleBackColor = true;
			// 
			// pMap
			// 
			this->pMap->BorderStyle = System::Windows::Forms::BorderStyle::FixedSingle;
			this->pMap->Location = System::Drawing::Point(4, 6);
			this->pMap->Name = L"pMap";
			this->pMap->Size = System::Drawing::Size(400, 400);
			this->pMap->TabIndex = 0;
			// 
			// bUp
			// 
			this->bUp->Location = System::Drawing::Point(231, 412);
			this->bUp->Name = L"bUp";
			this->bUp->Size = System::Drawing::Size(53, 56);
			this->bUp->TabIndex = 2;
			this->bUp->Text = L"/\\ /\\ /\\";
			this->bUp->UseVisualStyleBackColor = true;
			// 
			// tabPage2
			// 
			this->tabPage2->Controls->Add(this->nPPid);
			this->tabPage2->Controls->Add(this->label11);
			this->tabPage2->Controls->Add(this->cmbPRadius);
			this->tabPage2->Controls->Add(this->tPInfo);
			this->tabPage2->Controls->Add(this->tPName);
			this->tabPage2->Controls->Add(this->button7);
			this->tabPage2->Controls->Add(this->button5);
			this->tabPage2->Controls->Add(this->button4);
			this->tabPage2->Controls->Add(this->button3);
			this->tabPage2->Controls->Add(this->button2);
			this->tabPage2->Controls->Add(this->button1);
			this->tabPage2->Controls->Add(this->label10);
			this->tabPage2->Controls->Add(this->lbPLocMap);
			this->tabPage2->Controls->Add(this->label9);
			this->tabPage2->Controls->Add(this->label8);
			this->tabPage2->Controls->Add(this->label7);
			this->tabPage2->Controls->Add(this->label6);
			this->tabPage2->Controls->Add(this->lbPProtoMaps);
			this->tabPage2->Controls->Add(this->tabsPLoc);
			this->tabPage2->Location = System::Drawing::Point(4, 22);
			this->tabPage2->Name = L"tabPage2";
			this->tabPage2->Padding = System::Windows::Forms::Padding(3);
			this->tabPage2->Size = System::Drawing::Size(784, 545);
			this->tabPage2->TabIndex = 1;
			this->tabPage2->Text = L"Прототипы локаций";
			this->tabPage2->UseVisualStyleBackColor = true;
			// 
			// nPPid
			// 
			this->nPPid->Location = System::Drawing::Point(270, 490);
			this->nPPid->Name = L"nPPid";
			this->nPPid->Size = System::Drawing::Size(56, 20);
			this->nPPid->TabIndex = 19;
			// 
			// label11
			// 
			this->label11->AutoSize = true;
			this->label11->Location = System::Drawing::Point(267, 475);
			this->label11->Name = L"label11";
			this->label11->Size = System::Drawing::Size(99, 13);
			this->label11->TabIndex = 18;
			this->label11->Text = L"Номер прототипа:";
			// 
			// cmbPRadius
			// 
			this->cmbPRadius->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
			this->cmbPRadius->FormattingEnabled = true;
			this->cmbPRadius->Items->AddRange(gcnew cli::array< System::Object^  >(3) {L"24 - Большое поселение", L"12 - Среднее поселение", 
				L"6 - Небольшое поселение"});
			this->cmbPRadius->Location = System::Drawing::Point(56, 144);
			this->cmbPRadius->Name = L"cmbPRadius";
			this->cmbPRadius->Size = System::Drawing::Size(167, 21);
			this->cmbPRadius->TabIndex = 17;
			// 
			// tPInfo
			// 
			this->tPInfo->Location = System::Drawing::Point(6, 45);
			this->tPInfo->Name = L"tPInfo";
			this->tPInfo->Size = System::Drawing::Size(313, 96);
			this->tPInfo->TabIndex = 16;
			this->tPInfo->Text = L"";
			// 
			// tPName
			// 
			this->tPName->Location = System::Drawing::Point(41, 6);
			this->tPName->Name = L"tPName";
			this->tPName->Size = System::Drawing::Size(278, 20);
			this->tPName->TabIndex = 15;
			// 
			// button7
			// 
			this->button7->Location = System::Drawing::Point(135, 475);
			this->button7->Name = L"button7";
			this->button7->Size = System::Drawing::Size(126, 47);
			this->button7->TabIndex = 14;
			this->button7->Text = L"Сохранить прототип локации";
			this->button7->UseVisualStyleBackColor = true;
			this->button7->Click += gcnew System::EventHandler(this, &Form1::button7_Click);
			// 
			// button5
			// 
			this->button5->Location = System::Drawing::Point(3, 475);
			this->button5->Name = L"button5";
			this->button5->Size = System::Drawing::Size(126, 47);
			this->button5->TabIndex = 12;
			this->button5->Text = L"Загрузить прототип локации";
			this->button5->UseVisualStyleBackColor = true;
			this->button5->Click += gcnew System::EventHandler(this, &Form1::button5_Click);
			// 
			// button4
			// 
			this->button4->Location = System::Drawing::Point(457, 129);
			this->button4->Name = L"button4";
			this->button4->Size = System::Drawing::Size(72, 41);
			this->button4->TabIndex = 11;
			this->button4->Text = L"Вниз\r\n\\/";
			this->button4->UseVisualStyleBackColor = true;
			this->button4->Click += gcnew System::EventHandler(this, &Form1::button4_Click);
			// 
			// button3
			// 
			this->button3->Location = System::Drawing::Point(457, 35);
			this->button3->Name = L"button3";
			this->button3->Size = System::Drawing::Size(72, 41);
			this->button3->TabIndex = 10;
			this->button3->Text = L"/\\\r\nВверх";
			this->button3->UseVisualStyleBackColor = true;
			this->button3->Click += gcnew System::EventHandler(this, &Form1::button3_Click);
			// 
			// button2
			// 
			this->button2->Location = System::Drawing::Point(457, 82);
			this->button2->Name = L"button2";
			this->button2->Size = System::Drawing::Size(72, 41);
			this->button2->TabIndex = 9;
			this->button2->Text = L"Удалить";
			this->button2->UseVisualStyleBackColor = true;
			this->button2->Click += gcnew System::EventHandler(this, &Form1::button2_Click);
			// 
			// button1
			// 
			this->button1->Location = System::Drawing::Point(535, 200);
			this->button1->Name = L"button1";
			this->button1->Size = System::Drawing::Size(120, 43);
			this->button1->TabIndex = 8;
			this->button1->Text = L"/\\/\\/\\ Добавить <<<";
			this->button1->UseVisualStyleBackColor = true;
			this->button1->Click += gcnew System::EventHandler(this, &Form1::button1_Click);
			// 
			// label10
			// 
			this->label10->AutoSize = true;
			this->label10->Location = System::Drawing::Point(532, 3);
			this->label10->Name = L"label10";
			this->label10->Size = System::Drawing::Size(118, 13);
			this->label10->TabIndex = 7;
			this->label10->Text = L"Список карт локации:";
			// 
			// lbPLocMap
			// 
			this->lbPLocMap->BorderStyle = System::Windows::Forms::BorderStyle::FixedSingle;
			this->lbPLocMap->FormattingEnabled = true;
			this->lbPLocMap->Location = System::Drawing::Point(535, 19);
			this->lbPLocMap->Name = L"lbPLocMap";
			this->lbPLocMap->Size = System::Drawing::Size(120, 171);
			this->lbPLocMap->TabIndex = 6;
			// 
			// label9
			// 
			this->label9->AutoSize = true;
			this->label9->Location = System::Drawing::Point(4, 144);
			this->label9->Name = L"label9";
			this->label9->Size = System::Drawing::Size(46, 13);
			this->label9->TabIndex = 5;
			this->label9->Text = L"Радиус:";
			// 
			// label8
			// 
			this->label8->AutoSize = true;
			this->label8->Location = System::Drawing::Point(4, 29);
			this->label8->Name = L"label8";
			this->label8->Size = System::Drawing::Size(38, 13);
			this->label8->TabIndex = 4;
			this->label8->Text = L"Инфо:";
			// 
			// label7
			// 
			this->label7->AutoSize = true;
			this->label7->Location = System::Drawing::Point(3, 3);
			this->label7->Name = L"label7";
			this->label7->Size = System::Drawing::Size(32, 13);
			this->label7->TabIndex = 3;
			this->label7->Text = L"Имя:";
			// 
			// label6
			// 
			this->label6->AutoSize = true;
			this->label6->Location = System::Drawing::Point(658, 3);
			this->label6->Name = L"label6";
			this->label6->Size = System::Drawing::Size(100, 13);
			this->label6->TabIndex = 2;
			this->label6->Text = L"Весь список карт:";
			// 
			// lbPProtoMaps
			// 
			this->lbPProtoMaps->BorderStyle = System::Windows::Forms::BorderStyle::FixedSingle;
			this->lbPProtoMaps->FormattingEnabled = true;
			this->lbPProtoMaps->Location = System::Drawing::Point(661, 19);
			this->lbPProtoMaps->Name = L"lbPProtoMaps";
			this->lbPProtoMaps->Size = System::Drawing::Size(120, 509);
			this->lbPProtoMaps->TabIndex = 1;
			// 
			// tabsPLoc
			// 
			this->tabsPLoc->Controls->Add(this->tCity);
			this->tabsPLoc->Controls->Add(this->tEncaunter);
			this->tabsPLoc->Controls->Add(this->tSpecial);
			this->tabsPLoc->Controls->Add(this->tQuest);
			this->tabsPLoc->Location = System::Drawing::Point(3, 227);
			this->tabsPLoc->Name = L"tabsPLoc";
			this->tabsPLoc->SelectedIndex = 0;
			this->tabsPLoc->Size = System::Drawing::Size(652, 242);
			this->tabsPLoc->TabIndex = 0;
			// 
			// tCity
			// 
			this->tCity->Controls->Add(this->nPCStartMaps);
			this->tCity->Controls->Add(this->label15);
			this->tCity->Controls->Add(this->nPCLifeLevel);
			this->tCity->Controls->Add(this->nPCPopulation);
			this->tCity->Controls->Add(this->nPCSteal);
			this->tCity->Controls->Add(this->label14);
			this->tCity->Controls->Add(this->label13);
			this->tCity->Controls->Add(this->label12);
			this->tCity->Location = System::Drawing::Point(4, 22);
			this->tCity->Name = L"tCity";
			this->tCity->Padding = System::Windows::Forms::Padding(3);
			this->tCity->Size = System::Drawing::Size(644, 216);
			this->tCity->TabIndex = 0;
			this->tCity->Text = L"Город";
			this->tCity->UseVisualStyleBackColor = true;
			// 
			// nPCStartMaps
			// 
			this->nPCStartMaps->Location = System::Drawing::Point(9, 58);
			this->nPCStartMaps->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			this->nPCStartMaps->Name = L"nPCStartMaps";
			this->nPCStartMaps->Size = System::Drawing::Size(83, 20);
			this->nPCStartMaps->TabIndex = 7;
			this->nPCStartMaps->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			this->nPCStartMaps->ValueChanged += gcnew System::EventHandler(this, &Form1::nPStartMaps_ValueChanged);
			// 
			// label15
			// 
			this->label15->AutoSize = true;
			this->label15->Location = System::Drawing::Point(3, 42);
			this->label15->Name = L"label15";
			this->label15->Size = System::Drawing::Size(90, 13);
			this->label15->TabIndex = 6;
			this->label15->Text = L"Стартовых карт:";
			// 
			// nPCLifeLevel
			// 
			this->nPCLifeLevel->Location = System::Drawing::Point(9, 136);
			this->nPCLifeLevel->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {100000000, 0, 0, 0});
			this->nPCLifeLevel->Name = L"nPCLifeLevel";
			this->nPCLifeLevel->Size = System::Drawing::Size(83, 20);
			this->nPCLifeLevel->TabIndex = 5;
			// 
			// nPCPopulation
			// 
			this->nPCPopulation->Location = System::Drawing::Point(9, 97);
			this->nPCPopulation->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {100000000, 0, 0, 0});
			this->nPCPopulation->Name = L"nPCPopulation";
			this->nPCPopulation->Size = System::Drawing::Size(83, 20);
			this->nPCPopulation->TabIndex = 4;
			// 
			// nPCSteal
			// 
			this->nPCSteal->Location = System::Drawing::Point(9, 19);
			this->nPCSteal->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {300, 0, 0, 0});
			this->nPCSteal->Name = L"nPCSteal";
			this->nPCSteal->Size = System::Drawing::Size(83, 20);
			this->nPCSteal->TabIndex = 3;
			// 
			// label14
			// 
			this->label14->AutoSize = true;
			this->label14->Location = System::Drawing::Point(3, 120);
			this->label14->Name = L"label14";
			this->label14->Size = System::Drawing::Size(89, 13);
			this->label14->TabIndex = 2;
			this->label14->Text = L"Уровень жизни:";
			// 
			// label13
			// 
			this->label13->AutoSize = true;
			this->label13->Location = System::Drawing::Point(6, 81);
			this->label13->Name = L"label13";
			this->label13->Size = System::Drawing::Size(65, 13);
			this->label13->TabIndex = 1;
			this->label13->Text = L"Популяция:";
			// 
			// label12
			// 
			this->label12->AutoSize = true;
			this->label12->Location = System::Drawing::Point(6, 3);
			this->label12->Name = L"label12";
			this->label12->Size = System::Drawing::Size(71, 13);
			this->label12->TabIndex = 0;
			this->label12->Text = L"Скрытность:";
			// 
			// tEncaunter
			// 
			this->tEncaunter->Controls->Add(this->groupBox1);
			this->tEncaunter->Controls->Add(this->nPEMaxGroups);
			this->tEncaunter->Controls->Add(this->label17);
			this->tEncaunter->Location = System::Drawing::Point(4, 22);
			this->tEncaunter->Name = L"tEncaunter";
			this->tEncaunter->Padding = System::Windows::Forms::Padding(3);
			this->tEncaunter->Size = System::Drawing::Size(644, 216);
			this->tEncaunter->TabIndex = 1;
			this->tEncaunter->Text = L"Энкаунтер";
			this->tEncaunter->UseVisualStyleBackColor = true;
			// 
			// groupBox1
			// 
			this->groupBox1->Controls->Add(this->cbPEOcean);
			this->groupBox1->Controls->Add(this->cbPEMountain);
			this->groupBox1->Controls->Add(this->cbPEForest);
			this->groupBox1->Controls->Add(this->cbPEWasteland);
			this->groupBox1->Location = System::Drawing::Point(6, 6);
			this->groupBox1->Name = L"groupBox1";
			this->groupBox1->Size = System::Drawing::Size(130, 84);
			this->groupBox1->TabIndex = 4;
			this->groupBox1->TabStop = false;
			this->groupBox1->Text = L"Тип местности";
			// 
			// cbPEOcean
			// 
			this->cbPEOcean->AutoSize = true;
			this->cbPEOcean->Location = System::Drawing::Point(6, 65);
			this->cbPEOcean->Name = L"cbPEOcean";
			this->cbPEOcean->Size = System::Drawing::Size(60, 17);
			this->cbPEOcean->TabIndex = 3;
			this->cbPEOcean->Text = L"Океан";
			this->cbPEOcean->UseVisualStyleBackColor = true;
			// 
			// cbPEMountain
			// 
			this->cbPEMountain->AutoSize = true;
			this->cbPEMountain->Location = System::Drawing::Point(6, 34);
			this->cbPEMountain->Name = L"cbPEMountain";
			this->cbPEMountain->Size = System::Drawing::Size(54, 17);
			this->cbPEMountain->TabIndex = 2;
			this->cbPEMountain->Text = L"Горы";
			this->cbPEMountain->UseVisualStyleBackColor = true;
			// 
			// cbPEForest
			// 
			this->cbPEForest->AutoSize = true;
			this->cbPEForest->Location = System::Drawing::Point(6, 50);
			this->cbPEForest->Name = L"cbPEForest";
			this->cbPEForest->Size = System::Drawing::Size(48, 17);
			this->cbPEForest->TabIndex = 1;
			this->cbPEForest->Text = L"Лес";
			this->cbPEForest->UseVisualStyleBackColor = true;
			// 
			// cbPEWasteland
			// 
			this->cbPEWasteland->AutoSize = true;
			this->cbPEWasteland->Location = System::Drawing::Point(6, 19);
			this->cbPEWasteland->Name = L"cbPEWasteland";
			this->cbPEWasteland->Size = System::Drawing::Size(72, 17);
			this->cbPEWasteland->TabIndex = 0;
			this->cbPEWasteland->Text = L"Пустошь";
			this->cbPEWasteland->UseVisualStyleBackColor = true;
			// 
			// nPEMaxGroups
			// 
			this->nPEMaxGroups->Location = System::Drawing::Point(21, 109);
			this->nPEMaxGroups->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {10, 0, 0, 0});
			this->nPEMaxGroups->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			this->nPEMaxGroups->Name = L"nPEMaxGroups";
			this->nPEMaxGroups->Size = System::Drawing::Size(45, 20);
			this->nPEMaxGroups->TabIndex = 3;
			this->nPEMaxGroups->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			// 
			// label17
			// 
			this->label17->AutoSize = true;
			this->label17->Location = System::Drawing::Point(3, 93);
			this->label17->Name = L"label17";
			this->label17->Size = System::Drawing::Size(131, 13);
			this->label17->TabIndex = 2;
			this->label17->Text = L"Макс. групп на локации:";
			// 
			// tSpecial
			// 
			this->tSpecial->Location = System::Drawing::Point(4, 22);
			this->tSpecial->Name = L"tSpecial";
			this->tSpecial->Size = System::Drawing::Size(644, 216);
			this->tSpecial->TabIndex = 2;
			this->tSpecial->Text = L"Специал";
			this->tSpecial->UseVisualStyleBackColor = true;
			// 
			// tQuest
			// 
			this->tQuest->Location = System::Drawing::Point(4, 22);
			this->tQuest->Name = L"tQuest";
			this->tQuest->Size = System::Drawing::Size(644, 216);
			this->tQuest->TabIndex = 3;
			this->tQuest->Text = L"Квест";
			this->tQuest->UseVisualStyleBackColor = true;
			// 
			// cmbLog
			// 
			this->cmbLog->Dock = System::Windows::Forms::DockStyle::Bottom;
			this->cmbLog->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
			this->cmbLog->FormattingEnabled = true;
			this->cmbLog->Location = System::Drawing::Point(0, 550);
			this->cmbLog->Name = L"cmbLog";
			this->cmbLog->Size = System::Drawing::Size(792, 21);
			this->cmbLog->TabIndex = 1;
			// 
			// Form1
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(792, 571);
			this->Controls->Add(this->cmbLog);
			this->Controls->Add(this->tabControl1);
			this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedSingle;
			this->Name = L"Form1";
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
			this->Text = L"Form1";
			this->Load += gcnew System::EventHandler(this, &Form1::Form1_Load);
			this->tabControl1->ResumeLayout(false);
			this->tabPage1->ResumeLayout(false);
			this->tabPage1->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWy))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nWx))->EndInit();
			this->tabPage2->ResumeLayout(false);
			this->tabPage2->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPPid))->EndInit();
			this->tabsPLoc->ResumeLayout(false);
			this->tCity->ResumeLayout(false);
			this->tCity->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPCStartMaps))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPCLifeLevel))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPCPopulation))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPCSteal))->EndInit();
			this->tEncaunter->ResumeLayout(false);
			this->tEncaunter->PerformLayout();
			this->groupBox1->ResumeLayout(false);
			this->groupBox1->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPEMaxGroups))->EndInit();
			this->ResumeLayout(false);

		}
#pragma endregion
	private: System::Void Form1_Load(System::Object^  sender, System::EventArgs^  e)
			{
				Log("Инициализация Файл-менеджера.");
				if(!fm.Init(".\\"))
				{
					Log("Ошибка инициализации файл-менеджера.");
					Close();
				}
				Log("Файл-менеджер инициализирован.");

				Log("Инициализация SQL-менеджера.");
				if(!sql.Init())
				{
					Log("Ошибка инициализации SQL-менеджера.");
					Close();
				}
				Log("SQL-менеджер инициализирован.");

				cmbPRadius->SelectedIndex=0;

				System::Windows::Forms::RichTextBox^ rtb;
				rtb = (gcnew System::Windows::Forms::RichTextBox());

				Log("Загрузка списка прототипов карт.");
				rtb->LoadFile(".//maps//proto_maps.lst",System::Windows::Forms::RichTextBoxStreamType::PlainText);
				lbPProtoMaps->Items->AddRange(rtb->Lines);
				lbPProtoMaps->SelectedIndex=0;
				Log("Список прототипов карт загружен.");
			}
private: System::Void nPStartMaps_ValueChanged(System::Object^  sender, System::EventArgs^  e) {
		 }
private: System::Void button7_Click(System::Object^  sender, System::EventArgs^  e)
		{
			Log("Сохранение прототипа локации.");

			if(nPPid->Value==0 || nPPid->Value>=MAX_PROTO_LOCATIONS)
			{
				Log("Неверный pid.");
				return;
			}

			WORD pid=(WORD)nPPid->Value;

			if(!lbPLocMap->Items->Count || lbPLocMap->Items->Count>MAX_MAPS_IN_LOCATION)
			{
				Log("Колличество карт в локации равно нулю или превышает лимит");
				return;
			}

			loc.Clear();
			if(!loc.Init(pid,&fm))
			{
				Log("Не удалось проинициализировать прототип.");
				return;
			}

			//TODO: имя, инфо

			switch(cmbPRadius->SelectedIndex)
			{
			case 0: loc.radius=24; break;
			case 1: loc.radius=12; break;
			case 2: loc.radius=6; break;
			default: loc.radius=24; break;
			}

			for(int i=0;i<lbPLocMap->Items->Count;++i)
				loc.proto_maps.push_back((WORD)System::Convert::ToUInt32(lbPLocMap->Items[i]));

			if(tabsPLoc->SelectedTab->Name=="tCity")
			{
				loc.type=LOCATION_TYPE_CITY;

				loc.CITY.steal=(WORD)nPCSteal->Value;
				loc.CITY.count_starts_maps=(WORD)nPCStartMaps->Value;
				loc.CITY.population=(UINT)nPCPopulation->Value;
				loc.CITY.life_level=(UINT)nPCLifeLevel->Value;
			}
			else if(tabsPLoc->SelectedTab->Name=="tEnacaunter")
			{
				loc.type=LOCATION_TYPE_ENCAUNTER;

				loc.ENCAUNTER.district=0;
				if(cbPEWasteland->Checked==true) loc.ENCAUNTER.district+=DISTRICT_WASTELAND;
				if(cbPEMountain->Checked==true) loc.ENCAUNTER.district+=DISTRICT_MOUNTAINS;
				if(cbPEForest->Checked==true) loc.ENCAUNTER.district+=DISTRICT_FOREST;
				if(cbPEOcean->Checked==true) loc.ENCAUNTER.district+=DISTRICT_OCEAN;
				if(!loc.ENCAUNTER.district) loc.ENCAUNTER.district=DISTRICT_WASTELAND;
				
				loc.ENCAUNTER.max_groups=(WORD)nPEMaxGroups->Value;
			}
			else if(tabsPLoc->SelectedTab->Name=="tSpecial")
			{
				loc.type=LOCATION_TYPE_SPECIAL;

				loc.SPECIAL.reserved=0;
			}
			else if(tabsPLoc->SelectedTab->Name=="tQuest")
			{
				loc.type=LOCATION_TYPE_QUEST;

				loc.QUEST.reserved=0;
			}
			else
			{
				Log("Неизвестный тип прототипа локации.");
				return;
			}

			if(!loc.Save(PT_SRV_MAPS))
			{
				Log("Не удалось сохранить прототип.");
				return;
			}

			Log("Прототип сохранен успешно.");
		}
private: System::Void button5_Click(System::Object^  sender, System::EventArgs^  e)
		{
			Log("Загрузка прототипа локации.");

			if(nPPid->Value==0 || nPPid->Value>=MAX_PROTO_LOCATIONS)
			{
				Log("Неверный pid.");
				return;
			}

			WORD pid=(WORD)nPPid->Value;

			loc.Clear();
			if(!loc.Init(pid,&fm))
			{
				Log("Не удалось проинициализировать прототип.");
				return;
			}

			if(!loc.Load(PT_SRV_MAPS))
			{
				Log("Не удалось загрузить прототип.");
				return;
			}

			//TODO: имя, инфо

			switch(loc.radius)
			{
			case 24: cmbPRadius->SelectedIndex=0; break;
			case 12: cmbPRadius->SelectedIndex=1; break;
			case 6: cmbPRadius->SelectedIndex=2; break;
			default: cmbPRadius->SelectedIndex=0; break;
			}

			for(int i=0;i<loc.proto_maps.size();++i)
				lbPLocMap->Items->Add(System::Convert::ToString(loc.proto_maps[i]));

			switch(loc.GetType())
			{
			case LOCATION_TYPE_CITY:

				nPCSteal->Value=(UINT)loc.CITY.steal;
				nPCStartMaps->Value=(UINT)loc.CITY.count_starts_maps;
				nPCPopulation->Value=(UINT)loc.CITY.population;
				nPCLifeLevel->Value=(UINT)loc.CITY.life_level;

				break;
			case LOCATION_TYPE_ENCAUNTER:

				cbPEWasteland->Checked=false;
				cbPEMountain->Checked=false;
				cbPEForest->Checked=false;
				cbPEOcean->Checked=false;
				if(loc.ENCAUNTER.district & DISTRICT_WASTELAND) cbPEWasteland->Checked=true;
				if(loc.ENCAUNTER.district & DISTRICT_MOUNTAINS) cbPEMountain->Checked=true;
				if(loc.ENCAUNTER.district & DISTRICT_FOREST) cbPEForest->Checked=true;
				if(loc.ENCAUNTER.district & DISTRICT_OCEAN) cbPEOcean->Checked=true;
				
				nPEMaxGroups->Value=(UINT)loc.ENCAUNTER.max_groups;

				break;
			case LOCATION_TYPE_SPECIAL:
				break;
			case LOCATION_TYPE_QUEST:
				break;
			default:
				Log("Не известный тип локации.");
				return;
			}

			Log("Прототип загружен успешно.");
		}
private: System::Void button1_Click(System::Object^  sender, System::EventArgs^  e)
		{
			if(lbPProtoMaps->SelectedIndex==-1)
			{
				Log("Выберите номер прототипа карты для добавления.");
				return;
			}

			for(int i=0;i<lbPLocMap->Items->Count;++i)
				if(lbPLocMap->Items[i]->ToString()==lbPProtoMaps->SelectedItem->ToString())
				{
					Log("Нельзя добовлять в одну локацию одинаковые карты.");
					return;
				}

			lbPLocMap->Items->Add(lbPProtoMaps->SelectedItem);
		}
private: System::Void button2_Click(System::Object^  sender, System::EventArgs^  e)
		{
			if(lbPLocMap->SelectedIndex==-1)
			{
				Log("Выберите карту для удаления.");
				return;
			}

			lbPLocMap->Items->Remove(lbPLocMap->SelectedItem);
		}
private: System::Void button3_Click(System::Object^  sender, System::EventArgs^  e)
		{
			if(lbPLocMap->SelectedIndex==-1)
			{
				Log("Выберите карту для перемещения.");
				return;
			}

			if(!lbPLocMap->SelectedIndex) return;

			//TODO: move up
		}
private: System::Void button4_Click(System::Object^  sender, System::EventArgs^  e)
		{
			if(lbPLocMap->SelectedIndex==-1)
			{
				Log("Выберите карту для перемещения.");
				return;
			}

			if(lbPLocMap->SelectedIndex==lbPLocMap->Items->Count) return;

			//TODO: move down
		}
private: System::Void button6_Click(System::Object^  sender, System::EventArgs^  e)
		{
			lbProtoLoc->Items->Clear();

			System::Windows::Forms::RichTextBox^ rtb;
			rtb = (gcnew System::Windows::Forms::RichTextBox());

			Log("Обновление списка прототипов доступных локаций.");
			rtb->LoadFile(".//maps//proto_locations.lst",System::Windows::Forms::RichTextBoxStreamType::PlainText);
			lbProtoLoc->Items->AddRange(rtb->Lines);
			lbProtoLoc->SelectedIndex=0;
			Log("Список загружен.");
		}
private: System::Void bUpload_Click(System::Object^  sender, System::EventArgs^  e)
		{
			Log("Загрузка созданных локаций.");

			lbLoc->Items->Clear();

			if(!sql.Query("SELECT * FROM `locations_city`;"))
			{
				Log("Ошибка при запросе БД.");
				return;
			}

			if(!sql.StoreResult())
			{
				Log("Ошибка фиксации выборки.");
				return;
			}
			
			DWORD res_count=0;
			if(!(res_count=sql.GetResultCount()))
			{
				Log("Не найдено ни одного результата.");
				return;
			}
			
			SQL_RESULT res=NULL;

			vector<DWORD> maps_to_load;
			
			for(int i=0;i<res_count;++i)
			{
				if(!(res=sql.GetNextResult()))
				{
					Log("Ошибка взятия следующего результата.");
					return;
				}
				
				DWORD loc_id=atoi(res[0]);
				WORD loc_pid=atoi(res[1]);
				WORD loc_wx=atoi(res[2]);
				WORD loc_wy=atoi(res[3]);	
				DWORD loc_maps[MAX_MAPS_IN_LOCATION];
				memcpy((void*)&loc_maps[0],(void*)res[4],MAX_MAPS_IN_LOCATION*sizeof(DWORD));

				lbLoc->Items->Add(System::Convert::ToString(loc_wx+","+loc_wy+" "+loc_id+"("+loc_pid+")"));
			}

		}
private: System::Void bAddCity_Click(System::Object^  sender, System::EventArgs^  e)
		{
			if(lbProtoLoc->SelectedIndex==-1)
			{
				Log("Выберите прототип для локации.");
				return;
			}

			if(nWx->Value==0 || nWy->Value==0)
			{
				Log("Укажите мировые координаты будущей локации.");
				return;
			}

			WORD loc_pid=System::Convert::ToUInt32(lbProtoLoc->SelectedItem);
			WORD loc_wx=(WORD)nWx->Value;
			WORD loc_wy=(WORD)nWy->Value;

			char qstr[2048];

			sprintf(&qstr[0],"INSERT INTO `locations_city_new` (`proto_id`,`world_x`,`world_y`) VALUES ('%d','%d','%d');",loc_pid,loc_wx,loc_wy);

			if(!sql.Query(&qstr[0]))
			{
				Log("Не удалось добавить локацию\n");
				return;
			}

			Log("Локация успешно добавлена - перезагрузите сервер");
		}
};
}

