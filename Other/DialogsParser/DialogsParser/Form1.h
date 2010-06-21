#pragma once

namespace DialogsParser {

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

	char* path_dir;

	StrStrMap LangPacks;

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

		String^ DlgList;

		ListBox^ lbFindNames;

		RichTextBox^ PathFrom;
		RichTextBox^ PathToDlg;
	private: System::Windows::Forms::CheckBox^  cbIncludeOld;
	public: 
		RichTextBox^ PathToStr;

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
	private: System::Windows::Forms::ListBox^  lbFromDlg;
	private: System::Windows::Forms::ListBox^  lbToStr;
	private: System::Windows::Forms::Label^  label1;
	private: System::Windows::Forms::Label^  label2;
	private: System::Windows::Forms::Label^  label3;
	private: System::Windows::Forms::ListBox^  lbToDlg;
	private: System::Windows::Forms::ListBox^  lbFind;
	private: System::Windows::Forms::Label^  label4;
	private: System::Windows::Forms::Button^  btnFind;
	private: System::Windows::Forms::Button^  btnParse;
	private: System::Windows::Forms::Label^  label5;
	private: System::Windows::Forms::Label^  lConc;


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
			this->lbFromDlg = (gcnew System::Windows::Forms::ListBox());
			this->lbToStr = (gcnew System::Windows::Forms::ListBox());
			this->label1 = (gcnew System::Windows::Forms::Label());
			this->label2 = (gcnew System::Windows::Forms::Label());
			this->label3 = (gcnew System::Windows::Forms::Label());
			this->lbToDlg = (gcnew System::Windows::Forms::ListBox());
			this->lbFind = (gcnew System::Windows::Forms::ListBox());
			this->label4 = (gcnew System::Windows::Forms::Label());
			this->btnFind = (gcnew System::Windows::Forms::Button());
			this->btnParse = (gcnew System::Windows::Forms::Button());
			this->label5 = (gcnew System::Windows::Forms::Label());
			this->lConc = (gcnew System::Windows::Forms::Label());
			this->cbIncludeOld = (gcnew System::Windows::Forms::CheckBox());
			this->SuspendLayout();
			// 
			// lbFromDlg
			// 
			this->lbFromDlg->FormattingEnabled = true;
			this->lbFromDlg->Location = System::Drawing::Point(12, 47);
			this->lbFromDlg->Name = L"lbFromDlg";
			this->lbFromDlg->Size = System::Drawing::Size(361, 225);
			this->lbFromDlg->TabIndex = 0;
			// 
			// lbToStr
			// 
			this->lbToStr->FormattingEnabled = true;
			this->lbToStr->Location = System::Drawing::Point(12, 391);
			this->lbToStr->Name = L"lbToStr";
			this->lbToStr->Size = System::Drawing::Size(361, 95);
			this->lbToStr->TabIndex = 1;
			// 
			// label1
			// 
			this->label1->AutoSize = true;
			this->label1->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 20, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->label1->Location = System::Drawing::Point(65, 9);
			this->label1->Name = L"label1";
			this->label1->Size = System::Drawing::Size(85, 31);
			this->label1->TabIndex = 2;
			this->label1->Text = L"From:";
			// 
			// label2
			// 
			this->label2->AutoSize = true;
			this->label2->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 20, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->label2->Location = System::Drawing::Point(65, 357);
			this->label2->Name = L"label2";
			this->label2->Size = System::Drawing::Size(181, 31);
			this->label2->TabIndex = 3;
			this->label2->Text = L"Copy texts to:";
			// 
			// label3
			// 
			this->label3->AutoSize = true;
			this->label3->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 20, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->label3->Location = System::Drawing::Point(65, 277);
			this->label3->Name = L"label3";
			this->label3->Size = System::Drawing::Size(209, 31);
			this->label3->TabIndex = 4;
			this->label3->Text = L"Copy dialogs to:";
			// 
			// lbToDlg
			// 
			this->lbToDlg->FormattingEnabled = true;
			this->lbToDlg->Location = System::Drawing::Point(12, 311);
			this->lbToDlg->Name = L"lbToDlg";
			this->lbToDlg->Size = System::Drawing::Size(361, 43);
			this->lbToDlg->TabIndex = 5;
			// 
			// lbFind
			// 
			this->lbFind->FormattingEnabled = true;
			this->lbFind->Location = System::Drawing::Point(395, 47);
			this->lbFind->Name = L"lbFind";
			this->lbFind->Size = System::Drawing::Size(412, 589);
			this->lbFind->TabIndex = 6;
			// 
			// label4
			// 
			this->label4->AutoSize = true;
			this->label4->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 20, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->label4->Location = System::Drawing::Point(421, 9);
			this->label4->Name = L"label4";
			this->label4->Size = System::Drawing::Size(149, 31);
			this->label4->TabIndex = 7;
			this->label4->Text = L"Find result:";
			// 
			// btnFind
			// 
			this->btnFind->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 20, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->btnFind->Location = System::Drawing::Point(12, 506);
			this->btnFind->Name = L"btnFind";
			this->btnFind->Size = System::Drawing::Size(361, 40);
			this->btnFind->TabIndex = 8;
			this->btnFind->Text = L"Find";
			this->btnFind->UseVisualStyleBackColor = true;
			this->btnFind->Click += gcnew System::EventHandler(this, &Form1::btnFind_Click);
			// 
			// btnParse
			// 
			this->btnParse->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 20, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->btnParse->Location = System::Drawing::Point(12, 562);
			this->btnParse->Name = L"btnParse";
			this->btnParse->Size = System::Drawing::Size(361, 40);
			this->btnParse->TabIndex = 9;
			this->btnParse->Text = L"Parse";
			this->btnParse->UseVisualStyleBackColor = true;
			this->btnParse->Click += gcnew System::EventHandler(this, &Form1::btnParse_Click);
			// 
			// label5
			// 
			this->label5->AutoSize = true;
			this->label5->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 20, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->label5->Location = System::Drawing::Point(12, 605);
			this->label5->Name = L"label5";
			this->label5->Size = System::Drawing::Size(192, 31);
			this->label5->TabIndex = 10;
			this->label5->Text = L"Concurrences:";
			// 
			// lConc
			// 
			this->lConc->AutoSize = true;
			this->lConc->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 20, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->lConc->Location = System::Drawing::Point(210, 605);
			this->lConc->Name = L"lConc";
			this->lConc->Size = System::Drawing::Size(29, 31);
			this->lConc->TabIndex = 11;
			this->lConc->Text = L"0";
			// 
			// cbIncludeOld
			// 
			this->cbIncludeOld->AutoSize = true;
			this->cbIncludeOld->Location = System::Drawing::Point(194, 9);
			this->cbIncludeOld->Name = L"cbIncludeOld";
			this->cbIncludeOld->Size = System::Drawing::Size(111, 17);
			this->cbIncludeOld->TabIndex = 12;
			this->cbIncludeOld->Text = L"Include old strings";
			this->cbIncludeOld->UseVisualStyleBackColor = true;
			// 
			// Form1
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(820, 648);
			this->Controls->Add(this->cbIncludeOld);
			this->Controls->Add(this->lConc);
			this->Controls->Add(this->label5);
			this->Controls->Add(this->btnParse);
			this->Controls->Add(this->btnFind);
			this->Controls->Add(this->label4);
			this->Controls->Add(this->lbFind);
			this->Controls->Add(this->lbToDlg);
			this->Controls->Add(this->label3);
			this->Controls->Add(this->label2);
			this->Controls->Add(this->label1);
			this->Controls->Add(this->lbToStr);
			this->Controls->Add(this->lbFromDlg);
			this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedSingle;
			this->Icon = (cli::safe_cast<System::Drawing::Icon^  >(resources->GetObject(L"$this.Icon")));
			this->MaximizeBox = false;
			this->Name = L"Form1";
			this->Text = L"FOnline Dialogs Parser v1.2";
			this->Load += gcnew System::EventHandler(this, &Form1::Form1_Load);
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion
private: System::Void Form1_Load(System::Object^  sender, System::EventArgs^  e)
{
	setlocale( LC_ALL, "Russian" );

	//MAIN PATH
	path_dir=new char[1024];
	ZeroMemory(path_dir,1024);
	GetCurrentDirectoryA(300,path_dir);
	strcat(path_dir,"\\");

	lbFindNames=gcnew ListBox;

	PathFrom=gcnew RichTextBox;
	PathToDlg=gcnew RichTextBox;
	PathToStr=gcnew RichTextBox;

	PathFrom->LoadFile(String(path_dir).ToString()+String(DATA_PATH).ToString()+"from_dlg.txt",System::Windows::Forms::RichTextBoxStreamType::PlainText);
	PathToDlg->LoadFile(String(path_dir).ToString()+String(DATA_PATH).ToString()+"to_dlg.txt",System::Windows::Forms::RichTextBoxStreamType::PlainText);
	PathToStr->LoadFile(String(path_dir).ToString()+String(DATA_PATH).ToString()+"to_str.txt",System::Windows::Forms::RichTextBoxStreamType::PlainText);

	lbFromDlg->Items->AddRange(PathFrom->Lines);
	lbToDlg->Items->AddRange(PathToDlg->Lines);
	lbToStr->Items->AddRange(PathToStr->Lines);

	btnParse->Enabled=false;

	lConc->Text="none";
}
private: System::Void btnFind_Click(System::Object^  sender, System::EventArgs^  e)
{
	btnFind->Enabled=false;
	btnParse->Enabled=false;
	lbFind->Items->Clear();
	lbFindNames->Items->Clear();
	LangPacks.clear(); //leak
	DlgList="";
	lConc->Text="none";

	for(int i=0;i<lbFromDlg->Items->Count;++i)
	{
		DirectoryInfo^ dir=gcnew DirectoryInfo(PathFrom->Lines[i]);
		if(!dir->Exists) continue;
		if(File::Exists(PathFrom->Lines[i]+LIST_NAME)) DlgList=PathFrom->Lines[i]+LIST_NAME;
		array<FileInfo^>^fi = dir->GetFiles("*.dlg",SearchOption::TopDirectoryOnly);

		Collections::IEnumerator^ myEnum = fi->GetEnumerator();
		while(myEnum->MoveNext())
		{
			FileInfo^ fiTemp = safe_cast<FileInfo^>(myEnum->Current);

			if(fiTemp->Extension!=".dlg")
			{
				StrStrMap::iterator it=LangPacks.find(ToAnsi(fiTemp->Extension));
				if(it==LangPacks.end()) it=(LangPacks.insert(StrStrMap::value_type(string(ToAnsi(fiTemp->Extension)),new StrMap))).first;

				StrMap* str=(*it).second;

				FILE* f=NULL;
				if(!(f=fopen(ToAnsi(fiTemp->FullName),"rb"))) continue;

				fseek(f,0,SEEK_END);
				UINT buf_len=ftell(f)+1;
				fseek(f,0,SEEK_SET);

				char* buf=new char[buf_len];

				fread(buf,sizeof(char),buf_len,f);
				fclose(f);

				DWORD num_info=0;
				char info[4096];

				UINT buf_cur=0;

				while(buf_cur<buf_len)
				{
					if(buf[buf_cur++]!='{') continue;

					if(sscanf(&buf[buf_cur],"%d}{}{%[^}]}",&num_info,info)==2 && num_info!=0)
					{
						info[4096-1]='\0';
						if(str->count(num_info))
						{
							lConc->Text=fiTemp->Name+", number "+num_info;
							return;
						}

						str->insert(StrMap::value_type(num_info,string(info)));
					}

					while(buf_cur<buf_len)
						if(buf[buf_cur++]=='\n') break;
				}

				delete[] buf;
				continue;
			}

			lbFind->Items->Add(fiTemp->FullName);
			lbFindNames->Items->Add(fiTemp->Name);
		}
	}

	if(lbFind->Items->Count>0) btnParse->Enabled=true;
	btnFind->Enabled=true;
}
private: System::Void btnParse_Click(System::Object^  sender, System::EventArgs^  e)
{
	btnFind->Enabled=false;
	btnParse->Enabled=false;
	
	for(int j=0;j<lbToDlg->Items->Count;++j)
	{
		for(int i=0;i<lbFind->Items->Count;++i)
			File::Copy(lbFind->Items[i]->ToString(),lbToDlg->Items[j]->ToString()+lbFindNames->Items[i]->ToString(),true);

		if(DlgList!="")
			File::Copy(DlgList,lbToDlg->Items[j]->ToString()+LIST_NAME,true);
	}

	String^ ext="none";
	for(int i=0;i<lbToStr->Items->Count;++i)
	{
		if(lbToStr->Items[i]->ToString()[0]=='@')
		{
			ext=lbToStr->Items[i]->ToString();
			ext=ext->Remove(0,1);
			continue;
		}

		if(ext=="none") continue;

		String^ path=lbToStr->Items[i]->ToString()+MSG_NAME;

		if(cbIncludeOld->Checked)
		{
			//
		}

		File::Delete(path);

		StrStrMap::iterator it=LangPacks.find(string(ToAnsi(ext)));
		if(it==LangPacks.end()) continue;

		StrMap* str=(*it).second;

		FILE* f=NULL;
		if(!(f=fopen(ToAnsi(path),"wb"))) continue;

		string buf;
		char info[4096];
		for(StrMap::iterator it_t=str->begin();it_t!=str->end();++it_t)
		{
			sprintf(info,"{%d}{}{%s}\n",(*it_t).first,(*it_t).second.c_str());
			buf+=string(info);
		}

		fwrite(buf.c_str(),sizeof(char),buf.size(),f);
		fclose(f);
	}

	btnFind->Enabled=true;
	btnParse->Enabled=true;
}
};
}

