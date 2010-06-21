#pragma once

namespace ScriptEditor {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	static void DebLog(String^ str);
	static vector<ParamInfo> ScriptArgs;

	/// <summary>
	/// Summary for formDebug
	///
	/// WARNING: If you change the name of this class, you will need to change the
	///          'Resource File Name' property for the managed resource compiler tool
	///          associated with all .resx files this class depends on.  Otherwise,
	///          the designers will not be able to interact properly with localized
	///          resources associated with this form.
	/// </summary>
	public ref class formDebug : public System::Windows::Forms::Form
	{
	public:
		asIScriptEngine* scriptEngine;
	public:static System::Windows::Forms::RichTextBox^  rtbLog;


	public: System::Windows::Forms::Button^  bRun;



	public: System::Windows::Forms::Label^  label4;
	private: 

	private: System::Windows::Forms::TextBox^  tParamValue;
	private: System::Windows::Forms::ListBox^  lbParams;
	private: System::Windows::Forms::Panel^  panel1;
	private: System::Windows::Forms::Panel^  panel2;
	public:

		char* moduleName;

	public:
		formDebug(asIScriptEngine* script_engine, const char* module_name)
		{
			InitializeComponent();

			scriptEngine=script_engine;
			moduleName=new char[strlen(module_name)+1];
			strcpy(moduleName,module_name);
			rtbLog->Clear();
		}

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~formDebug()
		{
			if (components)
			{
				delete components;
			}
		}
	public: System::Windows::Forms::Label^  label1;
	protected: 

	private: System::Windows::Forms::ComboBox^  cbFuncNames;
	public: System::Windows::Forms::Label^  label2;
	public: System::Windows::Forms::Button^  bClose;
	private: 




	protected: 

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
			System::ComponentModel::ComponentResourceManager^  resources = (gcnew System::ComponentModel::ComponentResourceManager(formDebug::typeid));
			this->label1 = (gcnew System::Windows::Forms::Label());
			this->cbFuncNames = (gcnew System::Windows::Forms::ComboBox());
			this->label2 = (gcnew System::Windows::Forms::Label());
			this->bClose = (gcnew System::Windows::Forms::Button());
			this->rtbLog = (gcnew System::Windows::Forms::RichTextBox());
			this->bRun = (gcnew System::Windows::Forms::Button());
			this->label4 = (gcnew System::Windows::Forms::Label());
			this->tParamValue = (gcnew System::Windows::Forms::TextBox());
			this->lbParams = (gcnew System::Windows::Forms::ListBox());
			this->panel1 = (gcnew System::Windows::Forms::Panel());
			this->panel2 = (gcnew System::Windows::Forms::Panel());
			this->panel1->SuspendLayout();
			this->panel2->SuspendLayout();
			this->SuspendLayout();
			// 
			// label1
			// 
			this->label1->AutoSize = true;
			this->label1->Location = System::Drawing::Point(9, 2);
			this->label1->Name = L"label1";
			this->label1->Size = System::Drawing::Size(56, 13);
			this->label1->TabIndex = 0;
			this->label1->Text = L"Функция:";
			// 
			// cbFuncNames
			// 
			this->cbFuncNames->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
			this->cbFuncNames->FormattingEnabled = true;
			this->cbFuncNames->Location = System::Drawing::Point(3, 18);
			this->cbFuncNames->Name = L"cbFuncNames";
			this->cbFuncNames->Size = System::Drawing::Size(230, 21);
			this->cbFuncNames->TabIndex = 1;
			this->cbFuncNames->SelectedIndexChanged += gcnew System::EventHandler(this, &formDebug::cbFuncNames_SelectedIndexChanged);
			// 
			// label2
			// 
			this->label2->AutoSize = true;
			this->label2->Location = System::Drawing::Point(9, 42);
			this->label2->Name = L"label2";
			this->label2->Size = System::Drawing::Size(61, 13);
			this->label2->TabIndex = 2;
			this->label2->Text = L"Параметр:";
			// 
			// bClose
			// 
			this->bClose->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left));
			this->bClose->Location = System::Drawing::Point(3, 273);
			this->bClose->Name = L"bClose";
			this->bClose->Size = System::Drawing::Size(230, 27);
			this->bClose->TabIndex = 4;
			this->bClose->Text = L"Close";
			this->bClose->UseVisualStyleBackColor = true;
			this->bClose->Click += gcnew System::EventHandler(this, &formDebug::bClose_Click);
			// 
			// rtbLog
			// 
			this->rtbLog->Dock = System::Windows::Forms::DockStyle::Fill;
			this->rtbLog->Font = (gcnew System::Drawing::Font(L"Courier New", 8.25F, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->rtbLog->ForeColor = System::Drawing::SystemColors::WindowText;
			this->rtbLog->Location = System::Drawing::Point(0, 0);
			this->rtbLog->Name = L"rtbLog";
			this->rtbLog->Size = System::Drawing::Size(528, 309);
			this->rtbLog->TabIndex = 5;
			this->rtbLog->Text = resources->GetString(L"rtbLog.Text");
			this->rtbLog->WordWrap = false;
			// 
			// bRun
			// 
			this->bRun->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left));
			this->bRun->Location = System::Drawing::Point(3, 240);
			this->bRun->Name = L"bRun";
			this->bRun->Size = System::Drawing::Size(230, 27);
			this->bRun->TabIndex = 6;
			this->bRun->Text = L"Run";
			this->bRun->UseVisualStyleBackColor = true;
			this->bRun->Click += gcnew System::EventHandler(this, &formDebug::bRun_Click);
			// 
			// label4
			// 
			this->label4->AutoSize = true;
			this->label4->Location = System::Drawing::Point(9, 158);
			this->label4->Name = L"label4";
			this->label4->Size = System::Drawing::Size(58, 13);
			this->label4->TabIndex = 8;
			this->label4->Text = L"Значение:";
			// 
			// tParamValue
			// 
			this->tParamValue->Location = System::Drawing::Point(3, 174);
			this->tParamValue->Multiline = true;
			this->tParamValue->Name = L"tParamValue";
			this->tParamValue->Size = System::Drawing::Size(230, 60);
			this->tParamValue->TabIndex = 10;
			this->tParamValue->TextChanged += gcnew System::EventHandler(this, &formDebug::tParamValue_TextChanged);
			// 
			// lbParams
			// 
			this->lbParams->FormattingEnabled = true;
			this->lbParams->Location = System::Drawing::Point(3, 58);
			this->lbParams->Name = L"lbParams";
			this->lbParams->Size = System::Drawing::Size(230, 95);
			this->lbParams->TabIndex = 11;
			this->lbParams->SelectedIndexChanged += gcnew System::EventHandler(this, &formDebug::lbParams_SelectedIndexChanged);
			// 
			// panel1
			// 
			this->panel1->Controls->Add(this->lbParams);
			this->panel1->Controls->Add(this->label1);
			this->panel1->Controls->Add(this->tParamValue);
			this->panel1->Controls->Add(this->cbFuncNames);
			this->panel1->Controls->Add(this->label4);
			this->panel1->Controls->Add(this->label2);
			this->panel1->Controls->Add(this->bRun);
			this->panel1->Controls->Add(this->bClose);
			this->panel1->Dock = System::Windows::Forms::DockStyle::Left;
			this->panel1->Location = System::Drawing::Point(0, 0);
			this->panel1->Name = L"panel1";
			this->panel1->Size = System::Drawing::Size(239, 309);
			this->panel1->TabIndex = 12;
			// 
			// panel2
			// 
			this->panel2->Controls->Add(this->rtbLog);
			this->panel2->Dock = System::Windows::Forms::DockStyle::Fill;
			this->panel2->Location = System::Drawing::Point(239, 0);
			this->panel2->Name = L"panel2";
			this->panel2->Size = System::Drawing::Size(528, 309);
			this->panel2->TabIndex = 13;
			// 
			// formDebug
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(767, 309);
			this->Controls->Add(this->panel2);
			this->Controls->Add(this->panel1);
			this->Icon = (cli::safe_cast<System::Drawing::Icon^  >(resources->GetObject(L"$this.Icon")));
			this->Name = L"formDebug";
			this->Text = L"formDebug";
			this->Load += gcnew System::EventHandler(this, &formDebug::formDebug_Load);
			this->panel1->ResumeLayout(false);
			this->panel1->PerformLayout();
			this->panel2->ResumeLayout(false);
			this->ResumeLayout(false);

		}
