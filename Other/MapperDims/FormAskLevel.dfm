object frmSelLevel: TfrmSelLevel
  Left = 348
  Top = 283
  BorderIcons = [biSystemMenu]
  BorderStyle = bsDialog
  Caption = 'Выберите уровень'
  ClientHeight = 107
  ClientWidth = 192
  Color = 15985129
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  FormStyle = fsStayOnTop
  OldCreateOrder = False
  Position = poOwnerFormCenter
  PixelsPerInch = 96
  TextHeight = 13
  object Label1: TLabel
    Left = 8
    Top = 8
    Width = 177
    Height = 57
    Alignment = taCenter
    AutoSize = False
    Caption = 'Label1'
    WordWrap = True
  end
  object Shape1: TShape
    Left = 0
    Top = 0
    Width = 192
    Height = 107
    Align = alClient
    Brush.Style = bsClear
    Pen.Color = 8086883
    Pen.Width = 2
  end
  object Button1: TButton
    Left = 88
    Top = 80
    Width = 75
    Height = 19
    Caption = 'Открыть'
    TabOrder = 0
    OnClick = Button1Click
  end
  object UpDown1: TUpDown
    Left = 65
    Top = 80
    Width = 12
    Height = 21
    Associate = Edit1
    Min = 0
    Position = 0
    TabOrder = 1
    Wrap = False
  end
  object Edit1: TEdit
    Left = 32
    Top = 80
    Width = 33
    Height = 21
    TabOrder = 2
    Text = '0'
  end
end
