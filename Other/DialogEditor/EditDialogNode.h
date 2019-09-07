#pragma once

using namespace System;
using namespace System::ComponentModel;
using namespace System::Collections;
using namespace System::Windows::Forms;
using namespace System::Data;
using namespace System::Drawing;

namespace DialogEditor {

	/// <summary>
	/// Summary for Edit_node
	///
	/// WARNING: If you change the name of this class, you will need to change the
	///          'Resource File Name' property for the managed resource compiler tool
	///          associated with all .resx files this class depends on.  Otherwise,
	///          the designers will not be able to interact properly with localized
	///          resources associated with this form.
	/// </summary>
	public ref class Edit_node : public System::Windows::Forms::Form
	{
	public:
		Edit_node(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here
			//
		}
		// Компоненты формы.
		System::Windows::Forms::ComboBox^		cbScript;
		System::Windows::Forms::RichTextBox^	text;
		System::Windows::Forms::NumericUpDown^  num;
	public: System::Windows::Forms::Button^  btnCancel;

	public: System::Windows::Forms::Button^  btnOk;

		System::ComponentModel::Container^		components;
		System::Windows::Forms::CheckBox^		cbDialogNoShuffle;
	public: 

		// Переменные.
		bool edit;

	protected:
		~Edit_node()
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
			this->text = (gcnew System::Windows::Forms::RichTextBox());
			this->num = (gcnew System::Windows::Forms::NumericUpDown());
			this->btnCancel = (gcnew System::Windows::Forms::Button());
			this->btnOk = (gcnew System::Windows::Forms::Button());
			this->cbScript = (gcnew System::Windows::Forms::ComboBox());
			this->cbDialogNoShuffle = (gcnew System::Windows::Forms::CheckBox());
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->num))->BeginInit();
			this->SuspendLayout();
			// 
			// text
			// 
			this->text->Location = System::Drawing::Point(3, 2);
			this->text->Name = L"text";
			this->text->Size = System::Drawing::Size(292, 146);
			this->text->TabIndex = 0;
			this->text->Text = L"";
			this->text->KeyDown += gcnew System::Windows::Forms::KeyEventHandler(this, &Edit_node::text_KeyDown);
			this->text->KeyPress += gcnew System::Windows::Forms::KeyPressEventHandler(this, &Edit_node::text_KeyPress);
			// 
			// num
			// 
			this->num->Location = System::Drawing::Point(84, 154);
			this->num->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65000, 0, 0, 0});
			this->num->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {2, 0, 0, 0});
			this->num->Name = L"num";
			this->num->Size = System::Drawing::Size(128, 20);
			this->num->TabIndex = 1;
			this->num->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {2, 0, 0, 0});
			// 
			// btnCancel
			// 
			this->btnCancel->Location = System::Drawing::Point(3, 154);
			this->btnCancel->Name = L"btnCancel";
			this->btnCancel->Size = System::Drawing::Size(75, 23);
			this->btnCancel->TabIndex = 2;
			this->btnCancel->Text = L"Cancel";
			this->btnCancel->UseVisualStyleBackColor = true;
			this->btnCancel->Click += gcnew System::EventHandler(this, &Edit_node::cancel_Click);
			this->btnCancel->KeyDown += gcnew System::Windows::Forms::KeyEventHandler(this, &Edit_node::cancel_KeyDown);
			// 
			// btnOk
			// 
			this->btnOk->Location = System::Drawing::Point(220, 154);
			this->btnOk->Name = L"btnOk";
			this->btnOk->Size = System::Drawing::Size(75, 23);
			this->btnOk->TabIndex = 3;
			this->btnOk->Text = L"Ok";
			this->btnOk->UseVisualStyleBackColor = true;
			this->btnOk->Click += gcnew System::EventHandler(this, &Edit_node::ok_Click);
			// 
			// cbScript
			// 
			this->cbScript->FormattingEnabled = true;
			this->cbScript->Items->AddRange(gcnew cli::array< System::Object^  >(2) {L"None", L"Attack"});
			this->cbScript->Location = System::Drawing::Point(3, 180);
			this->cbScript->Name = L"cbScript";
			this->cbScript->Size = System::Drawing::Size(292, 21);
			this->cbScript->TabIndex = 4;
			// 
			// cbDialogNoShuffle
			// 
			this->cbDialogNoShuffle->AutoSize = true;
			this->cbDialogNoShuffle->Location = System::Drawing::Point(3, 207);
			this->cbDialogNoShuffle->Name = L"cbDialogNoShuffle";
			this->cbDialogNoShuffle->Size = System::Drawing::Size(157, 17);
			this->cbDialogNoShuffle->TabIndex = 5;
			this->cbDialogNoShuffle->Text = L"Не перемешивать ответы";
			this->cbDialogNoShuffle->UseVisualStyleBackColor = true;
			// 
			// Edit_node
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(299, 227);
			this->ControlBox = false;
			this->Controls->Add(this->cbDialogNoShuffle);
			this->Controls->Add(this->cbScript);
			this->Controls->Add(this->btnOk);
			this->Controls->Add(this->btnCancel);
			this->Controls->Add(this->num);
			this->Controls->Add(this->text);
			this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedDialog;
			this->Name = L"Edit_node";
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterParent;
			this->Text = L"Edit_node";
			this->Load += gcnew System::EventHandler(this, &Edit_node::Edit_node_Load);
			this->Shown += gcnew System::EventHandler(this, &Edit_node::Edit_node_Shown);
			this->KeyUp += gcnew System::Windows::Forms::KeyEventHandler(this, &Edit_node::Edit_node_KeyUp);
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->num))->EndInit();
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion
	private: 
		System::Void Edit_node_Load(System::Object^  sender, System::EventArgs^  e)
		{
			edit = false;
		}
		System::Void ok_Click(System::Object^  sender, System::EventArgs^  e)
		{
			edit = true;
			Close();
		}
		System::Void cancel_Click(System::Object^  sender, System::EventArgs^  e)
		{
			Close();
		}
		System::Void Edit_node_KeyUp(System::Object^  sender, System::Windows::Forms::KeyEventArgs^  e)
		{
			
		}
		System::Void text_KeyPress(System::Object^  sender, System::Windows::Forms::KeyPressEventArgs^  e)
		{
			
		}
		System::Void text_KeyDown(System::Object^  sender, System::Windows::Forms::KeyEventArgs^  e)
		{
			if (e->KeyCode==System::Windows::Forms::Keys::Escape) Close();
			if (e->KeyCode==System::Windows::Forms::Keys::F1) { edit=true; Close(); }

			if (e->KeyCode==System::Windows::Forms::Keys::PageUp) { if (num->Value<num->Maximum) num->Value++; }
			if (e->KeyCode==System::Windows::Forms::Keys::PageDown) { if (num->Value>num->Minimum) num->Value--; }
		}

		System::Void cancel_KeyDown(System::Object^  sender, System::Windows::Forms::KeyEventArgs^  e)
		{

		}
		System::Void Edit_node_Shown(System::Object^  sender, System::EventArgs^  e)
		{
			text->Focus();
			text->SelectAll();
		}
	};
}
