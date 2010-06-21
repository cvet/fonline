#pragma once
#include "formDebug.h"

namespace ScriptEditor {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;
	using namespace System::IO;

	static void Log(String^ str);
	static void CallBack(const asSMessageInfo *msg, void *param);

	char* path_dir;
	asIScriptEngine* scriptEngine;

	bool EnableFormat;

	char ProgramName[256];
	char ScriptInfo[256];
	char ScriptExt[256];
	char DllName[256];
	char PathScripts[256];
	char PathData[256];
	char Language[256];
	char textLine[256];
	char textCol[256];

	int curNewBox;

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
		ref class Words
		{
		public:
			String^ words;

			System::Drawing::Font^ fontStyle;
			System::Drawing::Color fontColor;
		};

		ref class EditBox
		{
		public:
			String^ name;
			String^ filename;
			RichTextBox^ RTB;

			bool edited;
			bool compiled;

			CBytecodeStream* textcode;
			CBytecodeStream* bytecode;

			void SetName(String^ fname)
			{
				filename=fname;
				if(filename==nullptr)
				{
					filename="";
					name="Script "+curNewBox;
				}
				else
					name=filename->Substring(filename->LastIndexOf("\\")+1,filename->Length-filename->LastIndexOf("\\")-1);

				curNewBox++;
			}

			void SetTextcode(const char* code, UINT size)
			{
				if(!textcode) textcode=new CBytecodeStream;
				textcode->Clear();
				textcode->Write(code,size);
			}

			void SetBytecode(CBytecodeStream* _bytecode)
			{
				if(bytecode) delete bytecode;
				bytecode=_bytecode;
				if(bytecode) bytecode->SetPos(0,0);
			}

			EditBox(){name="";filename="";RTB=nullptr;edited=false;compiled=false;textcode=NULL;bytecode=NULL;}
		};

		RichTextBox^ RTBlex;

		array<Words^>^ Lexems;
		array<EditBox^>^ EditBoxes;

		char CommentChar0;
		char CommentChar1;
		char StringChar;

		System::Drawing::Font^ fontDefault;
		System::Drawing::Font^ fontComments;
		System::Drawing::Font^ fontString;
		System::Drawing::Font^ fontDigit;
		System::Drawing::Color colorDefault;
		System::Drawing::Color colorComments;
		System::Drawing::Color colorString;
		System::Drawing::Color colorDigit;

		bool lexemsOff;
		bool commentsOff;
		bool stringOff;

		int LastBoxTab;

		array<String^>^ Buffer;
		int CurBuf;

		String^ strBtnDbgExecute;
		String^ strBtnDbgClose;
		String^ strLabDbgFunc;
		String^ strLabDbgParam;
		String^ strLabDbgVal;
	public: static System::Windows::Forms::ComboBox^  cmbLog;

	private: System::Windows::Forms::Label^  lLine;
	private: System::Windows::Forms::Label^  lCol;
	private: System::Windows::Forms::Button^  bClear;
	private: System::Windows::Forms::TabPage^  mtHelp;
	private: System::Windows::Forms::TabControl^  tcHelp;
	private: System::Windows::Forms::LinkLabel^  lAuthor;
	private: System::Windows::Forms::Label^  lVersion;
	private: System::Windows::Forms::Button^  bNew;
	private: System::Windows::Forms::TabControl^  tcEdit;
	private: System::Windows::Forms::ProgressBar^  pb;
	private: System::Windows::Forms::TabPage^  mtBytecode;
	private: System::Windows::Forms::RichTextBox^  tBytecode;
	private: System::Windows::Forms::Button^  bDebug;
private: System::Windows::Forms::TabPage^  tModules;
private: System::Windows::Forms::Button^  bAddModule;
private: System::Windows::Forms::ListBox^  lbModules;

	private: System::Windows::Forms::Button^  bSave;