#pragma endregion

private: System::Void formDebug_Load(System::Object^  sender, System::EventArgs^  e)
{
	for(int i=0,j=scriptEngine->GetFunctionCount(moduleName);i<j;i++)
	{
		int func_id=scriptEngine->GetFunctionIDByIndex(moduleName,i);
		if(func_id<0)
		{
			DebLog("Get function error.");
			cbFuncNames->Items->Add("< debug error >");
			continue;
		}
		cbFuncNames->Items->Add(String(scriptEngine->GetFunctionName(func_id)).ToString());
	}
}

private: System::Void bClose_Click(System::Object^  sender, System::EventArgs^  e)
{
	Close();
}

private: System::Void cbFuncNames_SelectedIndexChanged(System::Object^  sender, System::EventArgs^  e)
{
	ScriptArgs.clear();
	lbParams->Items->Clear();

	if(cbFuncNames->SelectedIndex==-1) return;

	int func_id=scriptEngine->GetFunctionIDByIndex(moduleName,cbFuncNames->SelectedIndex);
	if(func_id<0)
	{
		DebLog("Function not find.");
		return;
	}

	asIScriptContext* ctx=scriptEngine->CreateContext();
	if(!ctx)
	{
		DebLog("Create context fail.");
		return;
	}

	if(ctx->Prepare(func_id)<0)
	{
		DebLog("Function prepare fail.");
		return;
	}

	asCContext* _ctx=dynamic_cast<asCContext*>(ctx);
	if(!_ctx)
	{
		DebLog("Invalid cast.");
		return;
	}

	for(int i=0,j=_ctx->currentFunction->parameterTypes.GetLength();i<j;i++)
	{
		lbParams->Items->Add(String(ctx->GetVarDeclaration(i)).ToString());
		ScriptArgs.push_back(ParamInfo(&_ctx->currentFunction->parameterTypes[i]));
	}

	ctx->Release();
}

