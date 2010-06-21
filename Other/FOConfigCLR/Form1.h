#pragma once

#include "vcclr.h"

namespace FOConfig {

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
		String^ iface00;
		String^ iface0;
		String^ iface1;
		String^ iface2;
	private: System::Windows::Forms::ComboBox^  cbScreenWidth;
	public: 

	public: 
	private: System::Windows::Forms::Label^  lScreenHeight;
	private: System::Windows::Forms::Label^  lScreenWidth;
	private: System::Windows::Forms::ComboBox^  cbScreenHeight;
	private: System::Windows::Forms::CheckBox^  cbAlwaysOnTop;
	private: System::Windows::Forms::CheckBox^  cbGlobalSound;
	private: System::Windows::Forms::CheckBox^  cbSoundNotify;


			
	public:
		Form1(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here
			//
		}

		String^ ToString(const char* str)
		{
			return String(str).ToString();
		}

		const char* ToAnsi(String^ str)
		{
			static char buf[4096];

			pin_ptr<const wchar_t> wch=PtrToStringChars(str);
			size_t origsize=wcslen(wch)+1;
			size_t convertedChars=0;
			wcstombs(buf,wch,sizeof(buf));

			return buf;
		}

		const char* ToAnsi(System::Decimal dc)
		{
			static char buf[4096];
			UINT ui=(UINT)dc;
			sprintf(buf,"%u",ui);
			return buf;
		}

		const char* Format(const char* fmt, ...)
		{
			static char res[4096];

			va_list list;
			va_start(list,fmt);
			vsprintf(res,fmt,list);
			va_end(list);

			return res;
		}

		void Log(String^ str)
		{
			cmbLog->Items->Add(str);
			cmbLog->Text=str;
		};

		void ToEnglish()
		{
			Lang=LANG_ENG;
			rbEnglish->Checked=true;
			btnParse->Text="Save";
			btnExit->Text="Exit";
			tabOther->Text="Other";
			tabGame->Text="Game";
			tabNet->Text="Net";
			tabVideo->Text="Video";
			tabSound->Text="Sound";
			gbLanguage->Text="Language \\ Язык";
			cbMessNotify->Text="Flush window on\nnot active game";
			cbSoundNotify->Text="Beep sound on\nnot active game";
			cbInvertMessBox->Text="Invert text\nin messbox";
			cbLogging->Text="Logging in 'FOnline.log'";
			cbLoggingTime->Text="Logging with time";
			lScrollDelay->Text="Scroll delay";
			lScrollStep->Text="Scroll step";
			lMouseSpeed->Text="Mouse speed";
			lTextDelay->Text="Text delay (ms)";
			gbLangChange->Text="Keyboard language switch";
			lPathMaster->Text="Path to master.dat";
			lPathCritter->Text="Path to critter.dat";
			lHost->Text="Host";
			lPort->Text="Port";
			gbScreenMode->Text="Resolution";
			lLight->Text="Light";
			lSleep->Text="Sleep";
			lScreenWidth->Text="Width";
			lScreenHeight->Text="Height";
			lSprites->Text="Cache\nsprites";
			lTextures->Text="Textures\nsize";
			cbFullScreen->Text="Fullscreen";
			cbAlwaysOnTop->Text="Always On Top";
			cbClear->Text="Screen clear";
			cbVSync->Text="VSync";
			lMusicVolume->Text="Music volume";
			lSoundVolume->Text="Sound volume";
			cbGlobalSound->Text="Global sound";
		}

		void ToRussian()
		{
			Lang=LANG_RUS;
			rbRussian->Checked=true;
			btnParse->Text="Сохранить";
			btnExit->Text="Выход";
			tabOther->Text="Разное";
			tabGame->Text="Игра";
			tabNet->Text="Сеть";
			tabVideo->Text="Видео";
			tabSound->Text="Звук";
			gbLanguage->Text="Язык \\ Language";
			cbMessNotify->Text="Извещение о сообщениях\nпри неактивном окне.";
			cbSoundNotify->Text="Звуковое извещение о сообщениях\nпри неактивном окне.";
			cbInvertMessBox->Text="Инвертирование текста\nв окне сообщений.";
			cbLogging->Text="Ведение лога в 'FOnline.log'";
			cbLoggingTime->Text="Запись в лог с указанием времени";
			lScrollDelay->Text="Задержка скроллинга";
			lScrollStep->Text="Шаг скроллинга";
			lMouseSpeed->Text="Скорость мышки";
			lTextDelay->Text="Время задержки текста (мс)";
			gbLangChange->Text="Переключение раскладки";
			lPathMaster->Text="Путь к master.dat";
			lPathCritter->Text="Путь к critter.dat";
			lHost->Text="Хост";
			lPort->Text="Порт";
			gbScreenMode->Text="Разрешение";
			lLight->Text="Яркость";
			lSleep->Text="Sleep";
			lScreenWidth->Text="Ширина";
			lScreenHeight->Text="Высота";
			lSprites->Text="Кешируемые\nспрайты";
			lTextures->Text="Размер\nтекстур";
			cbFullScreen->Text="Полноэкранный режим";
			cbAlwaysOnTop->Text="Поверх всех окон";
			cbClear->Text="Очистка экрана";
			cbVSync->Text="Вертикальная синхронизация";
			lMusicVolume->Text="Громкость музыки";
			lSoundVolume->Text="Громкость звуков";
			cbGlobalSound->Text="Постоянный звук";
		}

		int Lang;

	private: System::Windows::Forms::Label^  lSoundVolume;
	public: 

	public: 

	private: System::Windows::Forms::Label^  lMusicVolume;
	public: 

	private: System::Windows::Forms::NumericUpDown^  numSoundVolume;