	public:
		Form1(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here
			//
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
	private: System::Windows::Forms::TabPage^  mtEdit;
	private: System::Windows::Forms::TabPage^  mtPrep;
	private: System::Windows::Forms::RichTextBox^  tPrep;
	private: System::Windows::Forms::Button^  bLoad;
	private: System::Windows::Forms::Button^  bSaveAs;
	private: System::Windows::Forms::Button^  bCompile;

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
			this->tabControl1 = (gcnew System::Windows::Forms::TabControl());
			this->mtEdit = (gcnew System::Windows::Forms::TabPage());
			this->tcEdit = (gcnew System::Windows::Forms::TabControl());
			this->mtPrep = (gcnew System::Windows::Forms::TabPage());
			this->tPrep = (gcnew System::Windows::Forms::RichTextBox());
			this->tModules = (gcnew System::Windows::Forms::TabPage());
			this->bAddModule = (gcnew System::Windows::Forms::Button());
			this->lbModules = (gcnew System::Windows::Forms::ListBox());
			this->mtBytecode = (gcnew System::Windows::Forms::TabPage());
			this->tBytecode = (gcnew System::Windows::Forms::RichTextBox());
			this->mtHelp = (gcnew System::Windows::Forms::TabPage());
			this->tcHelp = (gcnew System::Windows::Forms::TabControl());
			this->cmbLog = (gcnew System::Windows::Forms::ComboBox());
			this->bLoad = (gcnew System::Windows::Forms::Button());
			this->bSaveAs = (gcnew System::Windows::Forms::Button());
			this->bCompile = (gcnew System::Windows::Forms::Button());
			this->bSave = (gcnew System::Windows::Forms::Button());
			this->lLine = (gcnew System::Windows::Forms::Label());
			this->lCol = (gcnew System::Windows::Forms::Label());
			this->bClear = (gcnew System::Windows::Forms::Button());
			this->lAuthor = (gcnew System::Windows::Forms::LinkLabel());
			this->lVersion = (gcnew System::Windows::Forms::Label());
			this->bNew = (gcnew System::Windows::Forms::Button());
			this->pb = (gcnew System::Windows::Forms::ProgressBar());
			this->bDebug = (gcnew System::Windows::Forms::Button());
			this->tabControl1->SuspendLayout();
			this->mtEdit->SuspendLayout();
			this->mtPrep->SuspendLayout();
			this->tModules->SuspendLayout();
			this->mtBytecode->SuspendLayout();
			this->mtHelp->SuspendLayout();
			this->SuspendLayout();
			// 
			// tabControl1
			// 
			this->tabControl1->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Bottom) 
				| System::Windows::Forms::AnchorStyles::Left) 
				| System::Windows::Forms::AnchorStyles::Right));
			this->tabControl1->Controls->Add(this->mtEdit);
			this->tabControl1->Controls->Add(this->mtPrep);
			this->tabControl1->Controls->Add(this->tModules);
			this->tabControl1->Controls->Add(this->mtBytecode);
			this->tabControl1->Controls->Add(this->mtHelp);
			this->tabControl1->Location = System::Drawing::Point(12, 12);
			this->tabControl1->Name = L"tabControl1";
			this->tabControl1->SelectedIndex = 0;
			this->tabControl1->Size = System::Drawing::Size(702, 486);
			this->tabControl1->TabIndex = 1;
			this->tabControl1->SelectedIndexChanged += gcnew System::EventHandler(this, &Form1::tabControl1_SelectedIndexChanged);
			// 
			// mtEdit
			// 
			this->mtEdit->Controls->Add(this->tcEdit);
			this->mtEdit->Location = System::Drawing::Point(4, 22);
			this->mtEdit->Name = L"mtEdit";
			this->mtEdit->Padding = System::Windows::Forms::Padding(3);
			this->mtEdit->Size = System::Drawing::Size(694, 460);
			this->mtEdit->TabIndex = 0;
			this->mtEdit->Text = L"Редактор";
			this->mtEdit->UseVisualStyleBackColor = true;
			// 
			// tcEdit
			// 
			this->tcEdit->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tcEdit->Font = (gcnew System::Drawing::Font(L"Courier New", 9.75F, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->tcEdit->Location = System::Drawing::Point(3, 3);
			this->tcEdit->Name = L"tcEdit";
			this->tcEdit->SelectedIndex = 0;
			this->tcEdit->Size = System::Drawing::Size(688, 454);
			this->tcEdit->TabIndex = 2;
			this->tcEdit->SelectedIndexChanged += gcnew System::EventHandler(this, &Form1::tcEdit_SelectedIndexChanged);
			// 
			// mtPrep
			// 
			this->mtPrep->Controls->Add(this->tPrep);
			this->mtPrep->Location = System::Drawing::Point(4, 22);
			this->mtPrep->Name = L"mtPrep";
			this->mtPrep->Padding = System::Windows::Forms::Padding(3);
			this->mtPrep->Size = System::Drawing::Size(694, 460);
			this->mtPrep->TabIndex = 1;
			this->mtPrep->Text = L"После препроцессинга";
			this->mtPrep->UseVisualStyleBackColor = true;
			// 
			// tPrep
			// 
			this->tPrep->BackColor = System::Drawing::SystemColors::Control;
			this->tPrep->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tPrep->Location = System::Drawing::Point(3, 3);
			this->tPrep->Name = L"tPrep";
			this->tPrep->ReadOnly = true;
			this->tPrep->Size = System::Drawing::Size(688, 454);
			this->tPrep->TabIndex = 0;
			this->tPrep->Text = L"";
			this->tPrep->SelectionChanged += gcnew System::EventHandler(this, &Form1::tPrep_SelectionChanged);
			this->tPrep->KeyDown += gcnew System::Windows::Forms::KeyEventHandler(this, &Form1::tPrep_KeyDown);
			// 
			// tModules
			// 
			this->tModules->Controls->Add(this->bAddModule);
			this->tModules->Controls->Add(this->lbModules);
			this->tModules->Location = System::Drawing::Point(4, 22);
			this->tModules->Name = L"tModules";
			this->tModules->Size = System::Drawing::Size(694, 460);
			this->tModules->TabIndex = 4;
			this->tModules->Text = L"Загруженные модули";
			this->tModules->UseVisualStyleBackColor = true;
			// 
			// bAddModule
			// 
			this->bAddModule->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right));
			this->bAddModule->Location = System::Drawing::Point(272, 16);
			this->bAddModule->Name = L"bAddModule";
			this->bAddModule->Size = System::Drawing::Size(127, 38);
			this->bAddModule->TabIndex = 1;
			this->bAddModule->Text = L"Добавить текущий";
			this->bAddModule->UseVisualStyleBackColor = true;
			// 
			// lbModules
			// 
			this->lbModules->Dock = System::Windows::Forms::DockStyle::Left;
			this->lbModules->FormattingEnabled = true;
			this->lbModules->Location = System::Drawing::Point(0, 0);
			this->lbModules->Name = L"lbModules";
			this->lbModules->Size = System::Drawing::Size(266, 459);
			this->lbModules->TabIndex = 0;
			// 
			// mtBytecode
			// 
			this->mtBytecode->Controls->Add(this->tBytecode);
			this->mtBytecode->Location = System::Drawing::Point(4, 22);
			this->mtBytecode->Name = L"mtBytecode";
			this->mtBytecode->Size = System::Drawing::Size(694, 460);
			this->mtBytecode->TabIndex = 3;
			this->mtBytecode->Text = L"Байткод";
			this->mtBytecode->UseVisualStyleBackColor = true;
			// 
			// tBytecode
			// 
			this->tBytecode->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tBytecode->Font = (gcnew System::Drawing::Font(L"Courier New", 10, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->tBytecode->Location = System::Drawing::Point(0, 0);
			this->tBytecode->Name = L"tBytecode";
			this->tBytecode->ReadOnly = true;
			this->tBytecode->Size = System::Drawing::Size(694, 460);
			this->tBytecode->TabIndex = 0;
			this->tBytecode->Text = resources->GetString(L"tBytecode.Text");
			// 
			// mtHelp
			// 
			this->mtHelp->Controls->Add(this->tcHelp);
			this->mtHelp->Location = System::Drawing::Point(4, 22);
			this->mtHelp->Name = L"mtHelp";
			this->mtHelp->Size = System::Drawing::Size(694, 460);
			this->mtHelp->TabIndex = 2;
			this->mtHelp->Text = L"Справка";
			this->mtHelp->UseVisualStyleBackColor = true;
			// 
			// tcHelp
			// 
			this->tcHelp->Dock = System::Windows::Forms::DockStyle::Fill;
			this->tcHelp->Location = System::Drawing::Point(0, 0);
			this->tcHelp->Name = L"tcHelp";
			this->tcHelp->SelectedIndex = 0;
			this->tcHelp->Size = System::Drawing::Size(694, 460);
			this->tcHelp->TabIndex = 0;
			// 
			// cmbLog
			// 
			this->cmbLog->Dock = System::Windows::Forms::DockStyle::Bottom;
			this->cmbLog->DropDownHeight = 500;
			this->cmbLog->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
			this->cmbLog->FormattingEnabled = true;
			this->cmbLog->IntegralHeight = false;
			this->cmbLog->Location = System::Drawing::Point(0, 528);
			this->cmbLog->Name = L"cmbLog";
			this->cmbLog->Size = System::Drawing::Size(726, 21);
			this->cmbLog->TabIndex = 2;
			// 
			// bLoad
			// 
			this->bLoad->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Right));
			this->bLoad->Location = System::Drawing::Point(476, 500);
			this->bLoad->Name = L"bLoad";
			this->bLoad->Size = System::Drawing::Size(69, 23);
			this->bLoad->TabIndex = 3;
			this->bLoad->Text = L"Загрузить";
			this->bLoad->UseVisualStyleBackColor = true;
			this->bLoad->Click += gcnew System::EventHandler(this, &Form1::bLoad_Click);
			// 
			// bSaveAs
			// 
			this->bSaveAs->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Right));
			this->bSaveAs->Location = System::Drawing::Point(625, 500);
			this->bSaveAs->Name = L"bSaveAs";
			this->bSaveAs->Size = System::Drawing::Size(89, 23);
			this->bSaveAs->TabIndex = 4;
			this->bSaveAs->Text = L"Сохранить как";
			this->bSaveAs->UseVisualStyleBackColor = true;
			this->bSaveAs->Click += gcnew System::EventHandler(this, &Form1::bSaveAs_Click);
			// 
			// bCompile
			// 
			this->bCompile->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left));
			this->bCompile->Location = System::Drawing::Point(12, 500);
			this->bCompile->Name = L"bCompile";
			this->bCompile->Size = System::Drawing::Size(102, 23);
			this->bCompile->TabIndex = 5;
			this->bCompile->Text = L"Компилировать";
			this->bCompile->UseVisualStyleBackColor = true;
			this->bCompile->Click += gcnew System::EventHandler(this, &Form1::bCompile_Click);
			// 
			// bSave
			// 
			this->bSave->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Right));
			this->bSave->Location = System::Drawing::Point(551, 500);
			this->bSave->Name = L"bSave";
			this->bSave->Size = System::Drawing::Size(68, 23);
			this->bSave->TabIndex = 6;
			this->bSave->Text = L"Сохранить";
			this->bSave->UseVisualStyleBackColor = true;
			this->bSave->Click += gcnew System::EventHandler(this, &Form1::bSave_Click);
			// 
			// lLine
			// 
			this->lLine->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left));
			this->lLine->AutoSize = true;
			this->lLine->Location = System::Drawing::Point(191, 512);
			this->lLine->Name = L"lLine";
			this->lLine->Size = System::Drawing::Size(30, 13);
			this->lLine->TabIndex = 7;
			this->lLine->Text = L"Line:";
			// 
			// lCol
			// 
			this->lCol->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left));
			this->lCol->AutoSize = true;
			this->lCol->Location = System::Drawing::Point(191, 497);
			this->lCol->Name = L"lCol";
			this->lCol->Size = System::Drawing::Size(25, 13);
			this->lCol->TabIndex = 8;
			this->lCol->Text = L"Col:";
			// 
			// bClear
			// 
			this->bClear->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Right));
			this->bClear->Location = System::Drawing::Point(405, 500);
			this->bClear->Name = L"bClear";
			this->bClear->Size = System::Drawing::Size(65, 23);
			this->bClear->TabIndex = 9;
			this->bClear->Text = L"Убрать";
			this->bClear->UseVisualStyleBackColor = true;
			this->bClear->Click += gcnew System::EventHandler(this, &Form1::bClear_Click);
			// 
			// lAuthor
			// 
			this->lAuthor->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right));
			this->lAuthor->AutoSize = true;
			this->lAuthor->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 8.25F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->lAuthor->Location = System::Drawing::Point(536, 2);
			this->lAuthor->Name = L"lAuthor";
			this->lAuthor->Size = System::Drawing::Size(186, 13);
			this->lAuthor->TabIndex = 10;
			this->lAuthor->TabStop = true;
			this->lAuthor->Text = L"Author: Anton \'Cvet\' Tsvetinsky";
			this->lAuthor->LinkClicked += gcnew System::Windows::Forms::LinkLabelLinkClickedEventHandler(this, &Form1::lAuthor_LinkClicked);
			// 
			// lVersion
			// 
			this->lVersion->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right));
			this->lVersion->AutoSize = true;
			this->lVersion->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 8.25F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->lVersion->Location = System::Drawing::Point(427, 2);
			this->lVersion->Name = L"lVersion";
			this->lVersion->Size = System::Drawing::Size(97, 13);
			this->lVersion->TabIndex = 11;
			this->lVersion->Text = L"Version: XXXXX";
			// 
			// bNew
			// 
			this->bNew->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Right));
			this->bNew->Location = System::Drawing::Point(334, 500);
			this->bNew->Name = L"bNew";
			this->bNew->Size = System::Drawing::Size(65, 23);
			this->bNew->TabIndex = 12;
			this->bNew->Text = L"Создать";
			this->bNew->UseVisualStyleBackColor = true;
			this->bNew->Click += gcnew System::EventHandler(this, &Form1::btnNew_Click);
			// 
			// pb
			// 
			this->pb->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left) 
				| System::Windows::Forms::AnchorStyles::Right));
			this->pb->Location = System::Drawing::Point(186, 526);
			this->pb->Name = L"pb";
			this->pb->Size = System::Drawing::Size(326, 23);
			this->pb->TabIndex = 1;
			// 
			// bDebug
			// 
			this->bDebug->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left));
			this->bDebug->Location = System::Drawing::Point(120, 500);
			this->bDebug->Name = L"bDebug";
			this->bDebug->Size = System::Drawing::Size(65, 23);
			this->bDebug->TabIndex = 13;
			this->bDebug->Text = L"Отладка";
			this->bDebug->UseVisualStyleBackColor = true;
			this->bDebug->Click += gcnew System::EventHandler(this, &Form1::bDebug_Click);
			// 
			// Form1
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(726, 549);
			this->Controls->Add(this->bDebug);
			this->Controls->Add(this->pb);
			this->Controls->Add(this->bNew);
			this->Controls->Add(this->lVersion);
			this->Controls->Add(this->lAuthor);
			this->Controls->Add(this->bClear);
			this->Controls->Add(this->lCol);
			this->Controls->Add(this->lLine);
			this->Controls->Add(this->bSave);
			this->Controls->Add(this->bCompile);
			this->Controls->Add(this->bSaveAs);
			this->Controls->Add(this->bLoad);
			this->Controls->Add(this->cmbLog);
			this->Controls->Add(this->tabControl1);
			this->Icon = (cli::safe_cast<System::Drawing::Icon^  >(resources->GetObject(L"$this.Icon")));
			this->Name = L"Form1";
			this->Text = L"Редактор Скриптов FOnline";
			this->WindowState = System::Windows::Forms::FormWindowState::Maximized;
			this->FormClosing += gcnew System::Windows::Forms::FormClosingEventHandler(this, &Form1::Form1_FormClosing);
			this->Load += gcnew System::EventHandler(this, &Form1::Form1_Load);
			this->tabControl1->ResumeLayout(false);
			this->mtEdit->ResumeLayout(false);
			this->mtPrep->ResumeLayout(false);
			this->tModules->ResumeLayout(false);
			this->mtBytecode->ResumeLayout(false);
			this->mtHelp->ResumeLayout(false);
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion

const char* ToAnsi(String^ str)
{
	static char buf[4096];

//	pin_ptr<const wchar_t> wch=PtrToStringChars(str);
//	size_t origsize=wcslen(wch)+1;
//	size_t convertedChars=0;
//	wcstombs(buf,wch,sizeof(buf));

	return buf;
}

private: System::Void Form1_Load(System::Object^  sender, System::EventArgs^  e)
{
	setlocale( LC_ALL, "Russian" );

//ABOUT
	lVersion->Text="Version: "+String(VERSION_STR).ToString();
	lAuthor->Text="Author: Anton \'Cvet\' Tsvetinsky";
	lAuthor->Links->Add(8,lAuthor->Text->Length-8,\
		"mailto:cvet@tut.by?subject=ScriptEditor"+String(VERSION_STR).ToString());

//MAIN PATH
	path_dir=new char[1024];
	ZeroMemory(path_dir,1024);
	GetCurrentDirectoryA(300,path_dir);
	strcat(path_dir,"\\");

//TEXTS
	GetPrivateProfileStringA("EDITOR","ProgramName","Script Editor",ProgramName,256,PATH_INI);
	GetPrivateProfileStringA("EDITOR","ScriptInfo","AngelScript Scripts",ScriptInfo,256,PATH_INI);
	GetPrivateProfileStringA("EDITOR","ScriptExt","as",ScriptExt,256,PATH_INI);
	GetPrivateProfileStringA("EDITOR","DllName","dll",DllName,256,PATH_INI);
	GetPrivateProfileStringA("EDITOR","PathScripts","scripts\\",PathScripts,256,PATH_INI);
	GetPrivateProfileStringA("EDITOR","PathData","data\\",PathData,256,PATH_INI);
	GetPrivateProfileStringA("EDITOR","Language","ENGLISH",Language,256,PATH_INI);

	this->Text=String(ProgramName).ToString();

	char result[256];

	GetPrivateProfileStringA(Language,"tabEditor","Edit",result,256,PATH_INI);
	mtEdit->Text=String(result).ToString();
	GetPrivateProfileStringA(Language,"tabAfterPreprocess","After Preprocess",result,256,PATH_INI);
	mtPrep->Text=String(result).ToString();
	GetPrivateProfileStringA(Language,"tabBytecode","Bytecode",result,256,PATH_INI);
	mtBytecode->Text=String(result).ToString();
	GetPrivateProfileStringA(Language,"tabHelp","Help",result,256,PATH_INI);
	mtHelp->Text=String(result).ToString();

	GetPrivateProfileStringA(Language,"btnCompile","Compile",result,256,PATH_INI);
	bCompile->Text=String(result).ToString();
	GetPrivateProfileStringA(Language,"btnDebug","Debug",result,256,PATH_INI);
	bDebug->Text=String(result).ToString();
	GetPrivateProfileStringA(Language,"btnNew","New",result,256,PATH_INI);
	bNew->Text=String(result).ToString();
	GetPrivateProfileStringA(Language,"btnClear","Clear",result,256,PATH_INI);
	bClear->Text=String(result).ToString();
	GetPrivateProfileStringA(Language,"btnLoad","Load",result,256,PATH_INI);
	bLoad->Text=String(result).ToString();
	GetPrivateProfileStringA(Language,"btnSave","Save",result,256,PATH_INI);
	bSave->Text=String(result).ToString();
	GetPrivateProfileStringA(Language,"btnSaveAs","Save as",result,256,PATH_INI);
	bSaveAs->Text=String(result).ToString();

	GetPrivateProfileStringA(Language,"labLine","Line:",textLine,256,PATH_INI);
	GetPrivateProfileStringA(Language,"labCol","Col:",textCol,256,PATH_INI);

	GetPrivateProfileStringA(Language,"btnDbgExecute","Execute",result,256,PATH_INI);
	strBtnDbgExecute=String(result).ToString();
	GetPrivateProfileStringA(Language,"btnDbgClose","Close",result,256,PATH_INI);
	strBtnDbgClose=String(result).ToString();
	GetPrivateProfileStringA(Language,"labDbgFunc","Function:",result,256,PATH_INI);
	strLabDbgFunc=String(result).ToString();
	GetPrivateProfileStringA(Language,"labDbgParam","Parameter:",result,256,PATH_INI);
	strLabDbgParam=String(result).ToString();
	GetPrivateProfileStringA(Language,"labDbgVal","Value:",result,256,PATH_INI);
	strLabDbgVal=String(result).ToString();

//ANGEL SCRIPT
	scriptEngine=asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if(!scriptEngine)
	{
		Log("Error AngelScript Engine Init.");
		Close();
		return;
	}

	RegisterScriptString(scriptEngine);
	scriptEngine->SetMessageCallback(asFUNCTION(CallBack),0,asCALL_CDECL);

	char dll_dir[1024];
	sprintf(dll_dir,"%s%s%s",path_dir,PathData,DllName);

	HINSTANCE hDll;
	if((hDll=LoadLibraryA(dll_dir))==NULL)
	{
		Log("Dll Not Loaded.");
	}
	else
	{
		DLLFunc dllFunc;
		if((dllFunc=(DLLFunc)GetProcAddress(hDll,"Bind"))!=NULL)
		{
			int result=(*dllFunc)(scriptEngine);
			Log("Bindings result: "+Convert::ToString(result)+".");
		}
		else
			Log("Function \'Bind\' not found.");

		FreeLibrary(hDll);
	}

//LEXEMS
	lexemsOff=false;
	commentsOff=false;
	stringOff=false;

	char* tmp_str=new char[0x10000];
	DWORD style;
	DWORD color;
	DWORD size;

	char flexems[256];
	strcpy(flexems,PathData);
	strcat(flexems,"Lexems.txt");

	//Default
	style=GetPrivateProfileIntA("DEFAULT","FontStyle",1,flexems);
	color=GetPrivateProfileIntA("DEFAULT","FontColor",0,flexems);
	colorDefault=Color::FromArgb(color);
	GetPrivateProfileStringA("DEFAULT","FontName","Courier New",tmp_str,0x10000,flexems);
	size=GetPrivateProfileIntA("DEFAULT","FontSize",10,flexems);
	fontDefault=gcnew System::Drawing::Font(String(tmp_str).ToString(), size, (FontStyle)style);

	//Comments
	GetPrivateProfileStringA("COMMENTS","Char1","/",&tmp_str[0],2,flexems);
	GetPrivateProfileStringA("COMMENTS","Char2","/",&tmp_str[1],2,flexems);
	CommentChar0=tmp_str[0];
	CommentChar1=tmp_str[1];
	style=GetPrivateProfileIntA("COMMENTS","FontStyle",1,flexems);
	color=GetPrivateProfileIntA("COMMENTS","FontColor",0,flexems);
	colorComments=Color::FromArgb(color);
	GetPrivateProfileStringA("COMMENTS","FontName","Courier New",tmp_str,0x10000,flexems);
	size=GetPrivateProfileIntA("COMMENTS","FontSize",10,flexems);
	fontComments=gcnew System::Drawing::Font(String(tmp_str).ToString(), size, (FontStyle)style);

	//String
	GetPrivateProfileStringA("STRING","Char","\"",tmp_str,2,flexems);
	StringChar=tmp_str[0];
	style=GetPrivateProfileIntA("STRING","FontStyle",1,flexems);
	color=GetPrivateProfileIntA("STRING","FontColor",0,flexems);
	colorString=Color::FromArgb(color);
	GetPrivateProfileStringA("STRING","FontName","Courier New",tmp_str,0x10000,flexems);
	size=GetPrivateProfileIntA("STRING","FontSize",10,flexems);
	fontString=gcnew System::Drawing::Font(String(tmp_str).ToString(), size, (FontStyle)style);

	//Digit
	style=GetPrivateProfileIntA("DIGIT","FontStyle",1,flexems);
	color=GetPrivateProfileIntA("DIGIT","FontColor",0,flexems);
	colorDigit=Color::FromArgb(color);
	GetPrivateProfileStringA("DIGIT","FontName","Courier New",tmp_str,0x10000,flexems);
	size=GetPrivateProfileIntA("DIGIT","FontSize",10,flexems);
	fontDigit=gcnew System::Drawing::Font(String(tmp_str).ToString(), size, (FontStyle)style);

	//Lexems
	Lexems=gcnew cli::array<Words^>(0);
	int i;
	for(i=0;;++i)
	{
		char app_name[32];
		sprintf(app_name,"LEXEMS_%d",i);

		GetPrivateProfileStringA(app_name,"Words","",tmp_str,0x10000,flexems);
		if(!tmp_str[0]) break;

		Words^ wrds=gcnew Words;
		wrds->words=" "+String(tmp_str).ToString()+" ";

		style=GetPrivateProfileIntA(app_name,"FontStyle",1,flexems);
		color=GetPrivateProfileIntA(app_name,"FontColor",0,flexems);
		wrds->fontColor=Color::FromArgb(color);
		GetPrivateProfileStringA(app_name,"FontName","Courier New",tmp_str,0x10000,flexems);
		size=GetPrivateProfileIntA(app_name,"FontSize",10,flexems);
		wrds->fontStyle=gcnew System::Drawing::Font(String(tmp_str).ToString(), size, (FontStyle)style);

		Array::Resize(Lexems,i+1);
		Lexems[i]=wrds;
	}

	Array::Resize(Lexems,i);

	delete[] tmp_str;

//HELP
	RichTextBox^ rtb=gcnew RichTextBox;
	rtb->LoadFile(String(path_dir).ToString()+String(PathData).ToString()+"Help.txt",System::Windows::Forms::RichTextBoxStreamType::PlainText);
	String^ str=rtb->Text;

	int count=0;
	for(int i=0;i<str->Length;++i,++count)
	{
		if(str[i]=='*')
		{
			wchar_t str1[1024];
			wchar_t str2[1024];

			i++;
			int k;
			for(k=0;i<str->Length;++k,++i)
			{
				if(str[i]=='@' || str[i]=='\n') break;
				str1[k]=str[i];
			}
			str1[k]='\0';

			if(str[i]=='\n') continue;
			
			i++;
			for(k=0;i<str->Length;++k,++i)
			{
				if(str[i]=='\n') break;
				str2[k]=str[i];
			}
			str2[k]='\0';

			if(!str1[0] || !str2[0]) continue;

			System::Windows::Forms::RichTextBox^ richTextBox=(gcnew System::Windows::Forms::RichTextBox());
			richTextBox->Dock=System::Windows::Forms::DockStyle::Fill;
			richTextBox->Location=System::Drawing::Point(3, 3);
			richTextBox->Name="rtb"+Convert::ToString(count);
			richTextBox->Size=System::Drawing::Size(579, 340);
			richTextBox->TabIndex=0;
			richTextBox->ReadOnly=true;
			richTextBox->LoadFile(String(path_dir).ToString()+String(PathData).ToString()+String(str2).ToString(),System::Windows::Forms::RichTextBoxStreamType::RichText);

			System::Windows::Forms::TabPage^ tabPage=(gcnew System::Windows::Forms::TabPage());
			tabPage->SuspendLayout();
			tabPage->Controls->Add(richTextBox);
			tabPage->Location=System::Drawing::Point(4, 22);
			tabPage->Name="tb"+Convert::ToString(count);
			tabPage->Padding=System::Windows::Forms::Padding(3);
			tabPage->Size=System::Drawing::Size(585, 346);
			tabPage->TabIndex=0;
			tabPage->Text=String(str1).ToString();
			tabPage->UseVisualStyleBackColor=true;
			tabPage->ResumeLayout(false);

			tcHelp->Controls->Add(tabPage);
		}
		else if(str[i]==';')
		{
			while(i<str->Length && str[i]!='\n') i++;
		}
	}

	pb->Visible=false;

	EnableFormat=true;

	EditBoxes=gcnew array<EditBox^>(0);
	curNewBox=0;
	LastBoxTab=-1;

	CurBuf=0;
	Buffer=gcnew array<String^>(0);

	bSave->Enabled=false;

	tPrep->Clear();
	tBytecode->Clear();

	UpdateVisible();
}

