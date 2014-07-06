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

		protected StringAlignment _VerticalAlignment;
		public StringAlignment VerticalAlignment
		{
			get
			{
				return _VerticalAlignment;
			}
			set
			{
				_VerticalAlignment = value;
				RequestRedraw();
			}
		}
		protected StringAlignment _HorisontalAlignment;
		public StringAlignment HorisontalAlignment
		{
			get
			{
				return _HorisontalAlignment;
			}
			set
			{
				_HorisontalAlignment = value;
				RequestRedraw();
			}
		}

		public bool Align { get; set; }
		public bool NoColorize { get; set; }
		public bool Bordered { get; set; }
		public bool DrawFromBottom { get; set; }
		public string NormalColor { get; set; }
		public string FocusedColor { get; set; }

		public GUIText(GUIObject parent)
			: base(parent)
		{
		}

		public override void DrawPass1(Graphics g)
		{
			if (!string.IsNullOrEmpty(Text) || !string.IsNullOrEmpty(DynamicText))
			{
				RectangleF r = new RectangleF(AbsolutePosition, Size);
				StringFormat format = new StringFormat();
				format.LineAlignment = VerticalAlignment;
				format.Alignment = HorisontalAlignment;
				if (!string.IsNullOrEmpty(DynamicText))
					g.DrawString("DynamicText", new Font("Courier New", 8f), new SolidBrush(Color.Green), r, format);
				else
					g.DrawString("Text", new Font("Courier New", 8f), new SolidBrush(Color.Green), r, format);
			}

			base.DrawPass1(g);
		}
	}
}
