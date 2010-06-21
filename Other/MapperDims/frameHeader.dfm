object frmHdr: TfrmHdr
  Left = 0
  Top = 0
  Width = 236
  Height = 248
  TabOrder = 0
  TabStop = True
  object Shape10: TShape
    Left = 0
    Top = 0
    Width = 236
    Height = 248
    Align = alClient
    Brush.Color = clBlack
    Pen.Color = 7904687
    Pen.Width = 3
    ExplicitLeft = 135
    ExplicitTop = 112
  end
  object Label2: TLabel
    Left = 66
    Top = 3
    Width = 96
    Height = 13
    Caption = #1053#1072#1079#1074#1072#1085#1080#1077' '#1082#1072#1088#1090#1099
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object Label3: TLabel
    Left = 7
    Top = 166
    Width = 133
    Height = 13
    Caption = #1053#1072#1095#1072#1083#1100#1085#1086#1077' '#1087#1086#1083#1086#1078#1077#1085#1080#1077
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object Label4: TLabel
    Left = 9
    Top = 184
    Width = 9
    Height = 16
    Caption = 'X'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -13
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object Label5: TLabel
    Left = 57
    Top = 184
    Width = 9
    Height = 16
    Caption = 'Y'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -13
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object Label6: TLabel
    Left = 103
    Top = 184
    Width = 17
    Height = 16
    Caption = 'dir'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -13
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object Label1: TLabel
    Left = 8
    Top = 206
    Width = 100
    Height = 13
    Caption = #1042#1089#1077#1075#1086' '#1086#1073#1098#1077#1082#1090#1086#1074': '
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object lbTotalCount: TLabel
    Left = 112
    Top = 206
    Width = 7
    Height = 13
    Caption = '0'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object Label7: TLabel
    Left = 8
    Top = 226
    Width = 46
    Height = 13
    Caption = 'Map ID: '
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object Label8: TLabel
    Left = 165
    Top = 184
    Width = 27
    Height = 16
    Caption = 'Num'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -13
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object editStartX: TEdit
    Left = 24
    Top = 179
    Width = 27
    Height = 21
    MaxLength = 3
    TabOrder = 0
    Text = '0'
    OnChange = editStartXChange
    OnKeyPress = editStartXKeyPress
  end
  object editStartY: TEdit
    Left = 72
    Top = 179
    Width = 25
    Height = 21
    MaxLength = 3
    TabOrder = 1
    Text = '0'
    OnChange = editStartYChange
    OnKeyPress = editStartXKeyPress
  end
  object editStartDir: TEdit
    Left = 126
    Top = 179
    Width = 25
    Height = 21
    MaxLength = 1
    TabOrder = 2
    Text = '0'
    OnChange = editStartDirChange
    OnKeyPress = editStartXKeyPress
  end
  object editMapID: TEdit
    Left = 63
    Top = 222
    Width = 90
    Height = 21
    MaxLength = 10
    TabOrder = 3
    Text = '0'
    OnKeyPress = editMapIDKeyPress
  end
  object lbMaps: TListBox
    Left = 9
    Top = 21
    Width = 217
    Height = 121
    ItemHeight = 13
    TabOrder = 4
    OnClick = lbMapsClick
  end
  object btnClose: TButton
    Left = 168
    Top = 148
    Width = 59
    Height = 20
    Caption = #1047#1072#1082#1088#1099#1090#1100
    TabOrder = 5
    OnClick = btnCloseClick
  end
  object btnChange: TButton
    Left = 168
    Top = 220
    Width = 59
    Height = 20
    Caption = #1048#1079#1084#1077#1085#1080#1090#1100
    TabOrder = 6
    OnClick = btnChangeClick
  end
  object editNumSP: TEdit
    Left = 194
    Top = 179
    Width = 16
    Height = 21
    MaxLength = 3
    TabOrder = 7
    Text = '0'
    OnChange = editNumSPChange
    OnKeyPress = editStartXKeyPress
  end
  object udNumSP: TUpDown
    Left = 210
    Top = 178
    Width = 17
    Height = 25
    TabOrder = 8
    OnChanging = udNumSPChanging
  end
end
