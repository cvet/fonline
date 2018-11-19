#pragma once

using namespace System;
using namespace System::ComponentModel;
using namespace System::Collections;
using namespace System::Windows::Forms;
using namespace System::Data;
using namespace System::Drawing;


namespace DialogEditor {

	/// <summary>
	/// Summary for MessBox
	///
	/// WARNING: If you change the name of this class, you will need to change the
	///          'Resource File Name' property for the managed resource compiler tool
	///          associated with all .resx files this class depends on.  Otherwise,
	///          the designers will not be able to interact properly with localized
	///          resources associated with this form.
	/// </summary>
	public ref class MessBox : public System::Windows::Forms::Form
	{
	public:
		MessBox(void)
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
		~MessBox()
		{
			if (components)
			{
				delete components;
			}
		}
	public: System::Windows::Forms::RichTextBox^  TxtBox;
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
			this->TxtBox = (gcnew System::Windows::Forms::RichTextBox());
			this->SuspendLayout();
			// 
			// txtBox
			// 
			this->TxtBox->Dock = System::Windows::Forms::DockStyle::Fill;
			this->TxtBox->Location = System::Drawing::Point(0, 0);
			this->TxtBox->Name = L"txtBox";
			this->TxtBox->Size = System::Drawing::Size(407, 205);
			this->TxtBox->TabIndex = 0;
			this->TxtBox->Text = L"";
			// 
			// MessBox
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(407, 205);
			this->Controls->Add(this->TxtBox);
			this->Name = L"MessBox";
			this->ShowIcon = false;
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
			this->ResumeLayout(false);

		}
#pragma endregion
	};
}
