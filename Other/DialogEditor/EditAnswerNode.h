#pragma once

using namespace System;
using namespace System::ComponentModel;
using namespace System::Collections;
using namespace System::Windows::Forms;
using namespace System::Data;
using namespace System::Drawing;


namespace DialogEditor {

	/// <summary>
	/// Summary for Edit_nodeA
	///
	/// WARNING: If you change the name of this class, you will need to change the
	///          'Resource File Name' property for the managed resource compiler tool
	///          associated with all .resx files this class depends on.  Otherwise,
	///          the designers will not be able to interact properly with localized
	///          resources associated with this form.
	/// </summary>
	public ref class Edit_nodeA : public System::Windows::Forms::Form
	{
	public:
		bool edit;

		Edit_nodeA(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here
			//
		}
	public: System::Windows::Forms::Button^  btnOk;

		

	public: System::Windows::Forms::Button^  btnCancel;

	public:	System::Windows::Forms::TextBox^		answer;
	public:	System::Windows::Forms::NumericUpDown^  link;

	public: System::Windows::Forms::Label^  lError;

	public: System::Windows::Forms::ComboBox^  cbLinkOther;
	public: 
	public: System::Windows::Forms::RadioButton^  rbLinkOther;
	public: System::Windows::Forms::Label^  lErrText;
	public: System::Windows::Forms::RadioButton^  rbLinkId;

	public: System::ComponentModel::Container ^ components;

	protected:
		~Edit_nodeA()
		{
			if (components)
			{
				delete components;
			}
		}

#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			this->btnOk = (gcnew System::Windows::Forms::Button());
			this->btnCancel = (gcnew System::Windows::Forms::Button());
			this->answer = (gcnew System::Windows::Forms::TextBox());
			this->link = (gcnew System::Windows::Forms::NumericUpDown());
			this->cbLinkOther = (gcnew System::Windows::Forms::ComboBox());
			this->rbLinkOther = (gcnew System::Windows::Forms::RadioButton());
			this->rbLinkId = (gcnew System::Windows::Forms::RadioButton());
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->link))->BeginInit();
			this->SuspendLayout();
			// 
			// btnOk
			// 
			this->btnOk->Location = System::Drawing::Point(394, 4);
			this->btnOk->Name = L"btnOk";
			this->btnOk->Size = System::Drawing::Size(75, 23);
			this->btnOk->TabIndex = 3;
			this->btnOk->Text = L"Ok";
			this->btnOk->UseVisualStyleBackColor = true;
			this->btnOk->Click += gcnew System::EventHandler(this, &Edit_nodeA::ok_Click);
			// 
			// btnCancel
			// 
			this->btnCancel->Location = System::Drawing::Point(394, 33);
			this->btnCancel->Name = L"btnCancel";
			this->btnCancel->Size = System::Drawing::Size(75, 23);
			this->btnCancel->TabIndex = 2;
			this->btnCancel->Text = L"Cancel";
			this->btnCancel->UseVisualStyleBackColor = true;
			this->btnCancel->Click += gcnew System::EventHandler(this, &Edit_nodeA::Cancel_Click);
			// 
			// answer
			// 
			this->answer->Location = System::Drawing::Point(3, 3);
			this->answer->Multiline = true;
			this->answer->Name = L"answer";
			this->answer->Size = System::Drawing::Size(291, 119);
			this->answer->TabIndex = 0;
			this->answer->KeyDown += gcnew System::Windows::Forms::KeyEventHandler(this, &Edit_nodeA::answer_KeyDown);
			// 
			// link
			// 
			this->link->Location = System::Drawing::Point(300, 27);
			this->link->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65000, 0, 0, 0});
			this->link->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			this->link->Name = L"link";
			this->link->Size = System::Drawing::Size(73, 20);
			this->link->TabIndex = 1;
			this->link->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			// 
			// cbLinkOther
			// 
			this->cbLinkOther->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
			this->cbLinkOther->FormattingEnabled = true;
			this->cbLinkOther->Items->AddRange(gcnew cli::array< System::Object^  >(4) {L"Закрыть диалог / Close dialog", L"На предыдущий диалог / On previous dialog", 
				L"Бартер / Barter", L"Атаковать / Attack"});
			this->cbLinkOther->Location = System::Drawing::Point(300, 76);
			this->cbLinkOther->Name = L"cbLinkOther";
			this->cbLinkOther->Size = System::Drawing::Size(169, 21);
			this->cbLinkOther->TabIndex = 48;
			// 
			// rbLinkOther
			// 
			this->rbLinkOther->AutoSize = true;
			this->rbLinkOther->Location = System::Drawing::Point(300, 53);
			this->rbLinkOther->Name = L"rbLinkOther";
			this->rbLinkOther->Size = System::Drawing::Size(62, 17);
			this->rbLinkOther->TabIndex = 47;
			this->rbLinkOther->Text = L"Другой";
			this->rbLinkOther->UseVisualStyleBackColor = true;
			// 
			// rbLinkId
			// 
			this->rbLinkId->AutoSize = true;
			this->rbLinkId->Checked = true;
			this->rbLinkId->Location = System::Drawing::Point(300, 4);
			this->rbLinkId->Name = L"rbLinkId";
			this->rbLinkId->Size = System::Drawing::Size(77, 17);
			this->rbLinkId->TabIndex = 46;
			this->rbLinkId->TabStop = true;
			this->rbLinkId->Text = L"К диалогу";
			this->rbLinkId->UseVisualStyleBackColor = true;
			// 
			// Edit_nodeA
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(473, 128);
			this->ControlBox = false;
			this->Controls->Add(this->cbLinkOther);
			this->Controls->Add(this->rbLinkOther);
			this->Controls->Add(this->rbLinkId);
			this->Controls->Add(this->link);
			this->Controls->Add(this->answer);
			this->Controls->Add(this->btnCancel);
			this->Controls->Add(this->btnOk);
			this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedDialog;
			this->Name = L"Edit_nodeA";
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterParent;
			this->Text = L"Edit_nodeA";
			this->Load += gcnew System::EventHandler(this, &Edit_nodeA::Edit_nodeA_Load);
			this->Shown += gcnew System::EventHandler(this, &Edit_nodeA::Edit_nodeA_Shown);
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->link))->EndInit();
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion

	private:
		System::Void ok_Click        (System::Object^  sender, System::EventArgs^  e)
		{
			edit=true;
			Close();
		}
		System::Void Edit_nodeA_Load (System::Object^  sender, System::EventArgs^  e)
		{
				edit = false;
		}
		System::Void Cancel_Click    (System::Object^  sender, System::EventArgs^  e)
		{
			Close();
		}
		System::Void Edit_nodeA_Shown(System::Object^  sender, System::EventArgs^  e)
		{
			answer->Focus();
			answer->SelectAll();
		}
		System::Void answer_KeyDown  (System::Object^  sender, System::Windows::Forms::KeyEventArgs^  e)
		{
			if (e->KeyCode==System::Windows::Forms::Keys::Escape) Close();
			if (e->KeyCode==System::Windows::Forms::Keys::F1) { edit=true; Close(); }

			if ( e->KeyCode == System::Windows::Forms::Keys::PageUp ) 
				if (link->Value < link->Maximum) link->Value++; 
			if ( e->KeyCode == System::Windows::Forms::Keys::PageDown ) 
				if (link->Value>link->Minimum) link->Value--; 
		}
	};
}
