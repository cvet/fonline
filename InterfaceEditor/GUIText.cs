using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Drawing;
using System.ComponentModel;
using System.Drawing.Design;

namespace InterfaceEditor
{
	class GUIText : GUIObject
	{
		static public string[] FontNames = new string[]
		{
			"FONT_OLD_FO",
			"FONT_NUM",
			"FONT_BIG_NUM",
			"FONT_SAND_NUM",
			"FONT_SPECIAL",
			"FONT_FALLOUT",
			"FONT_THIN",
			"FONT_FAT",
			"FONT_BIG",
		};

		[Flags]
		public enum TextStyle
		{
			Default = CenterX | CenterY,
			NoBreak = 0x0001,
			OneLine = 0x0002,
			CenterX = 0x0004,
			CenterY = 0x0008,
			Right = 0x0010,
			Bottom = 0x0020,
			Upper = 0x0040,
			NoColorize = 0x0080,
			Align = 0x0100,
			Bordered = 0x0200,
		}

		protected string _Text;
		public string Text
		{
			get
			{
				return _Text;
			}
			set
			{
				_Text = value;
				RequestRedraw();
			}
		}

		protected string _DynamicText;
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string DynamicText
		{
			get
			{
				return _DynamicText;
			}
			set
			{
				_DynamicText = value;
				RequestRedraw();
			}
		}

		[Editor(typeof(ListValueEditor), typeof(UITypeEditor))]
		public string Font { get; set; }
		[Editor(typeof(FlagEnumUIEditor), typeof(UITypeEditor))]
		public TextStyle Style { get; set; }
		public string NormalColor { get; set; }
		public string FocusedColor { get; set; }

		public GUIText(GUIObject parent)
			: base(parent)
		{
		}

		public override void Draw(Graphics g)
		{
			base.Draw(g);

			if (!string.IsNullOrEmpty(Text) || !string.IsNullOrEmpty(DynamicText))
			{
				RectangleF r = new RectangleF(AbsolutePosition, Size);
				StringFormat format = new StringFormat();
				format.LineAlignment = StringAlignment.Center;
				format.Alignment = StringAlignment.Center;
				if (!string.IsNullOrEmpty(DynamicText))
					g.DrawString(DynamicText, new Font("Courier New", 8f), new SolidBrush(Color.Green), r, format);
				else
					g.DrawString(Text, new Font("Courier New", 8f), new SolidBrush(Color.Green), r, format);
			}
		}
	}
}