void UpdateVisible()
{
	bNew->Enabled=false;
	bClear->Enabled=false;
	bLoad->Enabled=false;
	bSave->Enabled=false;
	bSaveAs->Enabled=false;
	bCompile->Enabled=false;
	bDebug->Enabled=false;
	this->Text=String(ProgramName).ToString();

	if(tabControl1->SelectedTab->Name=="mtEdit")
	{
		EditBox^ box=GetCurEditBox();
		if(!box)
		{
			bNew->Enabled=true;
			bLoad->Enabled=true;
			return;
		}

		this->Text=String(ProgramName).ToString()+"    "+box->filename;
		if(box->edited==true) this->Text+="*";
		if(box->compiled==false) this->Text+="^";

		bNew->Enabled=true;
		bClear->Enabled=true;
		bLoad->Enabled=true;
		if(box->edited==true) bSave->Enabled=true;
		bSaveAs->Enabled=true;
		bCompile->Enabled=true;
		if(box->compiled==true) bDebug->Enabled=true;
	}
	else if(tabControl1->SelectedTab->Name=="mtPrep")
	{
		if(tPrep->Text->Length>0) bSaveAs->Enabled=true;
	}
	else if(tabControl1->SelectedTab->Name=="mtBytecode")
	{
		if(tBytecode->Text->Length>0) bSaveAs->Enabled=true;
	}
	else if(tabControl1->SelectedTab->Name=="mtHelp")
	{
	}
}

