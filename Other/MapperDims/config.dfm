object frmConfig: TfrmConfig
  Left = 624
  Top = 366
  BorderIcons = [biSystemMenu]
  BorderStyle = bsDialog
  Caption = 'Mapper configuration'
  ClientHeight = 403
  ClientWidth = 392
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poScreenCenter
  PixelsPerInch = 96
  TextHeight = 13
  object lblMapperSilver: TLabel
    Left = 8
    Top = 369
    Width = 91
    Height = 28
    AutoSize = False
    Caption = 'Mapper'
    Color = clNone
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clBtnText
    Font.Height = -20
    Font.Name = 'Arial Black'
    Font.Style = [fsBold, fsItalic]
    ParentColor = False
    ParentFont = False
    Transparent = True
  end
  object lblMapperGray: TLabel
    Left = 7
    Top = 368
    Width = 91
    Height = 28
    AutoSize = False
    Caption = 'Mapper'
    Color = clNone
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clSilver
    Font.Height = -20
    Font.Name = 'Arial Black'
    Font.Style = [fsBold, fsItalic]
    ParentColor = False
    ParentFont = False
    Transparent = True
  end
  object gb1: TGroupBox
    Left = 1
    Top = 2
    Width = 390
    Height = 191
    Caption = ' Fallout location '
    TabOrder = 2
    object lbl1: TLabel
      Left = 14
      Top = 45
      Width = 38
      Height = 13
      Caption = 'Dat files'
    end
    object btnBrowse: TButton
      Left = 360
      Top = 17
      Width = 25
      Height = 21
      Hint = 'Click to browse'
      Caption = '...'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
      OnClick = btnBrowseClick
    end
    object edGameDir: TEdit
      Left = 8
      Top = 17
      Width = 350
      Height = 21
      ReadOnly = True
      TabOrder = 1
    end
    object lvDAT: TListView
      Left = 8
      Top = 59
      Width = 377
      Height = 121
      Columns = <
        item
          Caption = 'File Name'
          Width = 120
        end
        item
          Caption = 'Size'
          Width = 80
        end
        item
          Caption = 'Description'
          Width = 170
        end>
      GridLines = True
      ReadOnly = True
      TabOrder = 2
      ViewStyle = vsReport
    end
  end
  object btnOK: TButton
    Left = 237
    Top = 371
    Width = 75
    Height = 25
    Caption = '&OK'
    Default = True
    ModalResult = 1
    TabOrder = 0
    OnClick = btnOKClick
  end
  object btnCancel: TButton
    Left = 316
    Top = 371
    Width = 75
    Height = 25
    Cancel = True
    Caption = '&Cancel'
    ModalResult = 2
    TabOrder = 1
  end
  object gb2: TGroupBox
    Left = 1
    Top = 194
    Width = 390
    Height = 110
    Caption = ' Local data location '
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    ParentFont = False
    TabOrder = 3
    object rb1: TRadioButton
      Left = 8
      Top = 47
      Width = 377
      Height = 17
      Caption = 'Use only DAT resources'
      Enabled = False
      TabOrder = 0
    end
    object rb2: TRadioButton
      Left = 8
      Top = 63
      Width = 377
      Height = 17
      Caption = 'Use only local resources'
      Enabled = False
      TabOrder = 1
    end
    object rb3: TRadioButton
      Left = 8
      Top = 79
      Width = 377
      Height = 17
      Caption = 'Both using resources (Recommended)'
      Checked = True
      Enabled = False
      TabOrder = 2
      TabStop = True
    end
    object edDataDir: TEdit
      Left = 8
      Top = 18
      Width = 350
      Height = 21
      ReadOnly = True
      TabOrder = 3
    end
    object btnBrowse2: TButton
      Left = 360
      Top = 17
      Width = 25
      Height = 21
      Hint = 'Click to browse'
      Caption = '...'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 4
      OnClick = btnBrowse2Click
    end
  end
  object GroupBox1: TGroupBox
    Left = 1
    Top = 312
    Width = 390
    Height = 49
    Caption = 'Templates location'
    TabOrder = 4
    object txtTmplLocation: TEdit
      Left = 8
      Top = 16
      Width = 350
      Height = 21
      TabOrder = 0
    end
    object Button1: TButton
      Left = 360
      Top = 17
      Width = 25
      Height = 21
      Hint = 'Click to browse'
      Caption = '...'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      OnClick = Button1Click
    end
  end
end