	private: System::Windows::Forms::NumericUpDown^  numMusicVolume;
	private: System::Windows::Forms::CheckBox^  cbInvertMessBox;


private: System::Windows::Forms::CheckBox^  cbLoggingTime;
private: System::Windows::Forms::CheckBox^  cbLogging;




#pragma region Components

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
private: System::Windows::Forms::TabPage^  tabGame;
private: System::Windows::Forms::TabPage^  tabNet;
private: System::Windows::Forms::TabPage^  tabVideo;
private: System::Windows::Forms::GroupBox^  gbScreenMode;
	protected: 
	private: System::Windows::Forms::CheckBox^  cbFullScreen;
private: System::Windows::Forms::TabPage^  tabSound;
private: System::Windows::Forms::Button^  btnParse;
private: System::Windows::Forms::Button^  btnExit;
	private: System::Windows::Forms::NumericUpDown^  nTextures;
	private: System::Windows::Forms::NumericUpDown^  nSprites;
	private: System::Windows::Forms::NumericUpDown^  nSleep;
	private: System::Windows::Forms::NumericUpDown^  nLight;
private: System::Windows::Forms::Label^  lTextures;
private: System::Windows::Forms::Label^  lSprites;
private: System::Windows::Forms::Label^  lSleep;
private: System::Windows::Forms::Label^  lLight;
	private: System::Windows::Forms::CheckBox^  cbVSync;
	private: System::Windows::Forms::CheckBox^  cbClear;
	private: System::Windows::Forms::NumericUpDown^  nTextDelay;
	private: System::Windows::Forms::NumericUpDown^  nMouseSpeed;
	private: System::Windows::Forms::NumericUpDown^  nScrollStep;
	private: System::Windows::Forms::NumericUpDown^  nScrollDelay;
private: System::Windows::Forms::GroupBox^  gbLangChange;
	private: System::Windows::Forms::RadioButton^  rbKB_AS;
	private: System::Windows::Forms::RadioButton^  rbKB_CS;
private: System::Windows::Forms::Label^  lTextDelay;
private: System::Windows::Forms::Label^  lMouseSpeed;
private: System::Windows::Forms::Label^  lScrollStep;
private: System::Windows::Forms::Label^  lScrollDelay;
	private: System::Windows::Forms::TextBox^  tCritterPath;
	private: System::Windows::Forms::TextBox^  tMasterPath;
private: System::Windows::Forms::Label^  lPathCritter;
private: System::Windows::Forms::Label^  lPathMaster;
private: System::Windows::Forms::TabPage^  tabOther;
	private: System::Windows::Forms::Button^  button5;
	private: System::Windows::Forms::Button^  button4;
private: System::Windows::Forms::ComboBox^  cmbHost;
private: System::Windows::Forms::NumericUpDown^  nPort;
private: System::Windows::Forms::Label^  lPort;
private: System::Windows::Forms::Label^  lHost;
	private: System::Windows::Forms::ComboBox^  cmbLog;
private: System::Windows::Forms::GroupBox^  gbLanguage;
private: System::Windows::Forms::RadioButton^  rbEnglish;
private: System::Windows::Forms::RadioButton^  rbRussian;
private: System::Windows::Forms::CheckBox^  cbMessNotify;
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
			this->tabOther = (gcnew System::Windows::Forms::TabPage());
			this->cbSoundNotify = (gcnew System::Windows::Forms::CheckBox());
			this->cbLoggingTime = (gcnew System::Windows::Forms::CheckBox());
			this->cbLogging = (gcnew System::Windows::Forms::CheckBox());
			this->cbInvertMessBox = (gcnew System::Windows::Forms::CheckBox());
			this->cbMessNotify = (gcnew System::Windows::Forms::CheckBox());
			this->gbLanguage = (gcnew System::Windows::Forms::GroupBox());
			this->rbEnglish = (gcnew System::Windows::Forms::RadioButton());
			this->rbRussian = (gcnew System::Windows::Forms::RadioButton());
			this->tabGame = (gcnew System::Windows::Forms::TabPage());
			this->button5 = (gcnew System::Windows::Forms::Button());
			this->button4 = (gcnew System::Windows::Forms::Button());
			this->tCritterPath = (gcnew System::Windows::Forms::TextBox());
			this->tMasterPath = (gcnew System::Windows::Forms::TextBox());
			this->lPathCritter = (gcnew System::Windows::Forms::Label());
			this->lPathMaster = (gcnew System::Windows::Forms::Label());
			this->nTextDelay = (gcnew System::Windows::Forms::NumericUpDown());
			this->nMouseSpeed = (gcnew System::Windows::Forms::NumericUpDown());
			this->nScrollStep = (gcnew System::Windows::Forms::NumericUpDown());
			this->nScrollDelay = (gcnew System::Windows::Forms::NumericUpDown());
			this->gbLangChange = (gcnew System::Windows::Forms::GroupBox());
			this->rbKB_AS = (gcnew System::Windows::Forms::RadioButton());
			this->rbKB_CS = (gcnew System::Windows::Forms::RadioButton());
			this->lTextDelay = (gcnew System::Windows::Forms::Label());
			this->lMouseSpeed = (gcnew System::Windows::Forms::Label());
			this->lScrollStep = (gcnew System::Windows::Forms::Label());
			this->lScrollDelay = (gcnew System::Windows::Forms::Label());
			this->tabNet = (gcnew System::Windows::Forms::TabPage());
			this->cmbHost = (gcnew System::Windows::Forms::ComboBox());
			this->nPort = (gcnew System::Windows::Forms::NumericUpDown());
			this->lPort = (gcnew System::Windows::Forms::Label());
			this->lHost = (gcnew System::Windows::Forms::Label());
			this->tabVideo = (gcnew System::Windows::Forms::TabPage());
			this->cbAlwaysOnTop = (gcnew System::Windows::Forms::CheckBox());
			this->nTextures = (gcnew System::Windows::Forms::NumericUpDown());
			this->nSprites = (gcnew System::Windows::Forms::NumericUpDown());
			this->nSleep = (gcnew System::Windows::Forms::NumericUpDown());
			this->nLight = (gcnew System::Windows::Forms::NumericUpDown());
			this->lTextures = (gcnew System::Windows::Forms::Label());
			this->lSprites = (gcnew System::Windows::Forms::Label());
			this->lSleep = (gcnew System::Windows::Forms::Label());
			this->lLight = (gcnew System::Windows::Forms::Label());
			this->cbVSync = (gcnew System::Windows::Forms::CheckBox());
			this->cbClear = (gcnew System::Windows::Forms::CheckBox());
			this->gbScreenMode = (gcnew System::Windows::Forms::GroupBox());
			this->cbScreenHeight = (gcnew System::Windows::Forms::ComboBox());
			this->cbScreenWidth = (gcnew System::Windows::Forms::ComboBox());
			this->lScreenHeight = (gcnew System::Windows::Forms::Label());
			this->lScreenWidth = (gcnew System::Windows::Forms::Label());
			this->cbFullScreen = (gcnew System::Windows::Forms::CheckBox());
			this->tabSound = (gcnew System::Windows::Forms::TabPage());
			this->cbGlobalSound = (gcnew System::Windows::Forms::CheckBox());
			this->lSoundVolume = (gcnew System::Windows::Forms::Label());
			this->lMusicVolume = (gcnew System::Windows::Forms::Label());
			this->numSoundVolume = (gcnew System::Windows::Forms::NumericUpDown());
			this->numMusicVolume = (gcnew System::Windows::Forms::NumericUpDown());
			this->btnParse = (gcnew System::Windows::Forms::Button());
			this->btnExit = (gcnew System::Windows::Forms::Button());
			this->cmbLog = (gcnew System::Windows::Forms::ComboBox());
			this->tabControl1->SuspendLayout();
			this->tabOther->SuspendLayout();
			this->gbLanguage->SuspendLayout();
			this->tabGame->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nTextDelay))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nMouseSpeed))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nScrollStep))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nScrollDelay))->BeginInit();
			this->gbLangChange->SuspendLayout();
			this->tabNet->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPort))->BeginInit();
			this->tabVideo->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nTextures))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nSprites))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nSleep))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nLight))->BeginInit();
			this->gbScreenMode->SuspendLayout();
			this->tabSound->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numSoundVolume))->BeginInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMusicVolume))->BeginInit();
			this->SuspendLayout();
			// 
			// tabControl1
			// 
			this->tabControl1->Controls->Add(this->tabOther);
			this->tabControl1->Controls->Add(this->tabGame);
			this->tabControl1->Controls->Add(this->tabNet);
			this->tabControl1->Controls->Add(this->tabVideo);
			this->tabControl1->Controls->Add(this->tabSound);
			this->tabControl1->Location = System::Drawing::Point(8, 12);
			this->tabControl1->Name = L"tabControl1";
			this->tabControl1->SelectedIndex = 0;
			this->tabControl1->Size = System::Drawing::Size(251, 266);
			this->tabControl1->TabIndex = 0;
			// 
			// tabOther
			// 
			this->tabOther->Controls->Add(this->cbSoundNotify);
			this->tabOther->Controls->Add(this->cbLoggingTime);
			this->tabOther->Controls->Add(this->cbLogging);
			this->tabOther->Controls->Add(this->cbInvertMessBox);
			this->tabOther->Controls->Add(this->cbMessNotify);
			this->tabOther->Controls->Add(this->gbLanguage);
			this->tabOther->Location = System::Drawing::Point(4, 22);
			this->tabOther->Name = L"tabOther";
			this->tabOther->Size = System::Drawing::Size(243, 240);
			this->tabOther->TabIndex = 4;
			this->tabOther->Text = L"tabOther";
			this->tabOther->UseVisualStyleBackColor = true;
			// 
			// cbSoundNotify
			// 
			this->cbSoundNotify->AutoSize = true;
			this->cbSoundNotify->Location = System::Drawing::Point(3, 107);
			this->cbSoundNotify->Name = L"cbSoundNotify";
			this->cbSoundNotify->Size = System::Drawing::Size(96, 17);
			this->cbSoundNotify->TabIndex = 5;
			this->cbSoundNotify->Text = L"cbSoundNotify";
			this->cbSoundNotify->UseVisualStyleBackColor = true;
			// 
			// cbLoggingTime
			// 
			this->cbLoggingTime->AutoSize = true;
			this->cbLoggingTime->Location = System::Drawing::Point(3, 197);
			this->cbLoggingTime->Name = L"cbLoggingTime";
			this->cbLoggingTime->Size = System::Drawing::Size(99, 17);
			this->cbLoggingTime->TabIndex = 4;
			this->cbLoggingTime->Text = L"cbLoggingTime";
			this->cbLoggingTime->UseVisualStyleBackColor = true;
			// 
			// cbLogging
			// 
			this->cbLogging->AutoSize = true;
			this->cbLogging->Location = System::Drawing::Point(3, 179);
			this->cbLogging->Name = L"cbLogging";
			this->cbLogging->Size = System::Drawing::Size(76, 17);
			this->cbLogging->TabIndex = 3;
			this->cbLogging->Text = L"cbLogging";
			this->cbLogging->UseVisualStyleBackColor = true;
			// 
			// cbInvertMessBox
			// 
			this->cbInvertMessBox->AutoSize = true;
			this->cbInvertMessBox->Location = System::Drawing::Point(3, 144);
			this->cbInvertMessBox->Name = L"cbInvertMessBox";
			this->cbInvertMessBox->Size = System::Drawing::Size(108, 17);
			this->cbInvertMessBox->TabIndex = 2;
			this->cbInvertMessBox->Text = L"cbInvertMessBox";
			this->cbInvertMessBox->UseVisualStyleBackColor = true;
			// 
			// cbMessNotify
			// 
			this->cbMessNotify->AutoSize = true;
			this->cbMessNotify->Location = System::Drawing::Point(3, 76);
			this->cbMessNotify->Name = L"cbMessNotify";
			this->cbMessNotify->Size = System::Drawing::Size(90, 17);
			this->cbMessNotify->TabIndex = 1;
			this->cbMessNotify->Text = L"cbMessNotify";
			this->cbMessNotify->UseVisualStyleBackColor = true;
			// 
			// gbLanguage
			// 
			this->gbLanguage->Controls->Add(this->rbEnglish);
			this->gbLanguage->Controls->Add(this->rbRussian);
			this->gbLanguage->Location = System::Drawing::Point(3, 3);
			this->gbLanguage->Name = L"gbLanguage";
			this->gbLanguage->Size = System::Drawing::Size(125, 67);
			this->gbLanguage->TabIndex = 0;
			this->gbLanguage->TabStop = false;
			this->gbLanguage->Text = L"gbLanguage";
			// 
			// rbEnglish
			// 
			this->rbEnglish->AutoSize = true;
			this->rbEnglish->Location = System::Drawing::Point(6, 42);
			this->rbEnglish->Name = L"rbEnglish";
			this->rbEnglish->Size = System::Drawing::Size(59, 17);
			this->rbEnglish->TabIndex = 1;
			this->rbEnglish->TabStop = true;
			this->rbEnglish->Text = L"English";
			this->rbEnglish->UseVisualStyleBackColor = true;
			this->rbEnglish->CheckedChanged += gcnew System::EventHandler(this, &Form1::rbEnglish_CheckedChanged);
			// 
			// rbRussian
			// 
			this->rbRussian->AutoSize = true;
			this->rbRussian->Location = System::Drawing::Point(6, 19);
			this->rbRussian->Name = L"rbRussian";
			this->rbRussian->Size = System::Drawing::Size(67, 17);
			this->rbRussian->TabIndex = 0;
			this->rbRussian->TabStop = true;
			this->rbRussian->Text = L"Русский";
			this->rbRussian->UseVisualStyleBackColor = true;
			this->rbRussian->CheckedChanged += gcnew System::EventHandler(this, &Form1::rbRussian_CheckedChanged);
			// 
			// tabGame
			// 
			this->tabGame->Controls->Add(this->button5);
			this->tabGame->Controls->Add(this->button4);
			this->tabGame->Controls->Add(this->tCritterPath);
			this->tabGame->Controls->Add(this->tMasterPath);
			this->tabGame->Controls->Add(this->lPathCritter);
			this->tabGame->Controls->Add(this->lPathMaster);
			this->tabGame->Controls->Add(this->nTextDelay);
			this->tabGame->Controls->Add(this->nMouseSpeed);
			this->tabGame->Controls->Add(this->nScrollStep);
			this->tabGame->Controls->Add(this->nScrollDelay);
			this->tabGame->Controls->Add(this->gbLangChange);
			this->tabGame->Controls->Add(this->lTextDelay);
			this->tabGame->Controls->Add(this->lMouseSpeed);
			this->tabGame->Controls->Add(this->lScrollStep);
			this->tabGame->Controls->Add(this->lScrollDelay);
			this->tabGame->Location = System::Drawing::Point(4, 22);
			this->tabGame->Name = L"tabGame";
			this->tabGame->Padding = System::Windows::Forms::Padding(3);
			this->tabGame->Size = System::Drawing::Size(243, 240);
			this->tabGame->TabIndex = 0;
			this->tabGame->Text = L"tabGame";
			this->tabGame->UseVisualStyleBackColor = true;
			// 
			// button5
			// 
			this->button5->Location = System::Drawing::Point(112, 138);
			this->button5->Name = L"button5";
			this->button5->Size = System::Drawing::Size(21, 20);
			this->button5->TabIndex = 21;
			this->button5->Text = L">";
			this->button5->UseVisualStyleBackColor = true;
			this->button5->Click += gcnew System::EventHandler(this, &Form1::button5_Click);
			// 
			// button4
			// 
			this->button4->Location = System::Drawing::Point(112, 119);
			this->button4->Name = L"button4";
			this->button4->Size = System::Drawing::Size(21, 20);
			this->button4->TabIndex = 20;
			this->button4->Text = L">";
			this->button4->UseVisualStyleBackColor = true;
			this->button4->Click += gcnew System::EventHandler(this, &Form1::button4_Click);
			// 
			// tCritterPath
			// 
			this->tCritterPath->Location = System::Drawing::Point(133, 138);
			this->tCritterPath->Name = L"tCritterPath";
			this->tCritterPath->Size = System::Drawing::Size(100, 20);
			this->tCritterPath->TabIndex = 18;
			// 
			// tMasterPath
			// 
			this->tMasterPath->Location = System::Drawing::Point(133, 119);
			this->tMasterPath->Name = L"tMasterPath";
			this->tMasterPath->Size = System::Drawing::Size(100, 20);
			this->tMasterPath->TabIndex = 17;
			// 
			// lPathCritter
			// 
			this->lPathCritter->AutoSize = true;
			this->lPathCritter->Location = System::Drawing::Point(4, 141);
			this->lPathCritter->Name = L"lPathCritter";
			this->lPathCritter->Size = System::Drawing::Size(58, 13);
			this->lPathCritter->TabIndex = 15;
			this->lPathCritter->Text = L"lPathCritter";
			// 
			// lPathMaster
			// 
			this->lPathMaster->AutoSize = true;
			this->lPathMaster->Location = System::Drawing::Point(4, 122);
			this->lPathMaster->Name = L"lPathMaster";
			this->lPathMaster->Size = System::Drawing::Size(63, 13);
			this->lPathMaster->TabIndex = 13;
			this->lPathMaster->Text = L"lPathMaster";
			// 
			// nTextDelay
			// 
			this->nTextDelay->Increment = System::Decimal(gcnew cli::array< System::Int32 >(4) {100, 0, 0, 0});
			this->nTextDelay->Location = System::Drawing::Point(164, 56);
			this->nTextDelay->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {30000, 0, 0, 0});
			this->nTextDelay->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000, 0, 0, 0});
			this->nTextDelay->Name = L"nTextDelay";
			this->nTextDelay->Size = System::Drawing::Size(72, 20);
			this->nTextDelay->TabIndex = 11;
			this->nTextDelay->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000, 0, 0, 0});
			// 
			// nMouseSpeed
			// 
			this->nMouseSpeed->Location = System::Drawing::Point(164, 38);
			this->nMouseSpeed->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {5, 0, 0, 0});
			this->nMouseSpeed->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			this->nMouseSpeed->Name = L"nMouseSpeed";
			this->nMouseSpeed->Size = System::Drawing::Size(72, 20);
			this->nMouseSpeed->TabIndex = 10;
			this->nMouseSpeed->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			// 
			// nScrollStep
			// 
			this->nScrollStep->Increment = System::Decimal(gcnew cli::array< System::Int32 >(4) {4, 0, 0, 0});
			this->nScrollStep->Location = System::Drawing::Point(164, 20);
			this->nScrollStep->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {32, 0, 0, 0});
			this->nScrollStep->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {4, 0, 0, 0});
			this->nScrollStep->Name = L"nScrollStep";
			this->nScrollStep->Size = System::Drawing::Size(72, 20);
			this->nScrollStep->TabIndex = 9;
			this->nScrollStep->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {4, 0, 0, 0});
			// 
			// nScrollDelay
			// 
			this->nScrollDelay->Location = System::Drawing::Point(164, 1);
			this->nScrollDelay->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {32, 0, 0, 0});
			this->nScrollDelay->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			this->nScrollDelay->Name = L"nScrollDelay";
			this->nScrollDelay->Size = System::Drawing::Size(72, 20);
			this->nScrollDelay->TabIndex = 8;
			this->nScrollDelay->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			// 
			// gbLangChange
			// 
			this->gbLangChange->Controls->Add(this->rbKB_AS);
			this->gbLangChange->Controls->Add(this->rbKB_CS);
			this->gbLangChange->Location = System::Drawing::Point(7, 74);
			this->gbLangChange->Name = L"gbLangChange";
			this->gbLangChange->Size = System::Drawing::Size(229, 42);
			this->gbLangChange->TabIndex = 6;
			this->gbLangChange->TabStop = false;
			this->gbLangChange->Text = L"gbLangChange";
			// 
			// rbKB_AS
			// 
			this->rbKB_AS->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Right));
			this->rbKB_AS->AutoSize = true;
			this->rbKB_AS->Location = System::Drawing::Point(126, 19);
			this->rbKB_AS->Name = L"rbKB_AS";
			this->rbKB_AS->Size = System::Drawing::Size(70, 17);
			this->rbKB_AS->TabIndex = 1;
			this->rbKB_AS->TabStop = true;
			this->rbKB_AS->Text = L"Alt + Shift";
			this->rbKB_AS->UseVisualStyleBackColor = true;
			// 
			// rbKB_CS
			// 
			this->rbKB_CS->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Right));
			this->rbKB_CS->AutoSize = true;
			this->rbKB_CS->Location = System::Drawing::Point(10, 19);
			this->rbKB_CS->Name = L"rbKB_CS";
			this->rbKB_CS->Size = System::Drawing::Size(73, 17);
			this->rbKB_CS->TabIndex = 0;
			this->rbKB_CS->TabStop = true;
			this->rbKB_CS->Text = L"Ctrl + Shift";
			this->rbKB_CS->UseVisualStyleBackColor = true;
			// 
			// lTextDelay
			// 
			this->lTextDelay->AutoSize = true;
			this->lTextDelay->Location = System::Drawing::Point(3, 58);
			this->lTextDelay->Name = L"lTextDelay";
			this->lTextDelay->Size = System::Drawing::Size(57, 13);
			this->lTextDelay->TabIndex = 4;
			this->lTextDelay->Text = L"lTextDelay";
			// 
			// lMouseSpeed
			// 
			this->lMouseSpeed->AutoSize = true;
			this->lMouseSpeed->Location = System::Drawing::Point(3, 40);
			this->lMouseSpeed->Name = L"lMouseSpeed";
			this->lMouseSpeed->Size = System::Drawing::Size(72, 13);
			this->lMouseSpeed->TabIndex = 3;
			this->lMouseSpeed->Text = L"lMouseSpeed";
			// 
			// lScrollStep
			// 
			this->lScrollStep->AutoSize = true;
			this->lScrollStep->Location = System::Drawing::Point(3, 22);
			this->lScrollStep->Name = L"lScrollStep";
			this->lScrollStep->Size = System::Drawing::Size(57, 13);
			this->lScrollStep->TabIndex = 2;
			this->lScrollStep->Text = L"lScrollStep";
			// 
			// lScrollDelay
			// 
			this->lScrollDelay->AutoSize = true;
			this->lScrollDelay->Location = System::Drawing::Point(3, 3);
			this->lScrollDelay->Name = L"lScrollDelay";
			this->lScrollDelay->Size = System::Drawing::Size(62, 13);
			this->lScrollDelay->TabIndex = 1;
			this->lScrollDelay->Text = L"lScrollDelay";
			// 
			// tabNet
			// 
			this->tabNet->Controls->Add(this->cmbHost);
			this->tabNet->Controls->Add(this->nPort);
			this->tabNet->Controls->Add(this->lPort);
			this->tabNet->Controls->Add(this->lHost);
			this->tabNet->Location = System::Drawing::Point(4, 22);
			this->tabNet->Name = L"tabNet";
			this->tabNet->Padding = System::Windows::Forms::Padding(3);
			this->tabNet->Size = System::Drawing::Size(243, 240);
			this->tabNet->TabIndex = 1;
			this->tabNet->Text = L"tabNet";
			this->tabNet->UseVisualStyleBackColor = true;
			// 
			// cmbHost
			// 
			this->cmbHost->FormattingEnabled = true;
			this->cmbHost->Location = System::Drawing::Point(69, 3);
			this->cmbHost->Name = L"cmbHost";
			this->cmbHost->Size = System::Drawing::Size(116, 21);
			this->cmbHost->TabIndex = 4;
			this->cmbHost->Text = L"255.255.255.255";
			// 
			// nPort
			// 
			this->nPort->Location = System::Drawing::Point(69, 26);
			this->nPort->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {65535, 0, 0, 0});
			this->nPort->Name = L"nPort";
			this->nPort->Size = System::Drawing::Size(55, 20);
			this->nPort->TabIndex = 3;
			this->nPort->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			// 
			// lPort
			// 
			this->lPort->AutoSize = true;
			this->lPort->Location = System::Drawing::Point(5, 28);
			this->lPort->Name = L"lPort";
			this->lPort->Size = System::Drawing::Size(28, 13);
			this->lPort->TabIndex = 1;
			this->lPort->Text = L"lPort";
			// 
			// lHost
			// 
			this->lHost->AutoSize = true;
			this->lHost->Location = System::Drawing::Point(5, 6);
			this->lHost->Name = L"lHost";
			this->lHost->Size = System::Drawing::Size(31, 13);
			this->lHost->TabIndex = 0;
			this->lHost->Text = L"lHost";
			// 
			// tabVideo
			// 
			this->tabVideo->Controls->Add(this->cbAlwaysOnTop);
			this->tabVideo->Controls->Add(this->nTextures);
			this->tabVideo->Controls->Add(this->nSprites);
			this->tabVideo->Controls->Add(this->nSleep);
			this->tabVideo->Controls->Add(this->nLight);
			this->tabVideo->Controls->Add(this->lTextures);
			this->tabVideo->Controls->Add(this->lSprites);
			this->tabVideo->Controls->Add(this->lSleep);
			this->tabVideo->Controls->Add(this->lLight);
			this->tabVideo->Controls->Add(this->cbVSync);
			this->tabVideo->Controls->Add(this->cbClear);
			this->tabVideo->Controls->Add(this->gbScreenMode);
			this->tabVideo->Controls->Add(this->cbFullScreen);
			this->tabVideo->Location = System::Drawing::Point(4, 22);
			this->tabVideo->Name = L"tabVideo";
			this->tabVideo->Size = System::Drawing::Size(243, 240);
			this->tabVideo->TabIndex = 2;
			this->tabVideo->Text = L"tabVideo";
			this->tabVideo->UseVisualStyleBackColor = true;
			// 
			// cbAlwaysOnTop
			// 
			this->cbAlwaysOnTop->AutoSize = true;
			this->cbAlwaysOnTop->Location = System::Drawing::Point(3, 176);
			this->cbAlwaysOnTop->Name = L"cbAlwaysOnTop";
			this->cbAlwaysOnTop->Size = System::Drawing::Size(104, 17);
			this->cbAlwaysOnTop->TabIndex = 20;
			this->cbAlwaysOnTop->Text = L"cbAlwaysOnTop";
			this->cbAlwaysOnTop->UseVisualStyleBackColor = true;
			// 
			// nTextures
			// 
			this->nTextures->Location = System::Drawing::Point(177, 86);
			this->nTextures->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {8192, 0, 0, 0});
			this->nTextures->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {128, 0, 0, 0});
			this->nTextures->Name = L"nTextures";
			this->nTextures->Size = System::Drawing::Size(55, 20);
			this->nTextures->TabIndex = 19;
			this->nTextures->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {512, 0, 0, 0});
			this->nTextures->ValueChanged += gcnew System::EventHandler(this, &Form1::nTextures_ValueChanged);
			// 
			// nSprites
			// 
			this->nSprites->Location = System::Drawing::Point(177, 52);
			this->nSprites->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1000, 0, 0, 0});
			this->nSprites->Minimum = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			this->nSprites->Name = L"nSprites";
			this->nSprites->Size = System::Drawing::Size(55, 20);
			this->nSprites->TabIndex = 18;
			this->nSprites->Value = System::Decimal(gcnew cli::array< System::Int32 >(4) {1, 0, 0, 0});
			// 
			// nSleep
			// 
			this->nSleep->Location = System::Drawing::Point(177, 26);
			this->nSleep->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {10, 0, 0, 0});
			this->nSleep->Name = L"nSleep";
			this->nSleep->Size = System::Drawing::Size(55, 20);
			this->nSleep->TabIndex = 17;
			// 
			// nLight
			// 
			this->nLight->Location = System::Drawing::Point(177, 7);
			this->nLight->Maximum = System::Decimal(gcnew cli::array< System::Int32 >(4) {50, 0, 0, 0});
			this->nLight->Name = L"nLight";
			this->nLight->Size = System::Drawing::Size(55, 20);
			this->nLight->TabIndex = 16;
			// 
			// lTextures
			// 
			this->lTextures->AutoSize = true;
			this->lTextures->Location = System::Drawing::Point(104, 80);
			this->lTextures->Name = L"lTextures";
			this->lTextures->Size = System::Drawing::Size(50, 13);
			this->lTextures->TabIndex = 10;
			this->lTextures->Text = L"lTextures";
			// 
			// lSprites
			// 
			this->lSprites->AutoSize = true;
			this->lSprites->Location = System::Drawing::Point(103, 49);
			this->lSprites->Name = L"lSprites";
			this->lSprites->Size = System::Drawing::Size(41, 13);
			this->lSprites->TabIndex = 9;
			this->lSprites->Text = L"lSprites";
			// 
			// lSleep
			// 
			this->lSleep->AutoSize = true;
			this->lSleep->Location = System::Drawing::Point(104, 26);
			this->lSleep->Name = L"lSleep";
			this->lSleep->Size = System::Drawing::Size(36, 13);
			this->lSleep->TabIndex = 8;
			this->lSleep->Text = L"lSleep";
			// 
			// lLight
			// 
			this->lLight->AutoSize = true;
			this->lLight->Location = System::Drawing::Point(104, 7);
			this->lLight->Name = L"lLight";
			this->lLight->Size = System::Drawing::Size(32, 13);
			this->lLight->TabIndex = 7;
			this->lLight->Text = L"lLight";
			// 
			// cbVSync
			// 
			this->cbVSync->AutoSize = true;
			this->cbVSync->Location = System::Drawing::Point(3, 153);
			this->cbVSync->Name = L"cbVSync";
			this->cbVSync->Size = System::Drawing::Size(178, 17);
			this->cbVSync->TabIndex = 3;
			this->cbVSync->Text = L"Вертикальная синхронизация";
			this->cbVSync->UseVisualStyleBackColor = true;
			// 
			// cbClear
			// 
			this->cbClear->AutoSize = true;
			this->cbClear->Location = System::Drawing::Point(3, 130);
			this->cbClear->Name = L"cbClear";
			this->cbClear->Size = System::Drawing::Size(188, 17);
			this->cbClear->TabIndex = 2;
			this->cbClear->Text = L"Дополнительня очистка экрана";
			this->cbClear->UseVisualStyleBackColor = true;
			// 
			// gbScreenMode
			// 
			this->gbScreenMode->Controls->Add(this->cbScreenHeight);
			this->gbScreenMode->Controls->Add(this->cbScreenWidth);
			this->gbScreenMode->Controls->Add(this->lScreenHeight);
			this->gbScreenMode->Controls->Add(this->lScreenWidth);
			this->gbScreenMode->Location = System::Drawing::Point(3, 3);
			this->gbScreenMode->Name = L"gbScreenMode";
			this->gbScreenMode->Size = System::Drawing::Size(95, 100);
			this->gbScreenMode->TabIndex = 1;
			this->gbScreenMode->TabStop = false;
			this->gbScreenMode->Text = L"gbScreenMode";
			// 
			// cbScreenHeight
			// 
			this->cbScreenHeight->FormattingEnabled = true;
			this->cbScreenHeight->Items->AddRange(gcnew cli::array< System::Object^  >(4) {L"480", L"600", L"768", L"1024"});
			this->cbScreenHeight->Location = System::Drawing::Point(9, 72);
			this->cbScreenHeight->Name = L"cbScreenHeight";
			this->cbScreenHeight->Size = System::Drawing::Size(80, 21);
			this->cbScreenHeight->TabIndex = 3;
			this->cbScreenHeight->SelectedIndexChanged += gcnew System::EventHandler(this, &Form1::cbScreenHeight_SelectedIndexChanged);
			// 
			// cbScreenWidth
			// 
			this->cbScreenWidth->FormattingEnabled = true;
			this->cbScreenWidth->Items->AddRange(gcnew cli::array< System::Object^  >(4) {L"640", L"800", L"1024", L"1280"});
			this->cbScreenWidth->Location = System::Drawing::Point(9, 32);
			this->cbScreenWidth->Name = L"cbScreenWidth";
			this->cbScreenWidth->Size = System::Drawing::Size(80, 21);
			this->cbScreenWidth->TabIndex = 2;
			this->cbScreenWidth->SelectedIndexChanged += gcnew System::EventHandler(this, &Form1::comboBox1_SelectedIndexChanged);
			// 
			// lScreenHeight
			// 
			this->lScreenHeight->AutoSize = true;
			this->lScreenHeight->Location = System::Drawing::Point(6, 56);
			this->lScreenHeight->Name = L"lScreenHeight";
			this->lScreenHeight->Size = System::Drawing::Size(74, 13);
			this->lScreenHeight->TabIndex = 1;
			this->lScreenHeight->Text = L"lScreenHeight";
			// 
			// lScreenWidth
			// 
			this->lScreenWidth->AutoSize = true;
			this->lScreenWidth->Location = System::Drawing::Point(6, 16);
			this->lScreenWidth->Name = L"lScreenWidth";
			this->lScreenWidth->Size = System::Drawing::Size(71, 13);
			this->lScreenWidth->TabIndex = 0;
			this->lScreenWidth->Text = L"lScreenWidth";
			// 
			// cbFullScreen
			// 
			this->cbFullScreen->AutoSize = true;
			this->cbFullScreen->Location = System::Drawing::Point(3, 109);
			this->cbFullScreen->Name = L"cbFullScreen";
			this->cbFullScreen->Size = System::Drawing::Size(145, 17);
			this->cbFullScreen->TabIndex = 0;
			this->cbFullScreen->Text = L"Полноэкранный режим";
			this->cbFullScreen->UseVisualStyleBackColor = true;
			// 
			// tabSound
			// 
			this->tabSound->Controls->Add(this->cbGlobalSound);
			this->tabSound->Controls->Add(this->lSoundVolume);
			this->tabSound->Controls->Add(this->lMusicVolume);
			this->tabSound->Controls->Add(this->numSoundVolume);
			this->tabSound->Controls->Add(this->numMusicVolume);
			this->tabSound->Location = System::Drawing::Point(4, 22);
			this->tabSound->Name = L"tabSound";
			this->tabSound->Size = System::Drawing::Size(243, 240);
			this->tabSound->TabIndex = 3;
			this->tabSound->Text = L"tabSound";
			this->tabSound->UseVisualStyleBackColor = true;
			// 
			// cbGlobalSound
			// 
			this->cbGlobalSound->AutoSize = true;
			this->cbGlobalSound->Location = System::Drawing::Point(12, 90);
			this->cbGlobalSound->Name = L"cbGlobalSound";
			this->cbGlobalSound->Size = System::Drawing::Size(99, 17);
			this->cbGlobalSound->TabIndex = 4;
			this->cbGlobalSound->Text = L"cbGlobalSound";
			this->cbGlobalSound->UseVisualStyleBackColor = true;
			// 
			// lSoundVolume
			// 
			this->lSoundVolume->AutoSize = true;
			this->lSoundVolume->Location = System::Drawing::Point(9, 48);
			this->lSoundVolume->Name = L"lSoundVolume";
			this->lSoundVolume->Size = System::Drawing::Size(75, 13);
			this->lSoundVolume->TabIndex = 3;
			this->lSoundVolume->Text = L"lSoundVolume";
			// 
			// lMusicVolume
			// 
			this->lMusicVolume->AutoSize = true;
			this->lMusicVolume->Location = System::Drawing::Point(9, 9);
			this->lMusicVolume->Name = L"lMusicVolume";
			this->lMusicVolume->Size = System::Drawing::Size(72, 13);
			this->lMusicVolume->TabIndex = 2;
			this->lMusicVolume->Text = L"lMusicVolume";
			// 
			// numSoundVolume
			// 
			this->numSoundVolume->Location = System::Drawing::Point(12, 64);
			this->numSoundVolume->Name = L"numSoundVolume";
			this->numSoundVolume->Size = System::Drawing::Size(59, 20);
			this->numSoundVolume->TabIndex = 1;
			// 
			// numMusicVolume
			// 
			this->numMusicVolume->Location = System::Drawing::Point(12, 25);
			this->numMusicVolume->Name = L"numMusicVolume";
			this->numMusicVolume->Size = System::Drawing::Size(59, 20);
			this->numMusicVolume->TabIndex = 0;
			// 
			// btnParse
			// 
			this->btnParse->Location = System::Drawing::Point(8, 280);
			this->btnParse->Name = L"btnParse";
			this->btnParse->Size = System::Drawing::Size(75, 28);
			this->btnParse->TabIndex = 2;
			this->btnParse->Text = L"btnParse";
			this->btnParse->UseVisualStyleBackColor = true;
			this->btnParse->Click += gcnew System::EventHandler(this, &Form1::button2_Click);
			// 
			// btnExit
			// 
			this->btnExit->Location = System::Drawing::Point(184, 284);
			this->btnExit->Name = L"btnExit";
			this->btnExit->Size = System::Drawing::Size(75, 24);
			this->btnExit->TabIndex = 3;
			this->btnExit->Text = L"btnExit";
			this->btnExit->UseVisualStyleBackColor = true;
			this->btnExit->Click += gcnew System::EventHandler(this, &Form1::button3_Click);
			// 
			// cmbLog
			// 
			this->cmbLog->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
			this->cmbLog->FormattingEnabled = true;
			this->cmbLog->Location = System::Drawing::Point(8, 314);
			this->cmbLog->Name = L"cmbLog";
			this->cmbLog->Size = System::Drawing::Size(251, 21);
			this->cmbLog->TabIndex = 4;
			// 
			// Form1
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(267, 343);
			this->Controls->Add(this->cmbLog);
			this->Controls->Add(this->btnExit);
			this->Controls->Add(this->btnParse);
			this->Controls->Add(this->tabControl1);
			this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedSingle;
			this->Icon = (cli::safe_cast<System::Drawing::Icon^  >(resources->GetObject(L"$this.Icon")));
			this->MaximizeBox = false;
			this->Name = L"Form1";
			this->Text = L"FOnline Configurator v.1.30";
			this->Load += gcnew System::EventHandler(this, &Form1::Form1_Load);
			this->tabControl1->ResumeLayout(false);
			this->tabOther->ResumeLayout(false);
			this->tabOther->PerformLayout();
			this->gbLanguage->ResumeLayout(false);
			this->gbLanguage->PerformLayout();
			this->tabGame->ResumeLayout(false);
			this->tabGame->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nTextDelay))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nMouseSpeed))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nScrollStep))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nScrollDelay))->EndInit();
			this->gbLangChange->ResumeLayout(false);
			this->gbLangChange->PerformLayout();
			this->tabNet->ResumeLayout(false);
			this->tabNet->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nPort))->EndInit();
			this->tabVideo->ResumeLayout(false);
			this->tabVideo->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nTextures))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nSprites))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nSleep))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->nLight))->EndInit();
			this->gbScreenMode->ResumeLayout(false);
			this->gbScreenMode->PerformLayout();
			this->tabSound->ResumeLayout(false);
			this->tabSound->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numSoundVolume))->EndInit();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->numMusicVolume))->EndInit();
			this->ResumeLayout(false);

		}
