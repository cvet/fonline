object frmObject: TfrmObject
  Left = 0
  Top = 0
  Width = 236
  Height = 616
  HorzScrollBar.Visible = False
  VertScrollBar.Visible = False
  TabOrder = 0
  TabStop = True
  object Shape1: TShape
    Left = 0
    Top = 0
    Width = 236
    Height = 616
    Align = alClient
    Brush.Color = clBlack
    Pen.Color = 7904687
    Pen.Width = 3
  end
  object Label1: TLabel
    Left = 7
    Top = 8
    Width = 94
    Height = 13
    Caption = #1053#1086#1084#1077#1088' '#1086#1073#1098#1077#1082#1090#1072':'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object lbID: TLabel
    Left = 119
    Top = 6
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
  object Label2: TLabel
    Left = 7
    Top = 22
    Width = 26
    Height = 13
    Caption = #1058#1080#1087': '
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object lbType: TLabel
    Left = 39
    Top = 22
    Width = 28
    Height = 13
    Caption = 'none'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object Label3: TLabel
    Left = 103
    Top = 22
    Width = 50
    Height = 13
    Caption = #1055#1086#1076#1090#1080#1087': '
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object lbSubType: TLabel
    Left = 159
    Top = 22
    Width = 28
    Height = 13
    Caption = 'none'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object Label4: TLabel
    Left = 7
    Top = 38
    Width = 61
    Height = 13
    Caption = #1053#1072#1079#1074#1072#1085#1080#1077': '
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object Label5: TLabel
    Left = 7
    Top = 62
    Width = 54
    Height = 13
    Caption = #1054#1087#1080#1089#1072#1085#1080#1077
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object lbName: TLabel
    Left = 72
    Top = 38
    Width = 153
    Height = 28
    AutoSize = False
    Caption = 'none '
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
    WordWrap = True
  end
  object Label7: TLabel
    Left = 145
    Top = 62
    Width = 80
    Height = 13
    Caption = #1048#1079#1086#1073#1088#1072#1078#1077#1085#1080#1077
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -11
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
    Transparent = True
  end
  object sbOk: TSpeedButton
    Left = 165
    Top = 591
    Width = 60
    Height = 22
    Caption = #1057#1086#1093#1088#1072#1085#1080#1090#1100
    Enabled = False
    OnClick = sbOkClick
  end
  object sbCancel: TSpeedButton
    Left = 99
    Top = 591
    Width = 60
    Height = 22
    Caption = #1054#1090#1084#1077#1085#1072
    Enabled = False
    OnClick = sbCancelClick
  end
  object mmDesc: TMemo
    Left = 6
    Top = 80
    Width = 134
    Height = 88
    Color = clScrollBar
    Lines.Strings = (
      #1053#1077#1090' '#1086#1087#1080#1089#1072#1085#1080#1103)
    TabOrder = 0
  end
  object lwParams: TListView
    Left = 5
    Top = 192
    Width = 225
    Height = 393
    Columns = <
      item
        Caption = #1053#1072#1079#1074#1072#1085#1080#1077
        Width = 135
      end
      item
        Caption = #1047#1085#1072#1095#1077#1085#1080#1077
        Width = 72
      end>
    ColumnClick = False
    GridLines = True
    RowSelect = True
    TabOrder = 1
    ViewStyle = vsReport
    OnClick = lwParamsClick
    OnSelectItem = lwParamsSelectItem
  end
  object rbFlags: TRadioButton
    Left = 128
    Top = 174
    Width = 65
    Height = 17
    Caption = #1060#1083#1072#1075#1080
    Color = clNone
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindow
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    ParentColor = False
    ParentFont = False
    TabOrder = 2
    OnClick = rbFlagsClick
  end
  object rbProps: TRadioButton
    Left = 40
    Top = 174
    Width = 81
    Height = 17
    Caption = #1057#1074#1086#1081#1089#1090#1074#1072
    Checked = True
    Color = clNone
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindow
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    ParentColor = False
    ParentFont = False
    TabOrder = 3
    TabStop = True
    OnClick = rbFlagsClick
  end
  object imgObj: TPanel
    Left = 144
    Top = 80
    Width = 85
    Height = 88
    BevelOuter = bvNone
    BorderStyle = bsSingle
    Color = clScrollBar
    TabOrder = 4
  end
  object txtEdit: TEdit
    Left = 3
    Top = 591
    Width = 92
    Height = 21
    TabOrder = 5
    OnKeyDown = txtEditKeyDown
  end
end