EditBox^ CreateNewEditBox(String^ filename)
{
	EditBox^ box=gcnew EditBox();

	box->SetName(filename);

	RichTextBox^ richTextBox=(gcnew RichTextBox());
	richTextBox->Dock=System::Windows::Forms::DockStyle::Fill;
	richTextBox->Location=System::Drawing::Point(3, 3);
	richTextBox->Name="rtb"+Convert::ToString(curNewBox-1);
	richTextBox->Size=System::Drawing::Size(579, 340);
	richTextBox->TabIndex=0;
	richTextBox->WordWrap=false;
	richTextBox->ScrollBars=RichTextBoxScrollBars::Both;
	richTextBox->AcceptsTab=true;
	richTextBox->TextChanged += gcnew System::EventHandler(this, &Form1::tEdit_TextChanged);
	richTextBox->SelectionChanged += gcnew System::EventHandler(this, &Form1::tEdit_SelectionChanged);
	richTextBox->KeyDown += gcnew System::Windows::Forms::KeyEventHandler(this, &Form1::tEdit_KeyDown);
	richTextBox->KeyPress += gcnew System::Windows::Forms::KeyPressEventHandler(this, &Form1::tEdit_KeyPress);

	System::Windows::Forms::TabPage^ tabPage=(gcnew System::Windows::Forms::TabPage());
	tabPage->SuspendLayout();
	tabPage->Controls->Add(richTextBox);
	tabPage->Location=System::Drawing::Point(4, 22);
	tabPage->Name="tb"+Convert::ToString(curNewBox-1);
	tabPage->Padding=System::Windows::Forms::Padding(3);
	tabPage->Size=System::Drawing::Size(585, 346);
	tabPage->TabIndex=0;
	tabPage->Text=box->name;
	tabPage->UseVisualStyleBackColor=true;
	tabPage->ResumeLayout(false);

	tcEdit->Controls->Add(tabPage);
	
	box->RTB=richTextBox;
	box->edited=false;
	box->compiled=false;

	Array::Resize(EditBoxes,EditBoxes->Length+1);
	EditBoxes[EditBoxes->Length-1]=box;

	tcEdit->SelectTab("tb"+Convert::ToString(curNewBox-1));

	return box;
}

private: System::Void btnNew_Click(System::Object^  sender, System::EventArgs^  e)
{
	CreateNewEditBox(nullptr);
}

EditBox^ GetCurEditBox()
{
	if(tabControl1->SelectedIndex!=0) return nullptr;
	if(!tcEdit->TabCount) return nullptr;
	if(tcEdit->SelectedIndex==-1) return nullptr;

	return EditBoxes[tcEdit->SelectedIndex];
}

private: System::Void bCompile_Click(System::Object^  sender, System::EventArgs^  e)
{
	Compile(GetCurEditBox());
}

