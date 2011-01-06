object MainForm: TMainForm
  Left = 329
  Top = 192
  BorderIcons = [biSystemMenu, biMinimize]
  BorderStyle = bsSingle
  Caption = 'MainForm'
  ClientHeight = 313
  ClientWidth = 273
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  Icon.Data = {
    0000010001002020100000000000E80200001600000028000000200000004000
    0000010004000000000000000000000000000000000000000000000000000000
    0000000080000080000000808000800000008000800080800000C0C0C0008080
    80000000FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF000000
    000080000000000000000088880000000F780800000000000000080008800000
    00F78080000000000000809100880880000F7808000000000008091097080088
    0007780800000000008091097108070880877808000000000809109719000F80
    08777808000000008091097190000F78877778088000000809109719000000F7
    7777FF8088000080910971900000000FFFFFF778088000091097190000000000
    00007F77808800910971900000000000000007F7780880109719000000000000
    0000007F77800009719000000000000000000007F70910071900000000000000
    000000007F709100000000000000000000000000078009108000000000000000
    00000000080700908800000000000000000000008070F7080880000000000000
    0000000807007F77808800000000000000000080700007F77808800000000000
    000008070000007F77808800000000000000807000000007F778088000000000
    00080700000000007F77808888000000008070000000000007F7F80000800000
    0807000000000000007FF788880800008070000000000000000F777777800080
    08000000000000000000778000F0080880000000000000000000780800000077
    80000000000000000000F8080000077700000000000000000000F70880000770
    000000000000000000000F70800000000000000000000000000000000000F07F
    FFC3F03FFF81F81FFF009C0FFE000C0FFC00000FF801000FF0030007E0078003
    C00FC001C01FE000803FFF00007FFF8000FFFFC001FFFFE003FFFFF007FFFFF8
    03FFFFF001FFFFE000FFFFC3007FFF87803FFF0FC01FFE1FE003FC3FF001F87F
    F800F0FFFC00C1FFFE0083FFFE0C83FFFE0E07FFFE070FFFFF079FFFFF8F}
  OldCreateOrder = False
  Position = poDesigned
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object BtnExit: TButton
    Left = 188
    Top = 281
    Width = 80
    Height = 29
    Caption = 'BtnExit'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -13
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    TabOrder = 0
    OnClick = BtnExitClick
  end
  object BtnParse: TButton
    Left = 98
    Top = 281
    Width = 80
    Height = 29
    Caption = 'BtnParse'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -13
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    TabOrder = 1
    OnClick = BtnParseClick
  end
  object PageControl: TPageControl
    Left = 8
    Top = 5
    Width = 260
    Height = 270
    ActivePage = TabOther
    TabOrder = 2
    object TabOther: TTabSheet
      Caption = 'TabOther'
      object GbLanguage: TGroupBox
        Left = 3
        Top = 0
        Width = 246
        Height = 65
        Caption = 'GbLanguage'
        TabOrder = 0
        object RbRussian: TRadioButton
          Left = 25
          Top = 16
          Width = 100
          Height = 17
          Caption = 'RbRussian'
          TabOrder = 0
          OnClick = RbRussianClick
        end
        object RbEnglish: TRadioButton
          Left = 25
          Top = 39
          Width = 100
          Height = 17
          Caption = 'RbEnglish'
          TabOrder = 1
          OnClick = RbEnglishClick
        end
      end
      object GbOther: TGroupBox
        Left = 3
        Top = 71
        Width = 246
        Height = 168
        Caption = 'GbOther'
        TabOrder = 1
        object LabelSleep: TLabel
          Left = 55
          Top = 143
          Width = 51
          Height = 13
          Caption = 'LabelSleep'
        end
        object CbWinNotify: TCheckBox
          Left = 3
          Top = 11
          Width = 232
          Height = 34
          Caption = 'CbWinNotify'
          Color = clBtnFace
          ParentColor = False
          TabOrder = 0
          WordWrap = True
        end
        object CbSoundNotify: TCheckBox
          Left = 3
          Top = 41
          Width = 232
          Height = 34
          Caption = 'CbSoundNotify'
          Color = clBtnFace
          ParentColor = False
          TabOrder = 1
          WordWrap = True
        end
        object CbInvertMessBox: TCheckBox
          Left = 3
          Top = 71
          Width = 232
          Height = 34
          Caption = 'CbInvertMessBox'
          Color = clBtnFace
          ParentColor = False
          TabOrder = 2
          WordWrap = True
        end
        object CbLogging: TCheckBox
          Left = 3
          Top = 100
          Width = 234
          Height = 19
          Caption = 'CbLogging'
          Color = clBtnFace
          ParentColor = False
          TabOrder = 3
          WordWrap = True
        end
        object CbLoggingTime: TCheckBox
          Left = 3
          Top = 119
          Width = 232
          Height = 18
          Caption = 'CbLoggingTime'
          Color = clBtnFace
          ParentColor = False
          TabOrder = 4
          WordWrap = True
        end
        object SeSleep: TCSpinEdit
          Left = 141
          Top = 140
          Width = 69
          Height = 22
          MaxValue = 100
          MinValue = -1
          TabOrder = 5
        end
      end
    end
    object TabGame: TTabSheet
      Caption = 'TabGame'
      ImageIndex = 1
      object GbGame: TGroupBox
        Left = 3
        Top = 3
        Width = 246
        Height = 136
        Caption = 'GbGame'
        TabOrder = 0
        object LabelTextDelay: TLabel
          Left = 13
          Top = 93
          Width = 74
          Height = 13
          Caption = 'LabelTextDelay'
        end
        object LabelMouseSpeed: TLabel
          Left = 13
          Top = 67
          Width = 86
          Height = 13
          Caption = 'LabelMouseSpeed'
        end
        object LabelScrollStep: TLabel
          Left = 13
          Top = 41
          Width = 72
          Height = 13
          Caption = 'LabelScrollStep'
        end
        object LabelScrollDelay: TLabel
          Left = 13
          Top = 15
          Width = 77
          Height = 13
          Caption = 'LabelScrollDelay'
        end
        object SeScrollDelay: TCSpinEdit
          Left = 160
          Top = 12
          Width = 83
          Height = 22
          MaxValue = 32
          MinValue = 1
          TabOrder = 0
          Value = 1
        end
        object SeScrollStep: TCSpinEdit
          Left = 160
          Top = 38
          Width = 83
          Height = 22
          Increment = 4
          MaxValue = 32
          MinValue = 4
          TabOrder = 1
          Value = 4
        end
        object SeMouseSpeed: TCSpinEdit
          Left = 160
          Top = 64
          Width = 83
          Height = 22
          MaxValue = 1000
          MinValue = 10
          TabOrder = 2
          Value = 100
        end
        object SeTextDelay: TCSpinEdit
          Left = 160
          Top = 90
          Width = 83
          Height = 22
          Increment = 100
          MaxValue = 30000
          MinValue = 1000
          TabOrder = 3
          Value = 1000
        end
        object CbAlwaysRun: TCheckBox
          Left = 13
          Top = 112
          Width = 97
          Height = 21
          Caption = 'CbAlwaysRun'
          TabOrder = 4
        end
      end
      object GbPath: TGroupBox
        Left = 3
        Top = 176
        Width = 246
        Height = 64
        Caption = 'GbPath'
        TabOrder = 1
        object LabelMasterPath: TLabel
          Left = 13
          Top = 15
          Width = 80
          Height = 13
          Caption = 'LabelMasterPath'
        end
        object LabelCritterPath: TLabel
          Left = 13
          Top = 38
          Width = 78
          Height = 13
          Caption = 'LabelCritterPath'
        end
        object EditMasterPath: TEdit
          Left = 122
          Top = 12
          Width = 121
          Height = 21
          TabOrder = 0
          Text = 'EditMasterPath'
        end
        object EditCritterPath: TEdit
          Left = 122
          Top = 36
          Width = 121
          Height = 21
          TabOrder = 1
          Text = 'EditCritterPath'
        end
        object BtnMasterPath: TButton
          Left = 97
          Top = 13
          Width = 27
          Height = 18
          Caption = 'BtnMasterPath'
          TabOrder = 2
          OnClick = BtnMasterPathClick
        end
        object BtnCritterPath: TButton
          Left = 97
          Top = 37
          Width = 27
          Height = 18
          Caption = 'BtnCritterPath'
          TabOrder = 3
          OnClick = BtnCritterPathClick
        end
      end
      object GbLangSwitch: TGroupBox
        Left = 3
        Top = 142
        Width = 246
        Height = 33
        Caption = 'GbLangSwitch'
        TabOrder = 2
        object RbCtrlShift: TRadioButton
          Left = 13
          Top = 13
          Width = 102
          Height = 17
          Caption = 'RbCtrlShift'
          TabOrder = 0
        end
        object RbAltShift: TRadioButton
          Left = 133
          Top = 15
          Width = 102
          Height = 14
          Caption = 'RbAltShift'
          TabOrder = 1
        end
      end
    end
    object TabCombat: TTabSheet
      Caption = 'TabCombat'
      ImageIndex = 5
      object GbDefCmbtMode: TGroupBox
        Left = 3
        Top = 3
        Width = 246
        Height = 70
        Caption = 'GbDefCmbtMode'
        TabOrder = 0
        object RbDefCmbtModeBoth: TRadioButton
          Left = 16
          Top = 16
          Width = 113
          Height = 17
          Caption = 'RbDefCmbtModeBoth'
          Checked = True
          TabOrder = 0
          TabStop = True
        end
        object RbDefCmbtModeRt: TRadioButton
          Left = 16
          Top = 31
          Width = 113
          Height = 17
          Caption = 'RbDefCmbtModeRt'
          TabOrder = 1
        end
        object RbDefCmbtModeTb: TRadioButton
          Left = 16
          Top = 46
          Width = 113
          Height = 17
          Caption = 'RbDefCmbtModeTb'
          TabOrder = 2
        end
      end
      object GbIndicator: TGroupBox
        Left = 3
        Top = 79
        Width = 246
        Height = 67
        Caption = 'GbIndicator'
        TabOrder = 1
        object RbIndicatorLines: TRadioButton
          Left = 16
          Top = 31
          Width = 113
          Height = 17
          Caption = 'RbIndicatorLines'
          Checked = True
          TabOrder = 0
          TabStop = True
        end
        object RbIndicatorNumbers: TRadioButton
          Left = 16
          Top = 46
          Width = 113
          Height = 17
          Caption = 'RbIndicatorNumbers'
          TabOrder = 1
        end
        object RbIndicatorBoth: TRadioButton
          Left = 16
          Top = 16
          Width = 113
          Height = 17
          Caption = 'RbIndicatorBoth'
          TabOrder = 2
        end
      end
      object GbCmbtMess: TGroupBox
        Left = 3
        Top = 152
        Width = 246
        Height = 42
        Caption = 'GbCmbtMess'
        TabOrder = 2
        object RbCmbtMessVerbose: TRadioButton
          Left = 16
          Top = 16
          Width = 113
          Height = 17
          Caption = 'RbCmbtMessVerbose'
          Checked = True
          TabOrder = 0
          TabStop = True
        end
        object RbCmbtMessBrief: TRadioButton
          Left = 130
          Top = 15
          Width = 113
          Height = 17
          Caption = 'RbCmbtMessBrief'
          TabOrder = 1
        end
      end
      object GbDamageHitDelay: TGroupBox
        Left = 3
        Top = 200
        Width = 246
        Height = 39
        Caption = 'GbDamageHitDelay'
        TabOrder = 3
        object LabelDamageHitDelay: TLabel
          Left = 16
          Top = 18
          Width = 104
          Height = 13
          Caption = 'LabelDamageHitDelay'
        end
        object SeDamageHitDelayValue: TCSpinEdit
          Left = 160
          Top = 14
          Width = 73
          Height = 22
          MaxValue = 30000
          TabOrder = 0
        end
      end
    end
    object TabNet: TTabSheet
      Caption = 'TabNet'
      ImageIndex = 2
      object GbServer: TGroupBox
        Left = 3
        Top = 3
        Width = 246
        Height = 62
        Caption = 'GbServer'
        TabOrder = 0
        object LabelServerHost: TLabel
          Left = 16
          Top = 13
          Width = 79
          Height = 13
          Caption = 'LabelServerHost'
        end
        object LabelServerPort: TLabel
          Left = 194
          Top = 13
          Width = 77
          Height = 13
          Caption = 'LabelServerPort'
        end
        object CbServerHost: TComboBox
          Left = 3
          Top = 32
          Width = 185
          Height = 21
          ItemHeight = 13
          TabOrder = 0
          Text = 'CbServerHost'
        end
        object SeServerPort: TCSpinEdit
          Left = 190
          Top = 32
          Width = 52
          Height = 22
          MaxValue = 65535
          TabOrder = 1
        end
      end
      object GbProxy: TGroupBox
        Left = 3
        Top = 71
        Width = 246
        Height = 168
        Caption = 'GbProxy'
        TabOrder = 1
        object LabelProxyType: TLabel
          Left = 72
          Top = 10
          Width = 77
          Height = 13
          Caption = 'LabelProxyType'
        end
        object LabelProxyHost: TLabel
          Left = 16
          Top = 39
          Width = 75
          Height = 13
          Caption = 'LabelProxyHost'
        end
        object LabelProxyPort: TLabel
          Left = 194
          Top = 39
          Width = 73
          Height = 13
          Caption = 'LabelProxyPort'
        end
        object LabelProxyName: TLabel
          Left = 16
          Top = 81
          Width = 80
          Height = 13
          Caption = 'LabelProxyName'
        end
        object LabelProxyPass: TLabel
          Left = 16
          Top = 123
          Width = 75
          Height = 13
          Caption = 'LabelProxyPass'
        end
        object RbProxyNone: TRadioButton
          Left = 3
          Top = 24
          Width = 113
          Height = 17
          Caption = 'RbProxyNone'
          TabOrder = 6
        end
        object RbProxySocks4: TRadioButton
          Left = 60
          Top = 24
          Width = 113
          Height = 17
          Caption = 'RbProxySocks4'
          TabOrder = 0
        end
        object RbProxySocks5: TRadioButton
          Left = 122
          Top = 24
          Width = 113
          Height = 17
          Caption = 'RbProxySocks5'
          TabOrder = 1
        end
        object RbProxyHttp: TRadioButton
          Left = 179
          Top = 24
          Width = 64
          Height = 17
          Caption = 'RbProxyHttp'
          TabOrder = 2
        end
        object SeProxyPort: TCSpinEdit
          Left = 190
          Top = 54
          Width = 52
          Height = 22
          MaxValue = 65535
          TabOrder = 3
        end
        object CbProxyHost: TComboBox
          Left = 3
          Top = 54
          Width = 185
          Height = 21
          ItemHeight = 13
          TabOrder = 4
          Text = 'CbProxyHost'
        end
        object EditProxyUser: TEdit
          Left = 3
          Top = 96
          Width = 240
          Height = 21
          TabOrder = 5
          Text = 'EditProxyUser'
        end
        object EditProxyPass: TEdit
          Left = 3
          Top = 138
          Width = 240
          Height = 21
          TabOrder = 7
          Text = 'EditProxyPass'
        end
      end
    end
    object TabVideo: TTabSheet
      Caption = 'TabVideo'
      ImageIndex = 3
      object GbScreenSize: TGroupBox
        Left = 0
        Top = 3
        Width = 249
        Height = 46
        Caption = 'GbScreenSize'
        TabOrder = 0
        object CbScreenWidth: TComboBox
          Left = 3
          Top = 16
          Width = 118
          Height = 21
          ItemHeight = 13
          TabOrder = 0
          Text = 'CbScreenWidth'
          OnCloseUp = CbScreenWidthChange
          Items.Strings = (
            '640'
            '800'
            '1024'
            '1280')
        end
        object CbScreenHeight: TComboBox
          Left = 127
          Top = 16
          Width = 118
          Height = 21
          ItemHeight = 13
          TabOrder = 1
          Text = 'CbScreenHeight'
          OnCloseUp = CbScreenHeightChange
          Items.Strings = (
            '480'
            '600'
            '768'
            '1024')
        end
      end
      object GbVideoOther: TGroupBox
        Left = 0
        Top = 55
        Width = 249
        Height = 184
        Caption = 'GbVideoOther'
        TabOrder = 1
        object LabelLight: TLabel
          Left = 11
          Top = 12
          Width = 48
          Height = 13
          Caption = 'LabelLight'
        end
        object LabelSprites: TLabel
          Left = 86
          Top = 12
          Width = 58
          Height = 13
          Caption = 'LabelSprites'
        end
        object LabelTexture: TLabel
          Left = 165
          Top = 12
          Width = 63
          Height = 13
          Caption = 'LabelTexture'
        end
        object LabelMultisampling: TLabel
          Left = 22
          Top = 71
          Width = 88
          Height = 13
          Caption = 'LabelMultisampling'
        end
        object LabelAnimation3dFPS: TLabel
          Left = 16
          Top = 86
          Width = 102
          Height = 13
          Caption = 'LabelAnimation3dFPS'
        end
        object LabelAnimation3dSmoothTime: TLabel
          Left = 6
          Top = 127
          Width = 74
          Height = 40
          AutoSize = False
          Caption = 'LabelAnimation3dSmoothTime'
          WordWrap = True
        end
        object CbFullScreen: TCheckBox
          Left = 79
          Top = 96
          Width = 174
          Height = 17
          Caption = 'CbFullScreen'
          TabOrder = 0
        end
        object CbClearScreen: TCheckBox
          Left = 79
          Top = 111
          Width = 174
          Height = 17
          Caption = 'CbClearScreen'
          TabOrder = 1
        end
        object CbVSync: TCheckBox
          Left = 79
          Top = 126
          Width = 174
          Height = 17
          Caption = 'CbVSync'
          TabOrder = 2
        end
        object CbAlwaysOnTop: TCheckBox
          Left = 79
          Top = 142
          Width = 174
          Height = 17
          Caption = 'CbAlwaysOnTop'
          TabOrder = 3
        end
        object SeLight: TCSpinEdit
          Left = 7
          Top = 40
          Width = 73
          Height = 22
          MaxValue = 50
          TabOrder = 4
        end
        object SeSprites: TCSpinEdit
          Left = 86
          Top = 40
          Width = 73
          Height = 22
          MaxValue = 1000
          MinValue = 1
          TabOrder = 5
          Value = 1
        end
        object SeTexture: TCSpinEdit
          Left = 165
          Top = 40
          Width = 73
          Height = 22
          Increment = 128
          MaxValue = 8192
          MinValue = 128
          TabOrder = 6
          Value = 128
        end
        object CbSoftwareSkinning: TCheckBox
          Left = 79
          Top = 158
          Width = 174
          Height = 17
          Caption = 'CbSoftwareSkinning'
          TabOrder = 7
        end
        object CbMultisampling: TComboBox
          Left = 127
          Top = 68
          Width = 111
          Height = 21
          Style = csDropDownList
          ItemHeight = 13
          TabOrder = 8
          Items.Strings = (
            'Auto'
            'Disable'
            'NonMaskable'
            '2x'
            '3x'
            '4x'
            '5x'
            '6x'
            '7x'
            '8x'
            '9x'
            '10x'
            '11x'
            '12x'
            '13x'
            '14x'
            '15x'
            '16x')
        end
        object SeAnimation3dFPS: TCSpinEdit
          Left = 8
          Top = 103
          Width = 65
          Height = 22
          MaxValue = 1000
          TabOrder = 9
        end
        object SeAnimation3dSmoothTime: TCSpinEdit
          Left = 8
          Top = 155
          Width = 65
          Height = 22
          MaxValue = 10000
          TabOrder = 10
        end
      end
    end
    object TabSound: TTabSheet
      Caption = 'TabSound'
      ImageIndex = 4
      object GbSoundVolume: TGroupBox
        Left = 3
        Top = 3
        Width = 246
        Height = 134
        Caption = 'GbSoundVolume'
        TabOrder = 0
        object LabelMusicVolume: TLabel
          Left = 16
          Top = 16
          Width = 85
          Height = 13
          Caption = 'LabelMusicVolume'
        end
        object LabelSoundVolume: TLabel
          Left = 16
          Top = 75
          Width = 89
          Height = 13
          Caption = 'LabelSoundVolume'
        end
        object TbMusicVolume: TTrackBar
          Left = 3
          Top = 32
          Width = 240
          Height = 30
          Max = 100
          Frequency = 10
          TabOrder = 0
        end
        object TbSoundVolume: TTrackBar
          Left = 3
          Top = 91
          Width = 240
          Height = 30
          Max = 100
          Frequency = 10
          TabOrder = 1
        end
      end
      object GbSoundOther: TGroupBox
        Left = 3
        Top = 143
        Width = 246
        Height = 42
        Caption = 'GbSoundOther'
        TabOrder = 1
        object CbGlobalSound: TCheckBox
          Left = 8
          Top = 16
          Width = 235
          Height = 17
          Caption = 'CbGlobalSound'
          TabOrder = 0
        end
      end
    end
  end
  object BtnExecute: TButton
    Left = 8
    Top = 281
    Width = 80
    Height = 29
    Caption = 'BtnExecute'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -13
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    TabOrder = 3
    OnClick = BtnExecuteClick
  end
end