#pragma endregion

#pragma endregion

#define LOG(rus,eng) do{if(Lang==LANG_RUS) Log(rus); else Log(eng);} while(0)
#define CHECK(val_,min_,max_,def_) do{if(val_<min_ || val_>max_) val_=def_;} while(0)

private: System::Void Form1_Load(System::Object^  sender, System::EventArgs^  e)
{
	char buf[4096];
	UINT ui;

//Set language
	GetPrivateProfileString(CFG_FILE_APP_NAME,"Language","russ",buf,2048,CFG_FILE);
	if(!strcmp(buf,"russ")) Lang=LANG_RUS;
	else if(!strcmp(buf,"engl")) Lang=LANG_ENG;
	else Lang=LANG_RUS;

	if(Lang==LANG_RUS) ToRussian();
	else ToEnglish();

	LOG("Загрузка параметров из 'FOnline.cfg'.","Load options from 'FOnline.cfg'.");

//Get options
	GetPrivateProfileString(CFG_FILE_APP_NAME,"RemoteHost","localhost",buf,2048,CFG_FILE);
	cmbHost->Text=ToString(buf);
	cmbHost->Items->Add(ToString(buf));

	for(int i=0;i<100;++i)
	{
		GetPrivateProfileString(CFG_FILE_APP_NAME,Format("RemoteHost_%d",i),"empty",buf,2048,CFG_FILE);
		if(!stricmp(buf,"empty")) continue;
		cmbHost->Items->Add(ToString(buf));
	}

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"RemotePort",4000,CFG_FILE);
	CHECK(ui,0,0xFFFF,4000);
	nPort->Value=ui;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"MusicVolume",100,CFG_FILE);
	CHECK(ui,0,100,100);
	numMusicVolume->Value=ui;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"SoundVolume",100,CFG_FILE);
	CHECK(ui,0,100,100);
	numSoundVolume->Value=ui;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"GlobalSound",1,CFG_FILE);
	CHECK(ui,0,1,1);
	cbGlobalSound->Checked=ui?true:false;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"ScreenWidth",800,CFG_FILE);
	CHECK(ui,320,1280,800);
	cbScreenWidth->Text=Convert::ToString(ui);

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"ScreenHeight",600,CFG_FILE);
	CHECK(ui,200,1024,600);
	cbScreenHeight->Text=Convert::ToString(ui);

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"FullScreen",0,CFG_FILE);
	CHECK(ui,0,1,0);
	cbFullScreen->Checked=ui?true:false;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"AlwaysOnTop",0,CFG_FILE);
	CHECK(ui,0,1,0);
	cbAlwaysOnTop->Checked=ui?true:false;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"Light",20,CFG_FILE);
	CHECK(ui,0,50,20);
	nLight->Value=ui;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"ScrollDelay",4,CFG_FILE);
	CHECK(ui,1,32,4);
	nScrollDelay->Value=ui;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"ScrollStep",32,CFG_FILE);
	CHECK(ui,4,32,32);
	nScrollStep->Value=ui;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"MouseSpeed",1,CFG_FILE);
	CHECK(ui,1,5,1);
	nMouseSpeed->Value=ui;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"TextDelay",3000,CFG_FILE);
	CHECK(ui,1000,30000,3000);
	nTextDelay->Value=ui;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"Sleep",1,CFG_FILE);
	CHECK(ui,0,10,1);
	nSleep->Value=ui;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"LangChange",0,CFG_FILE);
	CHECK(ui,0,1,0);
	switch(ui)
	{
	case 0: rbKB_CS->Checked=true; break;
	case 1: rbKB_AS->Checked=true; break;
	default: break;
	}

	GetPrivateProfileString(CFG_FILE_APP_NAME,"Iface_default","default800x600.ini",buf,2048,CFG_FILE);
	iface00=ToString(buf);
	GetPrivateProfileString(CFG_FILE_APP_NAME,"Iface_800x600","default800x600.ini",buf,2048,CFG_FILE);
	iface0=ToString(buf);
	GetPrivateProfileString(CFG_FILE_APP_NAME,"Iface_1024x768","default1024x768.ini",buf,2048,CFG_FILE);
	iface1=ToString(buf);
	GetPrivateProfileString(CFG_FILE_APP_NAME,"Iface_1280x1024","default1280x1024.ini",buf,2048,CFG_FILE);
	iface2=ToString(buf);

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"InvertMessBox",0,CFG_FILE);
	CHECK(ui,0,1,0);
	cbInvertMessBox->Checked=ui?true:false;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"WinNotify",1,CFG_FILE);
	CHECK(ui,0,1,1);
	cbMessNotify->Checked=ui?true:false;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"SoundNotify",0,CFG_FILE);
	CHECK(ui,0,1,0);
	cbSoundNotify->Checked=ui?true:false;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"Logging",1,CFG_FILE);
	CHECK(ui,0,1,1);
	cbLogging->Checked=ui?true:false;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"LoggingTime",0,CFG_FILE);
	CHECK(ui,0,1,0);
	cbLoggingTime->Checked=ui?true:false;

	GetPrivateProfileString(CFG_FILE_APP_NAME,"MasterDatPath","master.dat",buf,2048,CFG_FILE);
	tMasterPath->Text=ToString(buf);

	GetPrivateProfileString(CFG_FILE_APP_NAME,"CritterDatPath","critter.dat",buf,2048,CFG_FILE);
	tCritterPath->Text=ToString(buf);

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"FlushValue",50,CFG_FILE);
	CHECK(ui,1,1000,50);
	nSprites->Value=ui;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"BaseTexture",512,CFG_FILE);
	CHECK(ui,128,8192,512);
	nTextures->Value=ui;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"VSync",0,CFG_FILE);
	CHECK(ui,0,1,0);
	cbVSync->Checked=ui?true:false;

	ui=GetPrivateProfileInt(CFG_FILE_APP_NAME,"BackGroundClear",0,CFG_FILE);
	CHECK(ui,0,1,0);
	cbClear->Checked=ui?true:false;

	LOG("Загрузка закончена.","Load compleate.");
}

