#pragma once

#include <vcclr.h>
#include <locale.h>
#include <FileManager.h>
#include <CraftManager.h>
#include <ConstantsManager.h>
#include <IniParser.h>

namespace MrFixitEditor {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	CraftItem* CurCraft;

    const char* ToAnsi(String^ str)
    {
        static char mb[ 0x10000 ];
        pin_ptr<const wchar_t> wch = PtrToStringChars(str);
        if( WideCharToMultiByte( CP_UTF8, 0, wch, -1, mb, 0x10000 - 1, NULL, NULL ) == 0 )
            mb[ 0 ] = 0;
        return mb;
    }

    String^ ToClrString(const char* str)
    {
        static THREAD wchar_t wc[ 0x10000 ];
        if( MultiByteToWideChar( CP_UTF8, 0, str, -1, wc, 0x10000 - 1 ) == 0 )
            wc[ 0 ] = 0;
        return String(wc).ToString();
    }

    String^ ToClrString(string& str)
    {
        return ToClrString(str.c_str());
    }

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
			LogToFile(".\\MrFixit.log");
		}

		void Log(String^ str)
		{
			lErr->Text=str;
			WriteLog("%s\n",ToAnsi(str));
		}

		void UpdateCraftsList()
		{
			lbCraft->Items->Clear();

			CraftItemMap& crafts=MrFixit.GetAllCrafts();
			for(CraftItemMap::iterator it=crafts.begin(),end=crafts.end();it!=end;++it)
			{
				CraftItem* craft=(*it).second;
				String^ str=craft->Num+" - "+ToClrString(craft->Name.c_str())+" - "+ToClrString(craft->Info.c_str());
				lbCraft->Items->Add(str);
			}
		}

		void UpdateParamsLists()
		{
			cbParamNum->Items->Clear();
			StrVec param_names=ConstantsManager::GetCollection(CONSTANTS_PARAM);
			for(StrVec::iterator it=param_names.begin(),end=param_names.end();it!=end;++it)
			{
				String^ str=ToClrString((*it).c_str());
				cbParamNum->Items->Add(str);
			}

			cbItemPid->Items->Clear();
			StrVec item_names=ConstantsManager::GetCollection(CONSTANTS_ITEM);
			for(StrVec::iterator it=item_names.begin(),end=item_names.end();it!=end;++it)
			{
				String^ str=ToClrString((*it).c_str());
				cbItemPid->Items->Add(str);
			}
		}

		//void ParseCraft(CraftItem* craft);

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
	private: System::Windows::Forms::ListBox^  lbParamShow;
	private: System::Windows::Forms::Label^  label1;
	private: System::Windows::Forms::Label^  label2;
	private: System::Windows::Forms::Label^  label3;
	private: System::Windows::Forms::Label^  label4;
	private: System::Windows::Forms::Label^  label5;
	private: System::Windows::Forms::ListBox^  lbParamCraft;
	private: System::Windows::Forms::ListBox^  lbNeedItems;
	private: System::Windows::Forms::ListBox^  lbNeedTools;
	private: System::Windows::Forms::ListBox^  lbOutItems;
	private: System::Windows::Forms::ComboBox^  cbParamNum;
	private: System::Windows::Forms::NumericUpDown^  numParamCount;
	private: System::Windows::Forms::ComboBox^  cbItemPid;
	private: System::Windows::Forms::NumericUpDown^  numItemCount;
	private: System::Windows::Forms::Label^  label6;
	private: System::Windows::Forms::Label^  label7;
	private: System::Windows::Forms::Label^  label9;
	private: System::Windows::Forms::Label^  label10;
	private: System::Windows::Forms::Label^  label13;
	private: System::Windows::Forms::TextBox^  txtCraftName;
	private: System::Windows::Forms::NumericUpDown^  numCraftNum;
	private: System::Windows::Forms::TabControl^  tabControl1;
	private: System::Windows::Forms::TabPage^  tpCraft;
	private: System::Windows::Forms::TabPage^  tpMain;
	private: System::Windows::Forms::Button^  btnAddParamCraft;
	private: System::Windows::Forms::Button^  btnAddParamShow;
	private: System::Windows::Forms::Button^  btnSaveCraft;
	private: System::Windows::Forms::Button^  btnSave;
	private: System::Windows::Forms::Button^  btnLoad;
	private: System::Windows::Forms::ListBox^  lbCraft;
	private: System::Windows::Forms::Label^  lErr;
	private: System::Windows::Forms::Label^  label14;
	private: System::Windows::Forms::RichTextBox^  txtCraftInfo;
	private: System::Windows::Forms::Button^  btnAddNeedTool;
	private: System::Windows::Forms::Button^  btnAddNeedItem;
	private: System::Windows::Forms::Button^  btnAddOutItem;
	private: System::Windows::Forms::Button^  btnLoad2;
	private: System::Windows::Forms::CheckBox^  cbOr;
	private: System::Windows::Forms::StatusStrip^  statusStrip1;
	private: System::Windows::Forms::Button^  btnClear;
	private: System::Windows::Forms::Label^  label11;
	private: System::Windows::Forms::Label^  label8;
	private: System::Windows::Forms::TextBox^  txtScript;
	private: System::Windows::Forms::NumericUpDown^  numExperience;
	private: System::Windows::Forms::Label^  label16;

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
			this->lbParamShow = (gcnew System::Windows::Forms::ListBox());
			this->label1 = (gcnew System::Windows::Forms::Label());
			this->label2 = (gcnew System::Windows::Forms::Label());
			this->label3 = (gcnew System::Windows::Forms::Label());
			this->label4 = (gcnew System::Windows::Forms::Label());
			this->label5 = (gcnew System::Windows::Forms::Label());
			this->lbParamCraft = (gcnew System::Windows::Forms::ListBox());
			this->lbNeedItems = (gcnew System::Windows::Forms::ListBox());
			this->lbNeedTools = (gcnew System::Windows::Forms::ListBox());
			this->lbOutItems = (gcnew System::Windows::Forms::ListBox());
			this->cbParamNum = (gcnew System::Windows::Forms::ComboBox());
			this->numParamCount = (gcnew System::Windows::Forms::NumericUpDown());
			this->cbItemPid = (gcnew System::Windows::Forms::ComboBox());
			this->numItemCount = (gcnew System::Windows::Forms::NumericUpDown());
			this->label6 = (gcnew System::Windows::Forms::Label());
			this->label7 = (gcnew System::Windows::Forms::Label());
			this->label9 = (gcnew System::Windows::Forms::Label());
			this->label10 = (gcnew System::Windows::Forms::Label());
			this->label13 = (gcnew System::Windows::Forms::Label());
			this->txtCraftName = (gcnew System::Windows::Forms::TextBox());
			this->numCraftNum = (gcnew System::Windows::Forms::NumericUpDown());
			this->tabControl1 = (gcnew System::Windows::Forms::TabControl());
			this->tpMain = (gcnew System::Windows::Forms::TabPage());
			this->btnLoad2 = (gcnew System::Windows::Forms::Button());
			this->btnSave = (gcnew System::Windows::Forms::Button());
			this->btnLoad = (gcnew System::Windows::Forms::Button());
			this->lbCraft = (gcnew System::Windows::Forms::ListBox());
			this->tpCraft = (gcnew System::Windows::Forms::TabPage());
			this->numExperience = (gcnew System::Windows::Forms::NumericUpDown());
			this->label16 = (gcnew System::Windows::Forms::Label());
			this->label11 = (gcnew System::Windows::Forms::Label());
			this->label8 = (gcnew System::Windows::Forms::Label());
			this->txtScript = (gcnew System::Windows::Forms::TextBox());
			this->btnClear = (gcnew System::Windows::Forms::Button());
			this->cbOr = (gcnew System::Windows::Forms::CheckBox());
			this->btnAddOutItem = (gcnew System::Windows::Forms::Button());
			this->btnAddNeedTool = (gcnew System::Windows::Forms::Button());
			this->btnAddNeedItem = (gcnew System::Windows::Forms::Button());
			this->label14 = (gcnew System::Windows::Forms::Label());
			this->txtCraftInfo = (gcnew System::Windows::Forms::RichTextBox());
			this->btnAddParamCraft = (gcnew System::Windows::Forms::Button());
			this->btnAddParamShow = (gcnew System::Windows::Forms::Button());
			this->btnSaveCraft = (gcnew System::Windows::Forms::Button());
			this->lErr = (gcnew System::Windows::Forms::Label());
			this->statusStrip1 = (gcnew System::Windows::Forms::StatusStrip());
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numParamCount))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numItemCount))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numCraftNum))->BeginInit();
			this->tabControl1->SuspendLayout();
			this->tpMain->SuspendLayout();
			this->tpCraft->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numExperience))->BeginInit();
			this->SuspendLayout();
			// 
			// lbParamShow
			// 
			this->lbParamShow->FormattingEnabled = true;
			this->lbParamShow->HorizontalScrollbar = true;
			this->lbParamShow->Location = System::Drawing::Point(11, 32);
			this->lbParamShow->Name = L"lbParamShow";
			this->lbParamShow->Size = System::Drawing::Size(167, 95);
			this->lbParamShow->TabIndex = 0;
			this->lbParamShow->MouseClick += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseClick);
			this->lbParamShow->MouseDoubleClick += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseDoubleClick);
			this->lbParamShow->MouseDown += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseDown);
			// 
			// label1
			// 
			this->label1->AutoSize = true;
			this->label1->Location = System::Drawing::Point(11, 10);
			this->label1->Name = L"label1";
			this->label1->Size = System::Drawing::Size(100, 13);
			this->label1->TabIndex = 2;
			this->label1->Text = L"Parameters for view";
			// 
			// label2
			// 
			this->label2->AutoSize = true;
			this->label2->Location = System::Drawing::Point(184, 10);
			this->label2->Name = L"label2";
			this->label2->Size = System::Drawing::Size(105, 13);
			this->label2->TabIndex = 3;
			this->label2->Text = L"Parameters to create";
			// 
			// label3
			// 
			this->label3->AutoSize = true;
			this->label3->Location = System::Drawing::Point(21, 143);
			this->label3->Name = L"label3";
			this->label3->Size = System::Drawing::Size(60, 13);
			this->label3->TabIndex = 4;
			this->label3->Text = L"Need items";
			// 
			// label4
			// 
			this->label4->AutoSize = true;
			this->label4->Location = System::Drawing::Point(184, 143);
			this->label4->Name = L"label4";
			this->label4->Size = System::Drawing::Size(58, 13);
			this->label4->TabIndex = 5;
			this->label4->Text = L"Need tools";
			// 
			// label5
			// 
			this->label5->AutoSize = true;
			this->label5->Location = System::Drawing::Point(184, 257);
			this->label5->Name = L"label5";
			this->label5->Size = System::Drawing::Size(66, 13);
			this->label5->TabIndex = 6;
			this->label5->Text = L"Output items";
			// 
			// lbParamCraft
			// 
			this->lbParamCraft->FormattingEnabled = true;
			this->lbParamCraft->HorizontalScrollbar = true;
			this->lbParamCraft->Location = System::Drawing::Point(184, 32);
			this->lbParamCraft->Name = L"lbParamCraft";
			this->lbParamCraft->Size = System::Drawing::Size(167, 95);
			this->lbParamCraft->TabIndex = 7;
			this->lbParamCraft->MouseClick += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseClick);
			this->lbParamCraft->MouseDoubleClick += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseDoubleClick);
			this->lbParamCraft->MouseDown += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseDown);
			// 
			// lbNeedItems
			// 
			this->lbNeedItems->FormattingEnabled = true;
			this->lbNeedItems->HorizontalScrollbar = true;
			this->lbNeedItems->Location = System::Drawing::Point(11, 159);
			this->lbNeedItems->Name = L"lbNeedItems";
			this->lbNeedItems->Size = System::Drawing::Size(167, 95);
			this->lbNeedItems->TabIndex = 8;
			this->lbNeedItems->MouseClick += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseClick);
			this->lbNeedItems->MouseDoubleClick += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseDoubleClick);
			this->lbNeedItems->MouseDown += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseDown);
			// 
			// lbNeedTools
			// 
			this->lbNeedTools->FormattingEnabled = true;
			this->lbNeedTools->HorizontalScrollbar = true;
			this->lbNeedTools->Location = System::Drawing::Point(184, 159);
			this->lbNeedTools->Name = L"lbNeedTools";
			this->lbNeedTools->Size = System::Drawing::Size(167, 95);
			this->lbNeedTools->TabIndex = 9;
			this->lbNeedTools->MouseClick += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseClick);
			this->lbNeedTools->MouseDoubleClick += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseDoubleClick);
			this->lbNeedTools->MouseDown += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseDown);
			// 
			// lbOutItems
			// 
			this->lbOutItems->FormattingEnabled = true;
			this->lbOutItems->HorizontalScrollbar = true;
			this->lbOutItems->Location = System::Drawing::Point(184, 278);
			this->lbOutItems->Name = L"lbOutItems";
			this->lbOutItems->Size = System::Drawing::Size(167, 95);
			this->lbOutItems->TabIndex = 10;
			this->lbOutItems->MouseDoubleClick += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbParamShow_MouseDoubleClick);
			// 
			// cbParamNum
			// 
			this->cbParamNum->DropDownHeight = 600;
			this->cbParamNum->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
			this->cbParamNum->DropDownWidth = 300;
			this->cbParamNum->FormattingEnabled = true;
			this->cbParamNum->IntegralHeight = false;
			this->cbParamNum->Location = System::Drawing::Point(421, 32);
			this->cbParamNum->MaxDropDownItems = 50;
			this->cbParamNum->Name = L"cbParamNum";
			this->cbParamNum->Size = System::Drawing::Size(143, 21);
			this->cbParamNum->TabIndex = 11;
			// 
			// numParamCount
			// 
			this->numParamCount->Location = System::Drawing::Point(444, 60);
			this->numParamCount->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {2000000000, 0, 0, 0});
			this->numParamCount->Name = L"numParamCount";
			this->numParamCount->Size = System::Drawing::Size(120, 20);
			this->numParamCount->TabIndex = 12;
			// 
			// cbItemPid
			// 
			this->cbItemPid->DropDownHeight = 600;
			this->cbItemPid->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
			this->cbItemPid->DropDownWidth = 300;
			this->cbItemPid->FormattingEnabled = true;
			this->cbItemPid->IntegralHeight = false;
			this->cbItemPid->Location = System::Drawing::Point(421, 159);
			this->cbItemPid->Name = L"cbItemPid";
			this->cbItemPid->Size = System::Drawing::Size(143, 21);
			this->cbItemPid->TabIndex = 14;
			// 
			// numItemCount
			// 
			this->numItemCount->Location = System::Drawing::Point(444, 186);
			this->numItemCount->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {2000000000, 0, 0, 0});
			this->numItemCount->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			this->numItemCount->Name = L"numItemCount";
			this->numItemCount->Size = System::Drawing::Size(120, 20);
			this->numItemCount->TabIndex = 15;
			this->numItemCount->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			// 
			// label6
			// 
			this->label6->AutoSize = true;
			this->label6->Location = System::Drawing::Point(357, 35);
			this->label6->Name = L"label6";
			this->label6->Size = System::Drawing::Size(55, 13);
			this->label6->TabIndex = 18;
			this->label6->Text = L"Parameter";
			// 
			// label7
			// 
			this->label7->AutoSize = true;
			this->label7->Location = System::Drawing::Point(357, 62);
			this->label7->Name = L"label7";
			this->label7->Size = System::Drawing::Size(34, 13);
			this->label7->TabIndex = 19;
			this->label7->Text = L"Value";
			// 
			// label9
			// 
			this->label9->AutoSize = true;
			this->label9->Location = System::Drawing::Point(357, 162);
			this->label9->Name = L"label9";
			this->label9->Size = System::Drawing::Size(44, 13);
			this->label9->TabIndex = 21;
			this->label9->Text = L"Item pid";
			// 
			// label10
			// 
			this->label10->AutoSize = true;
			this->label10->Location = System::Drawing::Point(357, 188);
			this->label10->Name = L"label10";
			this->label10->Size = System::Drawing::Size(35, 13);
			this->label10->TabIndex = 22;
			this->label10->Text = L"Count";
			// 
			// label13
			// 
			this->label13->AutoSize = true;
			this->label13->Location = System::Drawing::Point(143, 382);
			this->label13->Name = L"label13";
			this->label13->Size = System::Drawing::Size(44, 13);
			this->label13->TabIndex = 26;
			this->label13->Text = L"Number";
			// 
			// txtCraftName
			// 
			this->txtCraftName->Location = System::Drawing::Point(46, 379);
			this->txtCraftName->MaxLength = 128;
			this->txtCraftName->Name = L"txtCraftName";
			this->txtCraftName->Size = System::Drawing::Size(94, 20);
			this->txtCraftName->TabIndex = 27;
			// 
			// numCraftNum
			// 
			this->numCraftNum->Location = System::Drawing::Point(189, 379);
			this->numCraftNum->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {2000000000, 0, 0, 0});
			this->numCraftNum->Name = L"numCraftNum";
			this->numCraftNum->Size = System::Drawing::Size(53, 20);
			this->numCraftNum->TabIndex = 28;
			// 
			// tabControl1
			// 
			this->tabControl1->Controls->Add(this->tpMain);
			this->tabControl1->Controls->Add(this->tpCraft);
			this->tabControl1->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tabControl1->Location = System::Drawing::Point(0, 0);
			this->tabControl1->Name = L"tabControl1";
			this->tabControl1->SelectedIndex = 0;
			this->tabControl1->Size = System::Drawing::Size(580, 479);
			this->tabControl1->TabIndex = 29;
			// 
			// tpMain
			// 
			this->tpMain->Controls->Add(this->btnLoad2);
			this->tpMain->Controls->Add(this->btnSave);
			this->tpMain->Controls->Add(this->btnLoad);
			this->tpMain->Controls->Add(this->lbCraft);
			this->tpMain->Location = System::Drawing::Point(4, 22);
			this->tpMain->Name = L"tpMain";
			this->tpMain->Padding = System::Windows::Forms::Padding(3);
			this->tpMain->Size = System::Drawing::Size(572, 453);
			this->tpMain->TabIndex = 1;
			this->tpMain->Text = L"Main";
			this->tpMain->UseVisualStyleBackColor = true;
			// 
			// btnLoad2
			// 
			this->btnLoad2->Location = System::Drawing::Point(6, 46);
			this->btnLoad2->Name = L"btnLoad2";
			this->btnLoad2->Size = System::Drawing::Size(143, 34);
			this->btnLoad2->TabIndex = 13;
			this->btnLoad2->Text = L"Apply list";
			this->btnLoad2->UseVisualStyleBackColor = true;
			this->btnLoad2->Click += gcnew System::EventHandler(this, &Form1::btnLoad_Click);
			// 
			// btnSave
			// 
			this->btnSave->Location = System::Drawing::Point(6, 86);
			this->btnSave->Name = L"btnSave";
			this->btnSave->Size = System::Drawing::Size(143, 34);
			this->btnSave->TabIndex = 6;
			this->btnSave->Text = L"Save list";
			this->btnSave->UseVisualStyleBackColor = true;
			this->btnSave->Click += gcnew System::EventHandler(this, &Form1::btnSave_Click);
			// 
			// btnLoad
			// 
			this->btnLoad->Location = System::Drawing::Point(6, 6);
			this->btnLoad->Name = L"btnLoad";
			this->btnLoad->Size = System::Drawing::Size(143, 34);
			this->btnLoad->TabIndex = 1;
			this->btnLoad->Text = L"Load list";
			this->btnLoad->UseVisualStyleBackColor = true;
			this->btnLoad->Click += gcnew System::EventHandler(this, &Form1::btnLoad_Click);
			// 
			// lbCraft
			// 
			this->lbCraft->FormattingEnabled = true;
			this->lbCraft->Location = System::Drawing::Point(155, 6);
			this->lbCraft->Name = L"lbCraft";
			this->lbCraft->Size = System::Drawing::Size(409, 394);
			this->lbCraft->TabIndex = 0;
			this->lbCraft->SelectedIndexChanged += gcnew System::EventHandler(this, &Form1::lbCraft_SelectedIndexChanged);
			this->lbCraft->MouseDoubleClick += gcnew System::Windows::Forms::MouseEventHandler(this, &Form1::lbCraft_MouseDoubleClick);
			// 
			// tpCraft
			// 
			this->tpCraft->Controls->Add(this->numExperience);
			this->tpCraft->Controls->Add(this->label16);
			this->tpCraft->Controls->Add(this->label11);
			this->tpCraft->Controls->Add(this->label8);
			this->tpCraft->Controls->Add(this->txtScript);
			this->tpCraft->Controls->Add(this->btnClear);
			this->tpCraft->Controls->Add(this->cbOr);
			this->tpCraft->Controls->Add(this->btnAddOutItem);
			this->tpCraft->Controls->Add(this->btnAddNeedTool);
			this->tpCraft->Controls->Add(this->btnAddNeedItem);
			this->tpCraft->Controls->Add(this->label14);
			this->tpCraft->Controls->Add(this->txtCraftInfo);
			this->tpCraft->Controls->Add(this->btnAddParamCraft);
			this->tpCraft->Controls->Add(this->btnAddParamShow);
			this->tpCraft->Controls->Add(this->btnSaveCraft);
			this->tpCraft->Controls->Add(this->numCraftNum);
			this->tpCraft->Controls->Add(this->lbOutItems);
			this->tpCraft->Controls->Add(this->txtCraftName);
			this->tpCraft->Controls->Add(this->lbParamShow);
			this->tpCraft->Controls->Add(this->label13);
			this->tpCraft->Controls->Add(this->label1);
			this->tpCraft->Controls->Add(this->label2);
			this->tpCraft->Controls->Add(this->label3);
			this->tpCraft->Controls->Add(this->label4);
			this->tpCraft->Controls->Add(this->label10);
			this->tpCraft->Controls->Add(this->label5);
			this->tpCraft->Controls->Add(this->label9);
			this->tpCraft->Controls->Add(this->lbParamCraft);
			this->tpCraft->Controls->Add(this->label7);
			this->tpCraft->Controls->Add(this->lbNeedItems);
			this->tpCraft->Controls->Add(this->label6);
			this->tpCraft->Controls->Add(this->lbNeedTools);
			this->tpCraft->Controls->Add(this->cbParamNum);
			this->tpCraft->Controls->Add(this->numParamCount);
			this->tpCraft->Controls->Add(this->numItemCount);
			this->tpCraft->Controls->Add(this->cbItemPid);
			this->tpCraft->Location = System::Drawing::Point(4, 22);
			this->tpCraft->Name = L"tpCraft";
			this->tpCraft->Padding = System::Windows::Forms::Padding(3);
			this->tpCraft->Size = System::Drawing::Size(572, 453);
			this->tpCraft->TabIndex = 0;
			this->tpCraft->Text = L"Craft";
			this->tpCraft->UseVisualStyleBackColor = true;
			this->tpCraft->Click += gcnew System::EventHandler(this, &Form1::tpCraft_Click);
			// 
			// numExperience
			// 
			this->numExperience->Location = System::Drawing::Point(317, 379);
			this->numExperience->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {10000, 0, 0, 0});
			this->numExperience->Name = L"numExperience";
			this->numExperience->Size = System::Drawing::Size(49, 20);
			this->numExperience->TabIndex = 44;
			this->numExperience->ValueChanged += gcnew System::EventHandler(this, &Form1::numExperience_ValueChanged);
			// 
			// label16
			// 
			this->label16->AutoSize = true;
			this->label16->Location = System::Drawing::Point(251, 381);
			this->label16->Name = L"label16";
			this->label16->Size = System::Drawing::Size(60, 13);
			this->label16->TabIndex = 43;
			this->label16->Text = L"Experience";
			// 
			// label11
			// 
			this->label11->AutoSize = true;
			this->label11->Location = System::Drawing::Point(8, 409);
			this->label11->Name = L"label11";
			this->label11->Size = System::Drawing::Size(34, 13);
			this->label11->TabIndex = 41;
			this->label11->Text = L"Script";
			// 
			// label8
			// 
			this->label8->AutoSize = true;
			this->label8->Location = System::Drawing::Point(8, 382);
			this->label8->Name = L"label8";
			this->label8->Size = System::Drawing::Size(35, 13);
			this->label8->TabIndex = 40;
			this->label8->Text = L"Name";
			// 
			// txtScript
			// 
			this->txtScript->Location = System::Drawing::Point(46, 405);
			this->txtScript->Name = L"txtScript";
			this->txtScript->Size = System::Drawing::Size(320, 20);
			this->txtScript->TabIndex = 39;
			this->txtScript->TextChanged += gcnew System::EventHandler(this, &Form1::txtScript_TextChanged);
			// 
			// btnClear
			// 
			this->btnClear->Location = System::Drawing::Point(397, 409);
			this->btnClear->Name = L"btnClear";
			this->btnClear->Size = System::Drawing::Size(167, 23);
			this->btnClear->TabIndex = 38;
			this->btnClear->Text = L"Clear";
			this->btnClear->UseVisualStyleBackColor = true;
			this->btnClear->Click += gcnew System::EventHandler(this, &Form1::btnClear_Click);
			// 
			// cbOr
			// 
			this->cbOr->AutoSize = true;
			this->cbOr->Location = System::Drawing::Point(360, 6);
			this->cbOr->Name = L"cbOr";
			this->cbOr->Size = System::Drawing::Size(169, 17);
			this->cbOr->TabIndex = 37;
			this->cbOr->Text = L"OR (if not checked than AND)";
			this->cbOr->UseVisualStyleBackColor = true;
			// 
			// btnAddOutItem
			// 
			this->btnAddOutItem->Location = System::Drawing::Point(360, 278);
			this->btnAddOutItem->Name = L"btnAddOutItem";
			this->btnAddOutItem->Size = System::Drawing::Size(98, 41);
			this->btnAddOutItem->TabIndex = 36;
			this->btnAddOutItem->Text = L"Output item";
			this->btnAddOutItem->UseVisualStyleBackColor = true;
			this->btnAddOutItem->Click += gcnew System::EventHandler(this, &Form1::btnAddOutItem_Click);
			// 
			// btnAddNeedTool
			// 
			this->btnAddNeedTool->Location = System::Drawing::Point(466, 213);
			this->btnAddNeedTool->Name = L"btnAddNeedTool";
			this->btnAddNeedTool->Size = System::Drawing::Size(98, 41);
			this->btnAddNeedTool->TabIndex = 35;
			this->btnAddNeedTool->Text = L"Need tool";
			this->btnAddNeedTool->UseVisualStyleBackColor = true;
			this->btnAddNeedTool->Click += gcnew System::EventHandler(this, &Form1::btnAddNeedTool_Click);
			// 
			// btnAddNeedItem
			// 
			this->btnAddNeedItem->Location = System::Drawing::Point(360, 213);
			this->btnAddNeedItem->Name = L"btnAddNeedItem";
			this->btnAddNeedItem->Size = System::Drawing::Size(98, 41);
			this->btnAddNeedItem->TabIndex = 34;
			this->btnAddNeedItem->Text = L"Need item";
			this->btnAddNeedItem->UseVisualStyleBackColor = true;
			this->btnAddNeedItem->Click += gcnew System::EventHandler(this, &Form1::btnAddNeedItem_Click);
			// 
			// label14
			// 
			this->label14->AutoSize = true;
			this->label14->Location = System::Drawing::Point(11, 257);
			this->label14->Name = L"label14";
			this->label14->Size = System::Drawing::Size(60, 13);
			this->label14->TabIndex = 33;
			this->label14->Text = L"Description";
			// 
			// txtCraftInfo
			// 
			this->txtCraftInfo->Location = System::Drawing::Point(11, 277);
			this->txtCraftInfo->MaxLength = 256;
			this->txtCraftInfo->Name = L"txtCraftInfo";
			this->txtCraftInfo->Size = System::Drawing::Size(167, 96);
			this->txtCraftInfo->TabIndex = 32;
			this->txtCraftInfo->Text = L"";
			// 
			// btnAddParamCraft
			// 
			this->btnAddParamCraft->Location = System::Drawing::Point(466, 86);
			this->btnAddParamCraft->Name = L"btnAddParamCraft";
			this->btnAddParamCraft->Size = System::Drawing::Size(98, 41);
			this->btnAddParamCraft->TabIndex = 31;
			this->btnAddParamCraft->Text = L"For craft";
			this->btnAddParamCraft->UseVisualStyleBackColor = true;
			this->btnAddParamCraft->Click += gcnew System::EventHandler(this, &Form1::btnAddParamCraft_Click);
			// 
			// btnAddParamShow
			// 
			this->btnAddParamShow->Location = System::Drawing::Point(360, 86);
			this->btnAddParamShow->Name = L"btnAddParamShow";
			this->btnAddParamShow->Size = System::Drawing::Size(98, 41);
			this->btnAddParamShow->TabIndex = 30;
			this->btnAddParamShow->Text = L"For view";
			this->btnAddParamShow->UseVisualStyleBackColor = true;
			this->btnAddParamShow->Click += gcnew System::EventHandler(this, &Form1::btnAddParamShow_Click);
			// 
			// btnSaveCraft
			// 
			this->btnSaveCraft->Location = System::Drawing::Point(397, 355);
			this->btnSaveCraft->Name = L"btnSaveCraft";
			this->btnSaveCraft->Size = System::Drawing::Size(167, 48);
			this->btnSaveCraft->TabIndex = 29;
			this->btnSaveCraft->Text = L"Add to list";
			this->btnSaveCraft->UseVisualStyleBackColor = true;
			this->btnSaveCraft->Click += gcnew System::EventHandler(this, &Form1::btnSaveCraft_Click);
			// 
			// lErr
			// 
			this->lErr->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left));
			this->lErr->AutoSize = true;
			this->lErr->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 8.25F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->lErr->Location = System::Drawing::Point(7, 460);
			this->lErr->Name = L"lErr";
			this->lErr->Size = System::Drawing::Size(43, 13);
			this->lErr->TabIndex = 7;
			this->lErr->Text = L"Status";
			this->lErr->Click += gcnew System::EventHandler(this, &Form1::lErr_Click);
			// 
			// statusStrip1
			// 
			this->statusStrip1->Location = System::Drawing::Point(0, 457);
			this->statusStrip1->Name = L"statusStrip1";
			this->statusStrip1->Size = System::Drawing::Size(580, 22);
			this->statusStrip1->TabIndex = 30;
			this->statusStrip1->Text = L"statusStrip1";
			// 
			// Form1
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(580, 479);
			this->Controls->Add(this->lErr);
			this->Controls->Add(this->statusStrip1);
			this->Controls->Add(this->tabControl1);
			this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedSingle;
			this->MaximizeBox = false;
			this->Name = L"Form1";
			this->Text = L"MrFixit Editor   v1.13";
			this->Load += gcnew System::EventHandler(this, &Form1::Form1_Load);
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numParamCount))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numItemCount))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numCraftNum))->EndInit();
			this->tabControl1->ResumeLayout(false);
			this->tpMain->ResumeLayout(false);
			this->tpCraft->ResumeLayout(false);
			this->tpCraft->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numExperience))->EndInit();
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion

