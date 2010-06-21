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

		
	public:	System::Windows::Forms::Button^			ok;
	public:	System::Windows::Forms::Button^			Cancel;
	public:	System::Windows::Forms::TextBox^		answer;
	public:	System::Windows::Forms::NumericUpDown^  link;
	public: System::Windows::Forms::Label^			lErr;
	public: System::Windows::Forms::Label^			label1;
	public: System::Windows::Forms::ComboBox^  cbLinkOther;
	public: 
	public: System::Windows::Forms::RadioButton^  rbLinkOther;
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
			this->ok = (gcnew System::Windows::Forms::Button());
			this->Cancel = (gcnew System::Windows::Forms::Button());
			this->answer = (gcnew System::Windows::Forms::TextBox());
			this->link = (gcnew System::Windows::Forms::NumericUpDown());
			this->lErr = (gcnew System::Windows::Forms::Label());
			this->label1 = (gcnew System::Windows::Forms::Label());
			this->cbLinkOther = (gcnew System::Windows::Forms::ComboBox());
			this->rbLinkOther = (gcnew System::Windows::Forms::RadioButton());
			this->rbLinkId = (gcnew System::Windows::Forms::RadioButton());
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->link))->BeginInit();
			this->SuspendLayout();
			// 
			// ok
			// 
			this->ok->Location = System::Drawing::Point(394, 4);
			this->ok->Name = L"ok";
			this->ok->Size = System::Drawing::Size(75, 23);
			this->ok->TabIndex = 3;
			this->ok->Text = L"Ok";
			this->ok->UseVisualStyleBackColor = true;
			this->ok->Click += gcnew System::EventHandler(this, &Edit_nodeA::ok_Click);
			// 
			// Cancel
			// 
			this->Cancel->Location = System::Drawing::Point(394, 33);
			this->Cancel->Name = L"Cancel";
			this->Cancel->Size = System::Drawing::Size(75, 23);
			this->Cancel->TabIndex = 2;
			this->Cancel->Text = L"Cancel";
			this->Cancel->UseVisualStyleBackColor = true;
			this->Cancel->Click += gcnew System::EventHandler(this, &Edit_nodeA::Cancel_Click);
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
			// lErr
			// 
			this->lErr->AutoSize = true;
			this->lErr->ForeColor = System::Drawing::Color::Red;
			this->lErr->Location = System::Drawing::Point(370, 109);
			this->lErr->Name = L"lErr";
			this->lErr->Size = System::Drawing::Size(24, 13);
			this->lErr->TabIndex = 5;
			this->lErr->Text = L"нет";
			// 
			// label1
			// 
			this->label1->AutoSize = true;
			this->label1->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 8.25F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(204)));
			this->label1->ForeColor = System::Drawing::Color::DarkGreen;
			this->label1->Location = System::Drawing::Point(316, 109);
			this->label1->Name = L"label1";
			this->label1->Size = System::Drawing::Size(57, 13);
			this->label1->TabIndex = 6;
			this->label1->Text = L"Ошибка:";
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
			this->rbLinkId->Size = System::Drawing::Size(75, 17);
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
			this->Controls->Add(this->label1);
			this->Controls->Add(this->lErr);
			this->Controls->Add(this->link);
			this->Controls->Add(this->answer);
			this->Controls->Add(this->Cancel);
			this->Controls->Add(this->ok);
			this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedDialog;
			this->Name = L"Edit_nodeA";
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterParent;
			this->Text = L"Edit_nodeA";
			this->Shown += gcnew System::EventHandler(this, &Edit_nodeA::Edit_nodeA_Shown);
			this->Load += gcnew System::EventHandler(this, &Edit_nodeA::Edit_nodeA_Load);
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
