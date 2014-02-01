object SelectorForm: TSelectorForm
  Left = 0
  Top = 0
  BorderIcons = [biSystemMenu, biMinimize]
  BorderStyle = bsSingle
  Caption = 'SelectorForm'
  ClientHeight = 323
  ClientWidth = 346
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  Icon.Data = {
    0000010001001010000001002000680400001600000028000000100000002000
    0000010020000000000040040000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    0000056F09FF0000000000000000000000000000000000000000000000000000
    000000000000000000000000000000000000000000000000000000000000056F
    09FF129920FF056F09FF00000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000056F09FF0E9A
    1BFF0FA81DFF169726FF056F09FF000000000000000000000000000000000000
    00000000000000000000000000000000000000000000056F09FF0C9518FF0EA5
    1BFF10A51EFF16AD27FF149121FF056F09FF0000000000000000000000000000
    000000000000000000000000000000000000056F09FF119C1FFF14AD24FF14A9
    24FF1AAD2CFF1FB134FF27BD41FF148F22FF056F09FF00000000000000000000
    0000000000000000000000000000056F09FF056F09FF056F09FF056F09FF1DA7
    30FF25B63CFF2BBC45FF056F09FF056F09FF056F09FF056F09FF000000000000
    0000000000000000000000000000000000000000000000000000056D07FF28B0
    40FF2FC04BFF33C251FF097811FF000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000046E07FF25AD
    3CFF3BCC5DFF39C459FF097610FF000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000046F08FF29AD
    42FF47D86FFF35B853FF06720CFF000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000056F09FF40C7
    63FF50DD7CFF1B902CFF00000000000000000000000000000000000000000000
    00000000000000000000000000000000000000000000036D07FF31B04CFF59E7
    89FF3BBB5BFF05710AFF00000000000000000000000000000000000000000000
    000000000000000000000000000005710AFF148520FF3CBD5EFF5DE98FFF41C2
    64FF08730DFF0000000000000000000000000000000000000000000000000571
    0AFF05700AFF08780FFF158922FF2CA945FF44C86AFF43C467FF29A13FFF0771
    0BFF000000000000000000000000000000000000000000000000000000000000
    000005710AFF06740CFF0B7C13FF10831BFF10821CFF097510FF000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    000000000000000000000000000000000000000000000000000000000000FFFF
    0000FFBF0000FF1F0000FE0F0000FC070000F8030000F0010000FE0F0000FE0F
    0000FE0F0000FE1F0000FC1F0000F03F0000007F000081FF0000FFFF0000}
  OldCreateOrder = False
  Position = poDesktopCenter
  PixelsPerInch = 96
  TextHeight = 13
  object LWaitRus: TLabel
    Left = 24
    Top = 104
    Width = 272
    Height = 16
    Caption = #1048#1076#1077#1090' '#1089#1073#1086#1088' '#1080#1085#1092#1086#1088#1084#1072#1094#1080#1080' '#1086' '#1089#1077#1088#1074#1077#1088#1072#1093'...'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clRed
    Font.Height = -13
    Font.Name = 'Courier New'
    Font.Style = [fsBold]
    ParentFont = False
  end
  object LWaitEng: TLabel
    Left = 24
    Top = 143
    Width = 312
    Height = 16
    Caption = 'Collecting information about servers...'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clRed
    Font.Height = -13
    Font.Name = 'Courier New'
    Font.Style = [fsBold]
    ParentFont = False
  end
  object PanelMain: TPanel
    Left = 0
    Top = 0
    Width = 346
    Height = 323
    Align = alClient
    TabOrder = 0
    Visible = False
    object LServerInfo: TLabel
      Left = 24
      Top = 165
      Width = 57
      Height = 13
      Caption = 'LServerInfo'
    end
    object LServers: TLabel
      Left = 24
      Top = 8
      Width = 42
      Height = 13
      Caption = 'LServers'
    end
    object LOptions: TLabel
      Left = 215
      Top = 8
      Width = 42
      Height = 13
      Caption = 'LOptions'
    end
    object LbServers: TListBox
      Left = 8
      Top = 27
      Width = 185
      Height = 134
      ItemHeight = 13
      TabOrder = 1
      OnClick = LbServersClick
      OnDblClick = LbServersDblClick
    end
    object PanelButtons: TPanel
      Left = 199
      Top = 27
      Width = 137
      Height = 134
      TabOrder = 0
      object RbLangRus: TRadioButton
        Left = 16
        Top = 8
        Width = 113
        Height = 17
        Caption = #1056#1091#1089#1089#1082#1080#1081' '#1103#1079#1099#1082
        Checked = True
        TabOrder = 3
        TabStop = True
        OnClick = RbLangRusClick
      end
      object RbLangEng: TRadioButton
        Left = 16
        Top = 31
        Width = 113
        Height = 17
        Caption = 'English language'
        TabOrder = 4
        OnClick = RbLangEngClick
      end
      object BtnSelect: TButton
        Left = 16
        Top = 54
        Width = 105
        Height = 39
        Caption = 'BtnSelect'
        TabOrder = 0
        OnClick = BtnSelectClick
      end
      object BtnExit: TButton
        Left = 72
        Top = 99
        Width = 50
        Height = 25
        Caption = 'BtnExit'
        TabOrder = 2
        OnClick = BtnExitClick
      end
      object BtnProxy: TButton
        Left = 16
        Top = 99
        Width = 50
        Height = 25
        Caption = 'BtnProxy'
        TabOrder = 1
        OnClick = BtnProxyClick
      end
    end
    object ReAbout: TRichEdit
      Left = 8
      Top = 184
      Width = 328
      Height = 129
      Font.Charset = RUSSIAN_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Tahoma'
      Font.Style = []
      ParentFont = False
      ReadOnly = True
      ScrollBars = ssBoth
      TabOrder = 2
      Visible = False
      WantReturns = False
    end
  end
  object TimerCheck: TTimer
    Enabled = False
    OnTimer = TimerCheckTimer
    Left = 288
    Top = 160
  end
end