public: System::Void Compile(EditBox^ box)
{
	if(box==nullptr) return;

	RichTextBox^ rtb=box->RTB;
	if(!rtb->Text->Length) return;

	cmbLog->Items->Clear();
	Log("Compiling...");

	scriptEngine->ResetModule(TEMP_NAME);

/************************************************************************/
/* Preprocessor                                                         */
/************************************************************************/
	Preprocessor::VectorOutStream vos;
	Preprocessor::VectorOutStream vos_err;
	Preprocessor::FileSource fsrc;

	rtb->SaveFile(String(path_dir).ToString()+String(PathData).ToString()+TEMP_FULL_NAME,System::Windows::Forms::RichTextBoxStreamType::PlainText);

	UINT res=0;
	if(res=Preprocessor::preprocess(string(path_dir)+string(PathData)+string(TEMP_FULL_NAME),fsrc,vos,vos_err))
	{
		Log("Unable to preprocess. Errors="+res+". See in After preprocess.");
		File::Delete(String(path_dir).ToString()+String(PathData).ToString()+TEMP_FULL_NAME);
		vos_err.PushNull();
		tPrep->Text=String(vos_err.data()).ToString();
		return;
	}

	vos.Format();

	char* str_buf=new char[vos.size()+1];
	memcpy(str_buf,vos.data(),vos.size());
	str_buf[vos.size()]='\0';

	tPrep->Clear();
	tPrep->Text=String(str_buf).ToString();

	delete str_buf;
/************************************************************************/
/* Compiler                                                             */
/************************************************************************/
	__int64 freq;
	QueryPerformanceFrequency((PLARGE_INTEGER) &freq);
	__int64 fp;
	QueryPerformanceCounter((PLARGE_INTEGER)&fp);
	DWORD t1=GetTickCount();

	if(scriptEngine->AddScriptSection(TEMP_NAME,TEMP_NAME,vos.data(),vos.size())<0)
	{
		Log("Unable to add section.");
		File::Delete(String(path_dir).ToString()+String(PathData).ToString()+TEMP_FULL_NAME);
		return;
	}

	if(scriptEngine->Build(TEMP_NAME)<0)
	{
		Log("Unable to build.");
		File::Delete(String(path_dir).ToString()+String(PathData).ToString()+TEMP_FULL_NAME);
		return;
	}

	__int64 fp2;
	QueryPerformanceCounter((PLARGE_INTEGER)&fp2);
	DWORD t2=GetTickCount();

	box->SetTextcode(vos.data(),vos.size());
/************************************************************************/
/* Bytecode                                                             */
/************************************************************************/
	/*CBytecodeStream* bytecode=new CBytecodeStream;
	scriptEngine->SaveByteCode(TEMP_NAME,bytecode);

	String^ bc="             00 01 02 03    04 05 06 07    08 09 10 11    12 13 14 15\n";
	const BYTE* bc_buf=bytecode->GetBuffer();
	for(UINT i=0,j=bytecode->GetSize();i<j;i++)
	{
		if(!(i%16)) bc+=String::Format("\n{0:00000000}  ",i);
		if(!(i%4)) bc+="   ";

		bc+=String::Format("{0:X}{1:X} ",bc_buf[i]/16,bc_buf[i]%16);
	}
	tBytecode->Text=bc;

	box->SetBytecode(bytecode);*/
/************************************************************************/
/*                                                                      */
/************************************************************************/

	String^ str="";
	str+="Success. Time of AddScriptSection and Build: ";
	str+="QPC/QPF "+Double(((double)fp2-(double)fp)/(double)freq*1000).ToString()+" ms, ";
	str+="GTC "+(t2-t1)+" ms.";
	Log(str);

	File::Delete(String(path_dir).ToString()+String(PathData).ToString()+TEMP_FULL_NAME);

	box->compiled=true;
	UpdateVisible();
}

private: System::Void bLoad_Click(System::Object^  sender, System::EventArgs^  e)
{
/*	if(bSave->Enabled==true)
	{
		System::Windows::Forms::DialogResult mbres=MessageBox::Show("Save Changes?","Script Editor",
				MessageBoxButtons::YesNoCancel,
				MessageBoxIcon::Information);

		switch(mbres)
		{
		case System::Windows::Forms::DialogResult::Yes: Save(GetCurEditBox()); break;
		case System::Windows::Forms::DialogResult::Cancel: return;
		}
	}*/

	OpenFileDialog^ dlg=gcnew OpenFileDialog;

	dlg->InitialDirectory=String(path_dir).ToString()+String(PathScripts).ToString();
	dlg->Filter=String(ScriptInfo).ToString()+"|*."+String(ScriptExt).ToString();
	dlg->RestoreDirectory=true;

	if(dlg->ShowDialog()!=System::Windows::Forms::DialogResult::OK) return;

	EditBox^ tbox=CreateNewEditBox(dlg->FileName);
	if(tbox==nullptr) return;

	EnableFormat=false;
	tbox->RTB->LoadFile(dlg->FileName,System::Windows::Forms::RichTextBoxStreamType::PlainText);

	pb->Value=0;
	pb->Maximum=tbox->RTB->Lines->Length;
	pb->Visible=true;
	for(int i=0;i<tbox->RTB->Lines->Length;++i)
	{
		pb->Value++;
		FormatLines(tbox->RTB,i,i);
		tbox->RTB->Update();
	}
	pb->Visible=false;
	//FormatRange(tEdit,0,tEdit->Text->Length);

	EnableFormat=true;

	UpdateVisible();

//		cur_buf=0;
//		max_buf=0;

	Log("Load Successful.");
}
private: System::Void bSaveAs_Click(System::Object^  sender, System::EventArgs^  e)
{
	if(tabControl1->SelectedTab->Name=="mtPrep") SaveAs(tPrep);
	else if(tabControl1->SelectedTab->Name=="mtBytecode") SaveAs(tBytecode);
	else if(tabControl1->SelectedTab->Name=="mtEdit") SaveAs(GetCurEditBox());
}
private: System::Void bSave_Click(System::Object^  sender, System::EventArgs^  e)
{
	if(tabControl1->SelectedTab->Name=="mtEdit") Save(GetCurEditBox());
}
public: System::Void SaveAs(System::Object^ object)
{
	if(object==nullptr) return;

	EditBox^ box=dynamic_cast<EditBox^>(object);
	RichTextBox^ rtb;
	if(box!=nullptr) rtb=box->RTB;
	else rtb=dynamic_cast<RichTextBox^>(object);
	if(rtb==nullptr) return;

	SaveFileDialog^ dlg=gcnew SaveFileDialog;

	dlg->InitialDirectory=String(path_dir).ToString()+String(PathScripts).ToString();
	dlg->Filter=String(ScriptInfo).ToString()+"|*."+String(ScriptExt).ToString();
	dlg->RestoreDirectory=true;

	if(dlg->ShowDialog()!=System::Windows::Forms::DialogResult::OK) return;

	if(box)
	{
		box->SetName(dlg->FileName);
		tcEdit->SelectedTab->Text=box->name;
	}

	if(box) Save(box);
	else Save(rtb,dlg->FileName);
}

public: System::Void Save(EditBox^ box)
{
	if(box==nullptr) return;

	if(box->filename=="")
	{
		SaveAs(box);
		return;
	}

	box->RTB->SaveFile(box->filename,System::Windows::Forms::RichTextBoxStreamType::PlainText);

	box->edited=false;

	UpdateVisible();

	Log("Save Successful.");
}

public: System::Void Save(RichTextBox^ rtb, String^ filename)
{
	if(rtb==nullptr || filename==nullptr || filename->Length<=0) return;

	rtb->SaveFile(filename,System::Windows::Forms::RichTextBoxStreamType::PlainText);

	Log("Save Successful.");
}