private: System::Void lbParams_SelectedIndexChanged(System::Object^  sender, System::EventArgs^  e)
{
	tParamValue->Clear();

	if(lbParams->SelectedIndex<0 || lbParams->SelectedIndex>=ScriptArgs.size()) return;

	ParamInfo* p=&ScriptArgs[lbParams->SelectedIndex];

	if(p->type->IsIntegerType())
	{
		if(p->type->GetSizeInMemoryBytes()==8) tParamValue->Text=Int64(p->valI).ToString();
		else if(p->type->GetSizeInMemoryBytes()==4) tParamValue->Text=Int32(p->valI).ToString();
		else if(p->type->GetSizeInMemoryBytes()==2) tParamValue->Text=Int16(p->valI).ToString();
		else if(p->type->GetSizeInMemoryBytes()==1) tParamValue->Text=Int16(p->valI).ToString();
	}
	else if(p->type->IsUnsignedType())
	{
		if(p->type->GetSizeInMemoryBytes()==8) tParamValue->Text=UInt64(p->valI).ToString();
		else if(p->type->GetSizeInMemoryBytes()==4) tParamValue->Text=UInt32(p->valI).ToString();
		else if(p->type->GetSizeInMemoryBytes()==2) tParamValue->Text=UInt16(p->valI).ToString();
		else if(p->type->GetSizeInMemoryBytes()==1) tParamValue->Text=UInt16(p->valI).ToString();
	}
	else if(p->type->IsFloatType())
	{
		tParamValue->Text=Double(p->valD).ToString();
	}
	else if(p->type->IsDoubleType())
	{
		tParamValue->Text=Double(p->valD).ToString();
	}
	else if(p->type->IsBooleanType())
	{
		tParamValue->Text=p->valI!=0?"1":"0";
	}
}

private: System::Void tParamValue_TextChanged(System::Object^  sender, System::EventArgs^  e)
{
	if(lbParams->SelectedIndex<0 || lbParams->SelectedIndex>=ScriptArgs.size()) return;
	if(tParamValue->Text->Length<=0) return;

	ParamInfo* p=&ScriptArgs[lbParams->SelectedIndex];

	try
	{
		if(p->type->IsIntegerType())
		{
			if(p->type->GetSizeInMemoryBytes()==8) p->valI=Convert::ToInt64(tParamValue->Text);
			else if(p->type->GetSizeInMemoryBytes()==4) p->valI=Convert::ToInt32(tParamValue->Text);
			else if(p->type->GetSizeInMemoryBytes()==2) p->valI=Convert::ToInt16(tParamValue->Text);
			else if(p->type->GetSizeInMemoryBytes()==1) p->valI=Convert::ToInt16(tParamValue->Text);
		}
		else if(p->type->IsUnsignedType())
		{
			if(p->type->GetSizeInMemoryBytes()==8) p->valI=Convert::ToUInt64(tParamValue->Text);
			else if(p->type->GetSizeInMemoryBytes()==4) p->valI=Convert::ToUInt32(tParamValue->Text);
			else if(p->type->GetSizeInMemoryBytes()==2) p->valI=Convert::ToUInt16(tParamValue->Text);
			else if(p->type->GetSizeInMemoryBytes()==1) p->valI=Convert::ToUInt16(tParamValue->Text);
		}
		else if(p->type->IsFloatType())
		{
			p->valD=Convert::ToDouble(tParamValue->Text);
		}
		else if(p->type->IsDoubleType())
		{
			p->valD=Convert::ToDouble(tParamValue->Text);
		}
		else if(p->type->IsBooleanType())
		{
			p->valI=Convert::ToInt32(tParamValue->Text)!=0?1:0;
		}
	}
	catch (...)
	{
		DebLog("Bad conversion.");
	}
}