private: System::Void button2_Click(System::Object^  sender, System::EventArgs^  e)
{
	LOG("Сохранение параметров в 'FOnline.cfg'.","Save options in 'FOnline.cfg'.");

	if(Lang==LANG_RUS) WritePrivateProfileString(CFG_FILE_APP_NAME,"Language","russ",CFG_FILE);
	else WritePrivateProfileString(CFG_FILE_APP_NAME,"Language","engl",CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"RemoteHost",ToAnsi(cmbHost->Text),CFG_FILE);
	for(int i=0,j=0;i<cmbHost->Items->Count;++i)
	{
		if(cmbHost->Text==cmbHost->Items[i]->ToString()) continue;
		WritePrivateProfileString(CFG_FILE_APP_NAME,Format("RemoteHost_%d",j),ToAnsi(cmbHost->Items[i]->ToString()),CFG_FILE);
		j++;
	}
	WritePrivateProfileString(CFG_FILE_APP_NAME,"RemotePort",ToAnsi(nPort->Value),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"MusicVolume",ToAnsi(numMusicVolume->Value),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"SoundVolume",ToAnsi(numSoundVolume->Value),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"GlobalSound",cbGlobalSound->Checked?"1":"0",CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"ScreenWidth",ToAnsi(cbScreenWidth->Text),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"ScreenHeight",ToAnsi(cbScreenHeight->Text),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"FullScreen",cbFullScreen->Checked?"1":"0",CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"AlwaysOnTop",cbAlwaysOnTop->Checked?"1":"0",CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"Light",ToAnsi(nLight->Value),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"ScrollDelay",ToAnsi(nScrollDelay->Value),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"ScrollStep",ToAnsi(nScrollStep->Value),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"MouseSpeed",ToAnsi(nMouseSpeed->Value),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"TextDelay",ToAnsi(nTextDelay->Value),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"Sleep",ToAnsi(nSleep->Value),CFG_FILE);
	if(rbKB_CS->Checked) WritePrivateProfileString(CFG_FILE_APP_NAME,"LangChange","0",CFG_FILE);
	else if(rbKB_AS->Checked) WritePrivateProfileString(CFG_FILE_APP_NAME,"LangChange","1",CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"Iface_default",ToAnsi(iface00),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"Iface_800x600",ToAnsi(iface0),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"Iface_1024x768",ToAnsi(iface1),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"Iface_1280x1024",ToAnsi(iface2),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"InvertMessBox",ToAnsi(cbInvertMessBox->Checked?"1":"0"),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"WinNotify",ToAnsi(cbMessNotify->Checked?"1":"0"),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"SoundNotify",ToAnsi(cbSoundNotify->Checked?"1":"0"),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"Logging",ToAnsi(cbLogging->Checked?"1":"0"),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"LoggingTime",ToAnsi(cbLoggingTime->Checked?"1":"0"),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"MasterDatPath",ToAnsi(tMasterPath->Text),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"CritterDatPath",ToAnsi(tCritterPath->Text),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"FlushValue",ToAnsi(nSprites->Value),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"BaseTexture",ToAnsi(nTextures->Value),CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"VSync",cbVSync->Checked?"1":"0",CFG_FILE);
	WritePrivateProfileString(CFG_FILE_APP_NAME,"BackGroundClear",cbClear->Checked?"1":"0",CFG_FILE);

	LOG("Сохранение закончено.","Save successful.");
}