private: System::Void Form1_Load(System::Object^  sender, System::EventArgs^  e)
{
	WriteLog("MrFixit initialization...\n");
	srand(GetTickCount());
	setlocale(LC_ALL,"Russian");
	CurCraft=new CraftItem();

	IniParser cfg;
	cfg.LoadFile("MrFixit.cfg",PT_ROOT);
	char path[MAX_FOPATH];

	cfg.GetStr("ServerPath",".\\",path);
	FileManager::InitDataFiles(path);

	ConstantsManager::Initialize(PT_SERVER_CONFIGS);
	UpdateParamsLists();
	WriteLog("MrFixit initialization complete.\n");
}

private: System::Void btnLoad_Click(System::Object^  sender, System::EventArgs^  e)
{
	OpenFileDialog^ dlg=gcnew OpenFileDialog();
	dlg->Filter="FOnline MSG files|*.msg";
	dlg->RestoreDirectory=true;
	if(dlg->ShowDialog()!=System::Windows::Forms::DialogResult::OK) return;
	if(sender==btnLoad) MrFixit.Finish();
	if(MrFixit.LoadCrafts(ToAnsi(dlg->FileName))) Log("Load ok.");
	else Log("Load with errors.");
	UpdateCraftsList();
}

private: System::Void btnSave_Click(System::Object^  sender, System::EventArgs^  e)
{
	SaveFileDialog^ dlg=gcnew SaveFileDialog();
	dlg->Filter="FOnline MSG files|*.msg";
	dlg->RestoreDirectory=true;
	if(dlg->ShowDialog()!=System::Windows::Forms::DialogResult::OK) return;
	if(MrFixit.SaveCrafts(ToAnsi(dlg->FileName))) Log("Save ok.");
	else Log("Save fail.");
}