private: System::Void tEdit_TextChanged(System::Object^  sender, System::EventArgs^  e)
{
	if(EnableFormat==false) return;

	RichTextBox^ rtb=dynamic_cast<RichTextBox^>(sender);
	if(rtb==nullptr) return;

	if(tcEdit->SelectedIndex!=-1)
	{
		if(EditBoxes[tcEdit->SelectedIndex]->edited==false || EditBoxes[tcEdit->SelectedIndex]->compiled==true)
		{
			EditBoxes[tcEdit->SelectedIndex]->edited=true;
			EditBoxes[tcEdit->SelectedIndex]->compiled=false;
			UpdateVisible();
		}
	}

	EnableFormat=false;
	lLine->Focus();

	int line1=rtb->GetLineFromCharIndex(rtb->SelectionStart);
	rtb->Undo();
	int line2=rtb->GetLineFromCharIndex(rtb->SelectionStart);
	rtb->Redo();
//	FormatLines(rtb,rtb->GetLineFromCharIndex(rtb->SelectionStart),rtb->GetLineFromCharIndex(rtb->SelectionStart));
	FormatLines(rtb,min(line1,line2),max(line1,line2));

	rtb->Focus();
	EnableFormat=true;

//	CurBuf++;
//	Array::Resize(Buffer,CurBuf);
//	Buffer[CurBuf-1]=rtb->Rtf;
}

public: System::Boolean IsDigit(wchar_t c)
{
	return c>='0' && c<='9';
}

public: System::Boolean IsLetter(wchar_t c)
{
	return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c=='_') || (c=='#');
}

public: System::Void FormatRange(RichTextBox^ rtb, int beg, int end)
{
	if(beg>=end) return;
	if(end>rtb->Text->Length) return;

	int pos_cur=rtb->SelectionStart;

	rtb->Select(beg,end-beg);
	rtb->SelectionFont=fontDefault;
	rtb->SelectionColor=colorDefault;

	wchar_t c;

	for(int i=beg;i<end;)
	{
		c=rtb->Text[i];

/************************************************************************/
/* Comment                                                              */
/************************************************************************/
		if(c==CommentChar0 && (!CommentChar1 || (i+1<rtb->Text->Length && rtb->Text[i+1]==CommentChar1)))
		{
			int j=i;
			do{i++;}
			while(i<rtb->Text->Length && rtb->Text[i]!='\n');

			rtb->Select(j,i-j);
			rtb->SelectionFont=fontComments;
			rtb->SelectionColor=colorComments;
		}
/************************************************************************/
/* String                                                               */
/************************************************************************/
		else if(c==StringChar)
		{
			int j=i;
			do{i++;}
			while(i<rtb->Text->Length && (rtb->Text[i]!=StringChar && rtb->Text[i]!='\n'));
			if(i<rtb->Text->Length && rtb->Text[i]==StringChar) i++;

			rtb->Select(j,i-j);
			rtb->SelectionFont=fontString;
			rtb->SelectionColor=colorString;
		}
/************************************************************************/
/* Lexem                                                                */
/************************************************************************/
		else if(IsLetter(c))
		{
			int j=i;
			do{i++;}
			while(i<rtb->Text->Length && (IsLetter(rtb->Text[i]) || IsDigit(rtb->Text[i])));

			rtb->Select(j,i-j);
			String^ lex=" "+rtb->SelectedText+" ";

			for(int k=0;k<Lexems->Length;++k)
			{
				if(Lexems[k]->words->Contains(lex))
				{
					rtb->SelectionFont=Lexems[k]->fontStyle;
					rtb->SelectionColor=Lexems[k]->fontColor;
					break;
				}
			}
		}
/************************************************************************/
/* Digit                                                                */
/************************************************************************/
		else if(IsDigit(c))
		{
			int j=i;
			do{i++;}
			while(i<rtb->Text->Length && IsDigit(rtb->Text[i]));

			rtb->Select(j,i-j);
			rtb->SelectionFont=fontDigit;
			rtb->SelectionColor=colorDigit;
		}
/************************************************************************/
/* Other                                                                */
/************************************************************************/
		else
		{
			i++;
		}
/************************************************************************/
/*                                                                      */
/************************************************************************/
	}

	rtb->SelectionLength=0;
	rtb->SelectionStart=pos_cur;
}

public: System::Void FormatLines(RichTextBox^ rtb, int beg, int end)
{
	if(!rtb->Text->Length) return;
	if(end>=rtb->Lines->Length) end=rtb->Lines->Length-1;
	if(beg>end) return;

	int pos_begin=rtb->GetFirstCharIndexFromLine(beg);
	int pos_end=rtb->GetFirstCharIndexFromLine(end);
	pos_end+=rtb->Lines[end]->Length;

	FormatRange(rtb,pos_begin,pos_end);
}

private: System::Void tEdit_MouseDown(System::Object^  sender, System::Windows::Forms::MouseEventArgs^  e){}
private: System::Void tEdit_PreviewKeyDown(System::Object^  sender, System::Windows::Forms::PreviewKeyDownEventArgs^  e){}
private: System::Void tEdit_SelectionChanged(System::Object^  sender, System::EventArgs^  e)
{
	RichTextBox^ rtb=dynamic_cast<RichTextBox^>(sender);
	if(rtb==nullptr) return;

	lLine->Text=String(textLine).ToString()+(rtb->GetLineFromCharIndex(rtb->SelectionStart)+1);
	lCol->Text=String(textCol).ToString()+(rtb->SelectionStart-rtb->GetFirstCharIndexOfCurrentLine()+1);
}
private: System::Void tPrep_SelectionChanged(System::Object^  sender, System::EventArgs^  e)
{
	lLine->Text=String(textLine).ToString()+(tPrep->GetLineFromCharIndex(tPrep->SelectionStart)+1);
	lCol->Text=String(textCol).ToString()+(tPrep->SelectionStart-tPrep->GetFirstCharIndexOfCurrentLine()+1);
}
private: System::Void bClear_Click(System::Object^  sender, System::EventArgs^  e)
{
	EditBox^ box=GetCurEditBox();
	if(box==nullptr) return;

	if(box->RTB->Text->Length>0)
	{
		if(MessageBox::Show("Are you sure?","Script Editor",
				MessageBoxButtons::YesNo,
				MessageBoxIcon::Information)==System::Windows::Forms::DialogResult::No) return;

		box->RTB->Clear();
		return;
	}

	array<EditBox^>^ boxes=gcnew cli::array<EditBox^>(EditBoxes->Length-1);

	if(tcEdit->SelectedIndex>0)
		Array::Copy(EditBoxes,0,boxes,0,tcEdit->SelectedIndex);

	if(tcEdit->SelectedIndex<tcEdit->TabCount-1)
		Array::Copy(EditBoxes,tcEdit->SelectedIndex+1,boxes,tcEdit->SelectedIndex,EditBoxes->Length-1-tcEdit->SelectedIndex);

	Array::Resize(EditBoxes,EditBoxes->Length-1);
	Array::Copy(boxes,EditBoxes,boxes->Length);

	tcEdit->Controls->Remove(tcEdit->SelectedTab);

	if(LastBoxTab!=-1 && LastBoxTab<tcEdit->TabCount) tcEdit->SelectTab(LastBoxTab);

	UpdateVisible();
}
private: System::Void tEdit_KeyPress(System::Object^  sender, System::Windows::Forms::KeyPressEventArgs^  e)
{
	RichTextBox^ rtb=dynamic_cast<RichTextBox^>(sender);
	if(rtb==nullptr) return;

	e->KeyChar=ParseChar(rtb,e->KeyChar);
}
private: System::Void tEdit_KeyDown(System::Object^  sender, System::Windows::Forms::KeyEventArgs^  e)
{
	RichTextBox^ rtb=dynamic_cast<RichTextBox^>(sender);
	if(rtb==nullptr) return;

	if(e->KeyCode==Keys::Enter)
	{
		//rtb->SelectionStart=rtb->GetFirstCharIndexFromLine(rtb->GetLineFromCharIndex(rtb->SelectionStart)-1);
		ParseChar(rtb,13);
		e->SuppressKeyPress=true;
	}

	if(e->KeyCode==Keys::F5) Compile(GetCurEditBox());
	if(e->KeyCode==Keys::F1) tabControl1->SelectTab(2);
}

