#pragma once

using namespace System;
using namespace System::ComponentModel;
using namespace System::Collections;
using namespace System::Windows::Forms;
using namespace System::Data;
using namespace System::Drawing;


namespace DialogEditor {

	/// <summary>
	/// Summary for EnterBox
	///
	/// WARNING: If you change the name of this class, you will need to change the
	///          'Resource File Name' property for the managed resource compiler tool
	///          associated with all .resx files this class depends on.  Otherwise,
	///          the designers will not be able to interact properly with localized
	///          resources associated with this form.
	/// </summary>
	public ref class EnterBox : public System::Windows::Forms::Form
	{
	public:
		bool IsOk;
		String^ Text;

		EnterBox(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here
			//
		}

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~EnterBox()
		{
			if (components)
			{
				delete components;
			}
		}
	private: System::Windows::Forms::TextBox^  tText;
	protected: 
	private: System::Windows::Forms::Button^  btnOk;

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
			this->tText = (gcnew System::Windows::Forms::TextBox());
			this->btnOk = (gcnew System::Windows::Forms::Button());
			this->SuspendLayout();
			// 
			// tText
			// 
			this->tText->Location = System::Drawing::Point(12, 9);
			this->tText->Name = L"tText";
			this->tText->Size = System::Drawing::Size(229, 20);
			this->tText->TabIndex = 0;
			this->tText->TextChanged += gcnew System::EventHandler(this, &EnterBox::tText_TextChanged);
			// 
			// btnOk
			// 
			this->btnOk->Location = System::Drawing::Point(251, 9);
			this->btnOk->Name = L"btnOk";
			this->btnOk->Size = System::Drawing::Size(57, 23);
			this->btnOk->TabIndex = 1;
			this->btnOk->Text = L"Ok";
			this->btnOk->UseVisualStyleBackColor = true;
			this->btnOk->Click += gcnew System::EventHandler(this, &EnterBox::btnOk_Click);
			// 
			// EnterBox
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(318, 41);
			this->Controls->Add(this->btnOk);
			this->Controls->Add(this->tText);
			this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedDialog;
			this->MaximizeBox = false;
			this->MinimizeBox = false;
			this->Name = L"EnterBox";
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
			this->Activated += gcnew System::EventHandler(this, &EnterBox::EnterBox_Activated);
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion
private: System::Void btnOk_Click(System::Object^  sender, System::EventArgs^  e)
{
	if(tText->Text->Length>0)
	{
		IsOk=true;
		Close();
	}
}

private: System::Void tText_TextChanged(System::Object^  sender, System::EventArgs^  e)
{
	Text=tText->Text;
}

private: System::Void EnterBox_Activated(System::Object^  sender, System::EventArgs^  e)
{
	IsOk=false;
	tText->Text="";
}
	};
}