private: System::Void btnLoad2_Click(System::Object^  sender, System::EventArgs^  e){}
private: System::Void lErr_Click(System::Object^  sender, System::EventArgs^  e){}
private: System::Void button2_Click(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void button3_Click(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void tpCraft_Click(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void checkBox1_CheckedChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void lbCraft_SelectedIndexChanged(System::Object^  sender, System::EventArgs^  e) {}
private: System::Void statusStrip1_ItemClicked(System::Object^  sender, System::Windows::Forms::ToolStripItemClickedEventArgs^  e){}

private: System::Void cbStats_CheckedChanged(System::Object^  sender, System::EventArgs^  e)
{
	UpdateParamsLists();
}

private: System::Void lbCraft_MouseDoubleClick(System::Object^  sender, System::Windows::Forms::MouseEventArgs^  e)
{
	if(lbCraft->SelectedIndex==-1) return;

	String^ str=lbCraft->SelectedItem->ToString();
	int pos=str->IndexOf(' ');
	int cnt=str->Length-pos;

	str=str->Remove(pos,cnt);
	int num=Convert::ToInt32(str);

	CraftItem* craft=MrFixit.GetCraft(num);
	if(!craft)
	{
		Log("Craft "+num+" not found.");
		return;
	}

	*CurCraft=*craft;
	ParseCraft(CurCraft);
}

void ParseCraft(CraftItem* craft)
{
	int errors=0;

	numCraftNum->Value=(int)craft->Num;
	txtCraftName->Text=ToClrString(craft->Name.c_str());
	txtCraftInfo->Text=ToClrString(craft->Info.c_str());

	// Param show
	lbParamShow->Items->Clear();
	for(size_t i=0,j=craft->ShowPNum.size();i<j;i++)
	{
		DWORD param_num=craft->ShowPNum[i];
		int param_val=craft->ShowPVal[i];
		char* s=(char*)ConstantsManager::GetParamName(param_num);
		if(!s)
		{
			errors++;
			continue;
		}

		String^ s2=ToClrString(s)+" "+param_val+"%";
		if(craft->ShowPOr[i]) s2+=" or";
		else s2+=" and";

		lbParamShow->Items->Add(s2);
	}

	// Param craft
	lbParamCraft->Items->Clear();
	for(size_t i=0,j=craft->NeedPNum.size();i<j;i++)
	{
		DWORD param_num=craft->NeedPNum[i];
		int param_val=craft->NeedPVal[i];

		const char* s=ConstantsManager::GetParamName(param_num);
		if(!s)
		{
			errors++;
			continue;
		} 

		String^ s2=ToClrString(s)+" "+param_val+"%";
		if(craft->NeedPOr[i]) s2+=" or";
		else s2+=" and";

		lbParamCraft->Items->Add(s2);
	}

	// Items need
	lbNeedItems->Items->Clear();
	for(size_t i=0,j=craft->NeedItems.size();i<j;i++)
	{
		const char* s=ConstantsManager::GetItemName(craft->NeedItems[i]);
		if(!s)
		{
			errors++;
			continue;
		}

		String^ s2=ToClrString(s)+" "+craft->NeedItemsVal[i]+"רע.";
		if(craft->NeedItemsOr[i]) s2+=" or";
		else s2+=" and";

		lbNeedItems->Items->Add(s2);
	}

	// Tools need
	lbNeedTools->Items->Clear();
	for(size_t i=0,j=craft->NeedTools.size();i<j;i++)
	{
		const char* s=ConstantsManager::GetItemName(craft->NeedTools[i]);
		if(!s)
		{
			errors++;
			continue;
		}

		String^ s2=ToClrString(s)+" "+craft->NeedToolsVal[i]+"רע.";
		if(craft->NeedToolsOr[i]) s2+=" or";
		else s2+=" and";

		lbNeedTools->Items->Add(s2);
	}

	// Items out
	lbOutItems->Items->Clear();
	for(size_t i=0,j=craft->OutItems.size();i<j;i++)
	{
		const char* s=ConstantsManager::GetItemName(craft->OutItems[i]);
		if(!s)
		{
			errors++;
			continue;
		}

		String^ s2=ToClrString(s)+" "+craft->OutItemsVal[i]+"רע.";
		lbOutItems->Items->Add(s2);
	}

	// Other
	txtScript->Text=ToClrString(CurCraft->Script.c_str());
	numExperience->Value=(UINT)CurCraft->Experience;

	// End
	if(errors>0) Log("Loaded with errors, "+errors+".");
	else Log("Craft "+CurCraft->Num+" loaded ok.");
	tabControl1->SelectedIndex=1;
}

private: System::Void lbParamShow_MouseDoubleClick(System::Object^  sender, System::Windows::Forms::MouseEventArgs^  e)
{
	ListBox^ lb=dynamic_cast<ListBox^>(sender);
	if(lb==nullptr) return;

	int i=(int)lb->SelectedIndex;
	if(i==-1) return;

	if(sender==lbParamShow)
	{
		if(i>=(int)CurCraft->ShowPNum.size()) return;
		CurCraft->ShowPNum.erase(CurCraft->ShowPNum.begin()+i);
		CurCraft->ShowPVal.erase(CurCraft->ShowPVal.begin()+i);
		CurCraft->ShowPOr.erase(CurCraft->ShowPOr.begin()+i);
	}
	else if(sender==lbParamCraft)
	{
		if(i>=(int)CurCraft->ShowPNum.size()) return;
		CurCraft->NeedPNum.erase(CurCraft->NeedPNum.begin()+i);
		CurCraft->NeedPVal.erase(CurCraft->NeedPVal.begin()+i);
		CurCraft->NeedPOr.erase(CurCraft->NeedPOr.begin()+i);
	}
	else if(sender==lbNeedItems)
	{
		if(i>=(int)CurCraft->NeedItems.size()) return;
		CurCraft->NeedItems.erase(CurCraft->NeedItems.begin()+i);
		CurCraft->NeedItemsVal.erase(CurCraft->NeedItemsVal.begin()+i);
		CurCraft->NeedItemsOr.erase(CurCraft->NeedItemsOr.begin()+i);
	}
	else if(sender==lbNeedTools)
	{
		if(i>=(int)CurCraft->NeedTools.size()) return;
		CurCraft->NeedTools.erase(CurCraft->NeedTools.begin()+i);
		CurCraft->NeedToolsVal.erase(CurCraft->NeedToolsVal.begin()+i);
		CurCraft->NeedToolsOr.erase(CurCraft->NeedToolsOr.begin()+i);	
	}
	else if(sender==lbOutItems)
	{
		if(i>=(int)CurCraft->OutItems.size()) return;
		CurCraft->OutItems.erase(CurCraft->OutItems.begin()+i);
		CurCraft->OutItemsVal.erase(CurCraft->OutItemsVal.begin()+i);
	}
	else
		return;

	ParseCraft(CurCraft);
}

private: System::Void btnAddParamShow_Click(System::Object^  sender, System::EventArgs^  e)
{
	if(cbParamNum->SelectedIndex==-1 || cbParamNum->Text=="")
	{
		Log("Invalid parameter.");
		return;
	}

	String^ name=cbParamNum->Text;
	int p_num=ConstantsManager::GetParamId(ToAnsi(name));
	int p_val=(int)numParamCount->Value;
	BYTE p_or=cbOr->Checked==true?1:0;

	if(p_num<0)
	{
		Log("Parameter not find.");
		return;
	}

	if(sender==btnAddParamShow)
	{
		CurCraft->ShowPNum.push_back(p_num);
		CurCraft->ShowPVal.push_back(p_val);
		CurCraft->ShowPOr.push_back(p_or);
	}
	else if(sender==btnAddParamCraft)
	{
		CurCraft->NeedPNum.push_back(p_num);
		CurCraft->NeedPVal.push_back(p_val);
		CurCraft->NeedPOr.push_back(p_or);
	}

	ParseCraft(CurCraft);
}

private: System::Void btnAddParamCraft_Click(System::Object^  sender, System::EventArgs^  e)
{
	btnAddParamShow_Click(sender,e);
}

private: System::Void btnAddNeedItem_Click(System::Object^  sender, System::EventArgs^  e)
{
	if(cbItemPid->SelectedIndex==-1 || cbItemPid->Text=="")
	{
		Log("Invalid item.");
		return;
	}

	int i_pid=ConstantsManager::GetItemPid(ToAnsi(cbItemPid->Text));
	if(i_pid<0)
	{
		Log("Item not find.");
		return;
	}

	DWORD i_count=(int)numItemCount->Value;
	if(!i_count) return;

	BYTE i_or=(cbOr->Checked==true?1:0);

	if(sender==btnAddNeedItem)
	{
		CurCraft->NeedItems.push_back(i_pid);
		CurCraft->NeedItemsVal.push_back(i_count);
		CurCraft->NeedItemsOr.push_back(i_or);
	}
	else if(sender==btnAddNeedTool)
	{
		CurCraft->NeedTools.push_back(i_pid);
		CurCraft->NeedToolsVal.push_back(i_count);
		CurCraft->NeedToolsOr.push_back(i_or);
	}
	else if(sender==btnAddOutItem)
	{
		CurCraft->OutItems.push_back(i_pid);
		CurCraft->OutItemsVal.push_back(i_count);
	}

	ParseCraft(CurCraft);
}

private: System::Void btnAddNeedTool_Click(System::Object^  sender, System::EventArgs^  e)
{
	btnAddNeedItem_Click(sender,e);
}

private: System::Void btnAddOutItem_Click(System::Object^  sender, System::EventArgs^  e)
{
	btnAddNeedItem_Click(sender,e);
}

private: System::Void btnSaveCraft_Click(System::Object^  sender, System::EventArgs^  e)
{
	CurCraft->Num=(int)numCraftNum->Value;
	CurCraft->Name=string(ToAnsi(txtCraftName->Text));
	CurCraft->Info=string(ToAnsi(txtCraftInfo->Text));

	if(!CurCraft->IsValid())
	{
		Log("Craft is not valid.");
		return;
	}

	if(MrFixit.IsCraftExist(CurCraft->Num))
	{
		if(MessageBox::Show("Rewrite?","MrFixit Editor",
			MessageBoxButtons::YesNo,
			MessageBoxIcon::Information)==System::Windows::Forms::DialogResult::No) return;

		MrFixit.EraseCraft(CurCraft->Num);
	}

	if(MrFixit.AddCraft(CurCraft,TRUE)) Log("Added.");
	else Log("Not added.");

	UpdateCraftsList();
}

private: System::Void lbParamShow_MouseClick(System::Object^  sender, System::Windows::Forms::MouseEventArgs^  e){}
private: System::Void lbParamShow_DoubleClick(System::Object^  sender, System::EventArgs^  e){}

private: System::Void lbParamShow_MouseDown(System::Object^  sender, System::Windows::Forms::MouseEventArgs^  e)
{
	if(e->Button!=Windows::Forms::MouseButtons::Right) return;

	ListBox^ lb=dynamic_cast<ListBox^>(sender);
	if(!lb) return;

	int pos=(int)lb->SelectedIndex;
	if(pos==-1) return;

	if(sender==lbParamShow)
	{
		if(pos>=(int)CurCraft->ShowPNum.size()) return;
		CurCraft->ShowPOr[pos]=!CurCraft->ShowPOr[pos];
	}
	else if(sender==lbParamCraft)
	{
		if(pos>=(int)CurCraft->ShowPNum.size()) return;
		CurCraft->NeedPOr[pos]=!CurCraft->NeedPOr[pos];
	}
	else if(sender==lbNeedItems)
	{
		if(pos>=(int)CurCraft->NeedItems.size()) return;
		CurCraft->NeedItemsOr[pos]=!CurCraft->NeedItemsOr[pos];
	}
	else if(sender==lbNeedTools)
	{
		if(pos>=(int)CurCraft->NeedTools.size()) return;
		CurCraft->NeedToolsOr[pos]=!CurCraft->NeedToolsOr[pos];
	}

	ParseCraft(CurCraft);
}
private: System::Void btnClear_Click(System::Object^  sender, System::EventArgs^  e)
{
	if(MessageBox::Show("Really?","MrFixit Editor",
		MessageBoxButtons::YesNo,
		MessageBoxIcon::Information)==System::Windows::Forms::DialogResult::No) return;

	CurCraft->Clear();
	ParseCraft(CurCraft);

	Log("Craft cleared.");
}
private: System::Void cbWithScript_CheckedChanged(System::Object^  sender, System::EventArgs^  e){}
private: System::Void txtScript_TextChanged(System::Object^  sender, System::EventArgs^  e)
{
	CurCraft->Script=ToAnsi(txtScript->Text);
}
private: System::Void numExperience_ValueChanged(System::Object^  sender, System::EventArgs^  e)
{
	CurCraft->Experience=(UINT)numExperience->Value;
}
};
}