wchar_t ParseChar(RichTextBox^ rtb, wchar_t ch)
{
	String^ cb_text;
	bool restore=false;
	if(Clipboard::ContainsText(TextDataFormat::Rtf))
	{
		cb_text=Clipboard::GetText(TextDataFormat::Rtf);
		restore=true;
	}

	switch(ch)
	{
/************************************************************************/
/* Enter                                                                */
/************************************************************************/
	case 13:
		{
			int t=CountBeginLine(rtb,rtb->GetLineFromCharIndex(rtb->SelectionStart),'\t',0);
			t+=CountBeginLine(rtb,rtb->GetLineFromCharIndex(rtb->SelectionStart),'{','\t');

			String^ str="\n";
			for(;t>0;t--) str+="\t";
			if(str->Length==1) str+="\n";
			Clipboard::SetText(str,TextDataFormat::Rtf);
			rtb->Paste();
		}
		break;
/************************************************************************/
/* {                                                                    */
/************************************************************************/
	case '{':
		{
			int t=CountBeginLine(rtb,rtb->GetLineFromCharIndex(rtb->SelectionStart)-1,'\t',0);
			t+=CountBeginLine(rtb,rtb->GetLineFromCharIndex(rtb->SelectionStart)-1,'{','\t');

			String^ str="{\n";
			for(int i=0;i<t;i++) str+="\t";
			str+="\n";
			for(int i=0;i<t;i++) str+="\t";
			str+="}";
			Clipboard::SetText(str,TextDataFormat::Rtf);
			rtb->Paste();
			rtb->SelectionStart-=t+2;
			ch='\t';
		}
		break;
/************************************************************************/
/* }                                                                    */
/************************************************************************/
	case '}':
		{
			int t=CountBeginLine(rtb,rtb->GetLineFromCharIndex(rtb->SelectionStart)-1,'\t',0);
			if(t<=0) break;

			rtb->SelectionStart--;
			rtb->SelectionLength=1;
			rtb->Cut();
		}
		break;
/************************************************************************/
/* {                                                                    */
/************************************************************************/
	case '[':
		{
			Clipboard::SetText("]",TextDataFormat::Rtf);
			rtb->Paste();
			rtb->SelectionStart--;
		}
		break;
/************************************************************************/
/* (                                                                    */
/************************************************************************/
	case '(':
		{
			Clipboard::SetText(")",TextDataFormat::Rtf);
			rtb->Paste();
			rtb->SelectionStart--;
		}
		break;
/************************************************************************/
/* "                                                                    */
/************************************************************************/
	case '\"':
		{
			Clipboard::SetText("\"",TextDataFormat::Rtf);
			rtb->Paste();
			rtb->SelectionStart--;
		}
		break;
/************************************************************************/
/*                                                                      */
/************************************************************************/
	}

	if(restore)
	{
		try{Clipboard::SetText(cb_text,TextDataFormat::Rtf);}catch(...){}
	}

	return ch;
}

int CountBeginLine(RichTextBox^ rtb, int line, wchar_t ch, wchar_t skip_ch)
{
	if(line<0 || rtb->Lines->Length<=0 || line>=rtb->Lines->Length) return 0;

	int res=0;
	for(int i=0;i<rtb->Lines[line]->Length;i++)
	{
		if(rtb->Lines[line][i]==skip_ch) continue;
		if(rtb->Lines[line][i]!=ch) break;
		res++;
	}

	return res;
}

private: System::Void Form1_FormClosing(System::Object^  sender, System::Windows::Forms::FormClosingEventArgs^  e)
{
	if(EditBoxes==nullptr) return;

	bool save_all=false;
	for(int i=0;i<EditBoxes->Length;i++)
		if(EditBoxes[i]->edited==true)
		{
			save_all=true;
			break;
		}

	if(save_all)
	{
		System::Windows::Forms::DialogResult mbres=MessageBox::Show("Save All Changes?","Script Editor",
			MessageBoxButtons::YesNoCancel,
			MessageBoxIcon::Information);

		switch(mbres)
		{
		case System::Windows::Forms::DialogResult::Yes:
			for(int i=0;i<EditBoxes->Length;i++)
			{
				EditBox^ box=EditBoxes[i];
				Save(box);
			}
			break;
		case System::Windows::Forms::DialogResult::Cancel:
			e->Cancel=true;
			break;
		}
	}
}
private: System::Void lAuthor_LinkClicked(System::Object^  sender, System::Windows::Forms::LinkLabelLinkClickedEventArgs^  e)
{
	String^ target=dynamic_cast<String^>(e->Link->LinkData);

	if(nullptr!=target)// && target->StartsWith("www"))
		System::Diagnostics::Process::Start(target);
}
private: System::Void tPrep_KeyDown(System::Object^  sender, System::Windows::Forms::KeyEventArgs^  e) {}
private: System::Void tabControl1_SelectedIndexChanged(System::Object^  sender, System::EventArgs^  e)
{
	UpdateVisible();
}
private: System::Void tcEdit_SelectedIndexChanged(System::Object^  sender, System::EventArgs^  e)
{
	UpdateVisible();
	LastBoxTab=tcEdit->SelectedIndex;
}
private: System::Void bDebug_Click(System::Object^  sender, System::EventArgs^  e)
{
	EditBox^ box=GetCurEditBox();
	if(!box)
	{
		Log("Edit box not find.");
		return;
	}

	if(box->compiled==false)
	{
		Log("First compile script.");
		return;
	}

	if(!box->textcode)
	{
		Log("Textcode nullptr.");
		return;
	}

	box->textcode->SetPos(0,0);
	if(scriptEngine->AddScriptSection(TEMP_NAME,TEMP_NAME,(char*)box->textcode->GetBuffer(),box->textcode->GetSize())<0)
	{
		Log("AddScriptSection error.");
		return;
	}

	if(scriptEngine->Build(TEMP_NAME)<0)
	{
		Log("Build error.");
		return;
	}

	formDebug^ f=gcnew formDebug(scriptEngine,TEMP_NAME);

	f->Text=bDebug->Text;
	f->bRun->Text=strBtnDbgExecute;
	f->bClose->Text=strBtnDbgClose;
	f->label1->Text=strLabDbgFunc;
	f->label2->Text=strLabDbgParam;
	f->label4->Text=strLabDbgVal;

	f->ShowDialog();
}
};



/************************************************************************/
/* Static functions                                                     */
/************************************************************************/

static void Log(String^ str)
{
	Form1::cmbLog->Items->Insert(0,str);
	Form1::cmbLog->Text=str;
}

static void CallBack(const asSMessageInfo *msg, void *param)
{
	const char *type = "ERR ";
	if( msg->type == asMSGTYPE_WARNING ) 
		type = "WARN";
	else if( msg->type == asMSGTYPE_INFORMATION ) 
		type = "INFO";

	Log(String(msg->section).ToString()+" ("+msg->row+", "+msg->col+") : "+String(type).ToString()+" : "+String(msg->message).ToString());
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

}