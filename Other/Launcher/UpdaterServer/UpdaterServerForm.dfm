object ServerForm: TServerForm
  Left = 0
  Top = 0
  Caption = 'Updater server'
  ClientHeight = 466
  ClientWidth = 565
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
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object Splitter: TSplitter
    Left = 0
    Top = 286
    Width = 565
    Height = 5
    Cursor = crVSplit
    Align = alBottom
    Color = clSilver
    ParentColor = False
    ExplicitTop = 315
    ExplicitWidth = 478
  end
  object MLog: TMemo
    Left = 0
    Top = 120
    Width = 565
    Height = 166
    Align = alClient
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'Courier New'
    Font.Style = []
    ParentFont = False
    ScrollBars = ssBoth
    TabOrder = 0
  end
  object LbList: TListBox
    Left = 0
    Top = 291
    Width = 565
    Height = 175
    Align = alBottom
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'Courier New'
    Font.Style = []
    ItemHeight = 14
    ParentFont = False
    TabOrder = 1
  end
  object GbOptions: TGroupBox
    Left = 0
    Top = 0
    Width = 565
    Height = 120
    Align = alTop
    TabOrder = 2
    ExplicitLeft = -42
    ExplicitTop = -9
    object LPort: TLabel
      Left = 29
      Top = 16
      Width = 77
      Height = 14
      Caption = 'Listen port'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object LConnAll: TLabel
      Left = 83
      Top = 47
      Width = 56
      Height = 14
      Caption = 'LConnAll'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object LConnSucc: TLabel
      Left = 83
      Top = 81
      Width = 63
      Height = 14
      Caption = 'LConnSucc'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object LConnFail: TLabel
      Left = 83
      Top = 98
      Width = 63
      Height = 14
      Caption = 'LConnFail'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object LConnCur: TLabel
      Left = 83
      Top = 64
      Width = 56
      Height = 14
      Caption = 'LConnCur'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object Label1: TLabel
      Left = 16
      Top = 47
      Width = 35
      Height = 14
      Caption = 'Whole'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object Label2: TLabel
      Left = 16
      Top = 64
      Width = 49
      Height = 14
      Caption = 'Current'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object Label3: TLabel
      Left = 16
      Top = 81
      Width = 49
      Height = 14
      Caption = 'Success'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object Label4: TLabel
      Left = 16
      Top = 98
      Width = 35
      Height = 14
      Caption = 'Error'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object Label5: TLabel
      Left = 163
      Top = 81
      Width = 77
      Height = 14
      Caption = 'MBytes sent'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object LBytesSent: TLabel
      Left = 264
      Top = 81
      Width = 70
      Height = 14
      Caption = 'LBytesSent'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object Label7: TLabel
      Left = 163
      Top = 98
      Width = 70
      Height = 14
      Caption = 'Files sent'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object LFilesSent: TLabel
      Left = 264
      Top = 98
      Width = 70
      Height = 14
      Caption = 'LFilesSent'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object ShapeIndicator: TShape
      Left = 434
      Top = 13
      Width = 57
      Height = 55
      Brush.Color = clRed
      Shape = stEllipse
    end
    object Label6: TLabel
      Left = 355
      Top = 81
      Width = 35
      Height = 14
      Caption = 'Pings'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object LPings: TLabel
      Left = 419
      Top = 81
      Width = 42
      Height = 14
      Caption = 'LPings'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
    end
    object CseListenPort: TCSpinEdit
      Left = 120
      Top = 13
      Width = 65
      Height = 23
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      MaxValue = 65535
      ParentFont = False
      TabOrder = 0
      Value = 4040
    end
    object BProcess: TButton
      Left = 232
      Top = 12
      Width = 136
      Height = 24
      Caption = 'Start listening'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
      TabOrder = 1
      OnClick = BProcessClick
    end
    object BRecalcFiles: TButton
      Left = 232
      Top = 42
      Width = 136
      Height = 24
      Caption = 'Refresh files'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Courier New'
      Font.Style = []
      ParentFont = False
      TabOrder = 2
      OnClick = BRecalcFilesClick
    end
  end
  object IdTCPServer: TIdTCPServer
    Bindings = <>
    DefaultPort = 0
    OnConnect = IdTCPServerConnect
    OnDisconnect = IdTCPServerDisconnect
    OnException = IdTCPServerException
    TerminateWaitTime = 60000
    OnExecute = IdTCPServerExecute
    Left = 528
    Top = 432
  end
end