private: System::Void bRun_Click(System::Object^  sender, System::EventArgs^  e)
{
//PREPARE
	if(cbFuncNames->SelectedIndex==-1)
	{
		DebLog("Choose function");
		goto label_End;
	}

	int func_id=scriptEngine->GetFunctionIDByIndex(moduleName,cbFuncNames->SelectedIndex);
	if(func_id<0)
	{
		DebLog("Function not find.");
		goto label_End;
	}

	asIScriptContext* ctx=scriptEngine->CreateContext();
	if(!ctx)
	{
		DebLog("Create context fail.");
		goto label_End;
	}

	asCContext* _ctx=dynamic_cast<asCContext*>(ctx);
	asCScriptEngine* _engine=dynamic_cast<asCScriptEngine*>(scriptEngine);
	if(!_engine || !_ctx)
	{
		DebLog("Bad cast.");
		return;
	}

	if(ctx->Prepare(func_id)<0)
	{
		DebLog("Prepare context fail.");
		goto label_End;
	}

//SER ARGS
	for(int i=0,j=ScriptArgs.size();i<j;i++)
	{
		ParamInfo* p=&ScriptArgs[i];

		if(p->type->IsIntegerType())
		{
			if(p->type->GetSizeInMemoryBytes()==8) ctx->SetArgQWord(i,(__int64)p->valI);
			else if(p->type->GetSizeInMemoryBytes()==4) ctx->SetArgDWord(i,(int)p->valI);
			else if(p->type->GetSizeInMemoryBytes()==2) ctx->SetArgWord(i,(short)p->valI);
			else if(p->type->GetSizeInMemoryBytes()==1) ctx->SetArgByte(i,(char)p->valI);
		}
		else if(p->type->IsUnsignedType())
		{
			if(p->type->GetSizeInMemoryBytes()==8) ctx->SetArgQWord(i,(unsigned __int64)p->valI);
			else if(p->type->GetSizeInMemoryBytes()==4) ctx->SetArgDWord(i,(unsigned int)p->valI);
			else if(p->type->GetSizeInMemoryBytes()==2) ctx->SetArgWord(i,(unsigned short)p->valI);
			else if(p->type->GetSizeInMemoryBytes()==1) ctx->SetArgByte(i,(unsigned char)p->valI);
		}
		else if(p->type->IsFloatType())
		{
			ctx->SetArgFloat(i,(float)p->valD);
		}
		else if(p->type->IsDoubleType())
		{
			ctx->SetArgDouble(i,(double)p->valD);
		}
		else if(p->type->IsBooleanType())
		{
			ctx->SetArgByte(i,(bool)p->valI);
		}
	}

//To string func name and args
	String^ args="";
	for(int i=0,j=ScriptArgs.size();i<j;i++)
	{
		ParamInfo* p=&ScriptArgs[i];

		args+=String(ctx->GetVarDeclaration(i)).ToString()+" = ";

		if(p->type->IsIntegerType()) args+=Int64(p->valI).ToString();
		else if(p->type->IsUnsignedType()) args+=UInt64(p->valI).ToString();
		else if(p->type->IsFloatType() || p->type->IsDoubleType()) args+=Double(p->valD).ToString();
		else if(p->type->IsBooleanType()) args+=Boolean(p->valI).ToString();

		if(i<j-1) args+=", ";
	}

	DebLog("Execute function "+String(_ctx->currentFunction->name.AddressOf()).ToString()+"("+args+")");

//EXECUTE
	rtbLog->Update();

	__int64 freq;
	QueryPerformanceFrequency((PLARGE_INTEGER) &freq);
	__int64 fp;
	QueryPerformanceCounter((PLARGE_INTEGER)&fp);
	DWORD t1=GetTickCount();

	int res=ctx->Execute();

	switch(res)
	{
/************************************************************************/
/* Finished                                                             */
/************************************************************************/
	case asEXECUTION_FINISHED:
		{
		//Time
			__int64 fp2;
			QueryPerformanceCounter((PLARGE_INTEGER)&fp2);
			DWORD t2=GetTickCount();

			String^ str="";
			str+="Execution success, time: ";
			str+="QPC/QPF "+Double(((double)fp2-(double)fp)/(double)freq*1000).ToString()+" ms, ";
			str+="GTC "+(t2-t1)+" ms.";
			DebLog(str);

		//Reurned value
			if(_ctx->currentFunction->returnType.GetSizeInMemoryBytes()!=0)
			{
				str="Returned value = ";
				asCDataType* dt=&_ctx->currentFunction->returnType;
				if(dt->IsIntegerType())
				{
					if(dt->GetSizeInMemoryBytes()==8) str+=Int64(ctx->GetReturnQWord()).ToString();
					else if(dt->GetSizeInMemoryBytes()==4) str+=Int32(ctx->GetReturnDWord()).ToString();
					else if(dt->GetSizeInMemoryBytes()==2) str+=Int16(ctx->GetReturnWord()).ToString();
					else if(dt->GetSizeInMemoryBytes()==1) str+=Int16(ctx->GetReturnByte()).ToString();
				}
				else if(dt->IsUnsignedType())
				{
					if(dt->GetSizeInMemoryBytes()==8) str+=UInt64(ctx->GetReturnQWord()).ToString();
					else if(dt->GetSizeInMemoryBytes()==4) str+=UInt32(ctx->GetReturnDWord()).ToString();
					else if(dt->GetSizeInMemoryBytes()==2) str+=UInt16(ctx->GetReturnWord()).ToString();
					else if(dt->GetSizeInMemoryBytes()==1) str+=UInt16(ctx->GetReturnByte()).ToString();
				}
				else if(dt->IsFloatType())
				{
					str+=Double(ctx->GetReturnFloat()).ToString();
				}
				else if(dt->IsDoubleType())
				{
					str+=Double(ctx->GetReturnDouble()).ToString();
				}
				else if(dt->IsBooleanType())
				{
					str+=ctx->GetReturnByte()!=0?"True":"False";
				}

				str+=".";
				DebLog(str);
			}
		}
		break;
/************************************************************************/
/* Suspended                                                            */
/************************************************************************/
	case asEXECUTION_SUSPENDED:
		{
			DebLog("Execution suspended (code 1).");
		}
		break;
/************************************************************************/
/* Aborted                                                              */
/************************************************************************/
	case asEXECUTION_ABORTED:
		{
			DebLog("Execution aborted (code 2).");
		}
		break;
/************************************************************************/
/* Exception                                                            */
/************************************************************************/
	case asEXECUTION_EXCEPTION:
		{
			int funcID=ctx->GetExceptionFunction();
			DebLog("Exception!");
			DebLog("Exeption desc: "+String(ctx->GetExceptionString()).ToString()+".");
		//	DebLog("Module Name : %s"+scriptEngine->GetModuleNameFromIndex(funcID >> 16));
			DebLog("Function name: "+String(scriptEngine->GetFunctionName(funcID)).ToString()+".");
			DebLog("Line number: "+ctx->GetExceptionLineNumber()+".");
		}
		break;
/************************************************************************/
/* Error                                                                */
/************************************************************************/
	default:
		{
			DebLog("Error execution, error #"+res+".");
		}
		break;
/************************************************************************/
/*                                                                      */
/************************************************************************/
	}

label_End:
	DebLog("");
}
};





/************************************************************************/
/* Static functions                                                     */
/************************************************************************/

static void DebLog(String^ str)
{
/*	Clipboard::SetText(str+"\n\n",TextDataFormat::Rtf);
	formDebug::rtbLog->SelectionStart=0;
	formDebug::rtbLog->SelectionLength=0;
	formDebug::rtbLog->Paste();*/

	formDebug::rtbLog->Text+=str+"\n";
//	formDebug::rtbLog->SelectionStart=formDebug::rtbLog->Text->Length-1;
//	formDebug::rtbLog->SelectionLength=0;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

}