private: System::Void button3_Click(System::Object^  sender, System::EventArgs^  e)
{
	Close();
}

private: System::Void button4_Click(System::Object^  sender, System::EventArgs^  e)
{
//Master
	OpenFileDialog^ dlg=gcnew OpenFileDialog;

	dlg->InitialDirectory=tMasterPath->Text;
	dlg->Filter="Fallout2 Master.dat file|master.dat";
	dlg->RestoreDirectory=true;

	if(dlg->ShowDialog()==System::Windows::Forms::DialogResult::OK) tMasterPath->Text=dlg->FileName;
}

private: System::Void button5_Click(System::Object^  sender, System::EventArgs^  e)
{
//Critter
	OpenFileDialog^ dlg=gcnew OpenFileDialog;

	dlg->InitialDirectory=tCritterPath->Text;
	dlg->Filter="Fallout2 Critter.dat file|critter.dat";
	dlg->RestoreDirectory=true;

	if(dlg->ShowDialog()==System::Windows::Forms::DialogResult::OK) tCritterPath->Text=dlg->FileName;
}

private: System::Void nTextures_ValueChanged(System::Object^  sender, System::EventArgs^  e) {
 }


private: System::Void rbRussian_CheckedChanged(System::Object^  sender, System::EventArgs^  e)
{
	if(rbRussian->Checked) ToRussian();
}

private: System::Void rbEnglish_CheckedChanged(System::Object^  sender, System::EventArgs^  e)
{
	if(rbEnglish->Checked) ToEnglish();
}

private: System::Void comboBox1_SelectedIndexChanged(System::Object^  sender, System::EventArgs^  e)
{
	cbScreenHeight->SelectedIndex=cbScreenWidth->SelectedIndex;
}
private: System::Void cbScreenHeight_SelectedIndexChanged(System::Object^  sender, System::EventArgs^  e)
{
	cbScreenWidth->SelectedIndex=cbScreenHeight->SelectedIndex;
}
};
}

