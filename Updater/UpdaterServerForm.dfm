object ServerForm: TServerForm
  Left = 0
  Top = 0
  BorderStyle = bsSingle
  Caption = 'Updater server'
  ClientHeight = 475
  ClientWidth = 491
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  Icon.Data = {
    0000010001001010000001002000680400001600000028000000100000002000
    0000010020000000000040040000000000000000000000000000000000000000
    00000000000005710AFF00000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    000005710AFF05700AFF00000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    000006740CFF08780FFF00000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    00000B7C13FF158922FF00000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    000010831BFF2CA945FF05710AFF000000000000000000000000000000000000
    0000056F09FF0000000000000000000000000000000000000000000000000000
    000010821CFF44C86AFF148520FF000000000000000000000000000000000000
    0000056F09FF056F09FF00000000000000000000000000000000000000000000
    0000097510FF43C467FF3CBD5EFF036D07FF0000000000000000000000000000
    0000056F09FF119C1FFF056F09FF000000000000000000000000000000000000
    00000000000029A13FFF5DE98FFF31B04CFF056F09FF046F08FF046E07FF056D
    07FF056F09FF14AD24FF0C9518FF056F09FF0000000000000000000000000000
    00000000000007710BFF41C264FF59E789FF40C763FF29AD42FF25AD3CFF28B0
    40FF1DA730FF14A924FF0EA51BFF0E9A1BFF056F09FF00000000000000000000
    0000000000000000000008730DFF3BBB5BFF50DD7CFF47D86FFF3BCC5DFF2FC0
    4BFF25B63CFF1AAD2CFF10A51EFF0FA81DFF129920FF056F09FF000000000000
    000000000000000000000000000005710AFF1B902CFF35B853FF39C459FF33C2
    51FF2BBC45FF1FB134FF16AD27FF169726FF056F09FF00000000000000000000
    0000000000000000000000000000000000000000000006720CFF097610FF0978
    11FF056F09FF27BD41FF149121FF056F09FF0000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    0000056F09FF148F22FF056F09FF000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    0000056F09FF056F09FF00000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    0000056F09FF0000000000000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    000000000000000000000000000000000000000000000000000000000000DFFF
    00009FFF00009FFF00009FFF00008FBF00008F9F0000878F0000C0070000C003
    0000E0010000F0030000FC070000FF8F0000FF9F0000FFBF0000FFFF0000}
  OldCreateOrder = False
  PixelsPerInch = 96
  TextHeight = 13
  object MLog: TMemo
    Left = 0
    Top = 120
    Width = 491
    Height = 248
    Align = alClient
    ScrollBars = ssBoth
    TabOrder = 0
  end
  object LbList: TListBox
    Left = 0
    Top = 368
    Width = 491
    Height = 107
    Align = alBottom
    ItemHeight = 13
    TabOrder = 1
  end
  object GbOptions: TGroupBox
    Left = 0
    Top = 0
    Width = 491
    Height = 120
    Align = alTop
    TabOrder = 2
    object LPort: TLabel
      Left = 16
      Top = 16
      Width = 51
      Height = 13
      Caption = 'Listen port'
    end
    object LConnAll: TLabel
      Left = 59
      Top = 41
      Width = 41
      Height = 13
      Caption = 'LConnAll'
    end
    object LConnSucc: TLabel
      Left = 60
      Top = 79
      Width = 52
      Height = 13
      Caption = 'LConnSucc'
    end
    object LConnFail: TLabel
      Left = 60
      Top = 98
      Width = 46
      Height = 13
      Caption = 'LConnFail'
    end
    object LConnCur: TLabel
      Left = 60
      Top = 60
      Width = 47
      Height = 13
      Caption = 'LConnCur'
    end
    object Label1: TLabel
      Left = 16
      Top = 41
      Width = 11
      Height = 13
      Caption = 'All'
    end
    object Label2: TLabel
      Left = 16
      Top = 60
      Width = 37
      Height = 13
      Caption = 'Current'
    end
    object Label3: TLabel
      Left = 16
      Top = 79
      Width = 38
      Height = 13
      Caption = 'Success'
    end
    object Label4: TLabel
      Left = 16
      Top = 98
      Width = 16
      Height = 13
      Caption = 'Fail'
    end
    object Label5: TLabel
      Left = 163
      Top = 79
      Width = 53
      Height = 13
      Caption = 'Bytes send'
    end
    object LBytesSend: TLabel
      Left = 240
      Top = 79
      Width = 56
      Height = 13
      Caption = 'LBytesSend'
    end
    object Label7: TLabel
      Left = 163
      Top = 98
      Width = 47
      Height = 13
      Caption = 'Files send'
    end
    object LFilesSend: TLabel
      Left = 240
      Top = 98
      Width = 50
      Height = 13
      Caption = 'LFilesSend'
    end
    object ShapeIndicator: TShape
      Left = 442
      Top = 74
      Width = 46
      Height = 40
      Brush.Color = clRed
      Shape = stEllipse
    end
    object CseListenPort: TCSpinEdit
      Left = 73
      Top = 13
      Width = 121
      Height = 22
      MaxValue = 65535
      TabOrder = 0
      Value = 4040
    end
    object BProcess: TButton
      Left = 200
      Top = 12
      Width = 120
      Height = 24
      Caption = 'Start listen'
      TabOrder = 1
      OnClick = BProcessClick
    end
    object BRecalcFiles: TButton
      Left = 200
      Top = 42
      Width = 120
      Height = 24
      Caption = 'Refresh files'
      TabOrder = 2
      OnClick = BRecalcFilesClick
    end
    object Button1: TButton
      Left = 326
      Top = 11
      Width = 120
      Height = 25
      Caption = 'Button1'
      TabOrder = 3
    end
    object Button2: TButton
      Left = 326
      Top = 42
      Width = 120
      Height = 25
      Caption = 'Button2'
      TabOrder = 4
    end
  end
  object IdTCPServer: TIdTCPServer
    Bindings = <>
    DefaultPort = 0
    OnConnect = IdTCPServerConnect
    OnDisconnect = IdTCPServerDisconnect
    OnException = IdTCPServerException
    OnExecute = IdTCPServerExecute
    Left = 448
    Top = 432
  end
end
