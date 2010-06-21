object fmChange: TfmChange
  Left = 402
  Top = 308
  BorderIcons = []
  BorderStyle = bsToolWindow
  Caption = 'Change items count'
  ClientHeight = 59
  ClientWidth = 181
  Color = clGrayText
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poOwnerFormCenter
  OnCloseQuery = FormCloseQuery
  PixelsPerInch = 96
  TextHeight = 13
  object UpDown1: TUpDown
    Left = 81
    Top = 13
    Width = 16
    Height = 21
    Associate = edValue
    Min = 1
    Max = 32767
    Position = 1
    TabOrder = 1
    Thousands = False
    Wrap = False
  end
  object btnOK: TButton
    Left = 103
    Top = 2
    Width = 75
    Height = 25
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 2
  end
  object btnCancel: TButton
    Left = 104
    Top = 29
    Width = 75
    Height = 25
    Cancel = True
    Caption = '&Cancel'
    ModalResult = 2
    TabOrder = 3
  end
  object edValue: TEdit
    Left = 8
    Top = 13
    Width = 73
    Height = 21
    Color = clBlack
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clLime
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    MaxLength = 5
    ParentFont = False
    TabOrder = 0
    Text = '1'
  end
end
