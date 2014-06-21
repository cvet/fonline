using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Drawing;
using System.ComponentModel;
using System.Drawing.Design;

namespace InterfaceEditor
{
	class GUIButton : GUIImage
	{
		protected string _PressedImageName;
		protected Image _PressedImageContent;
		[Editor(typeof(ImageLocationEditor), typeof(UITypeEditor))]
		public string PressedImage
		{
			get
			{
				return _PressedImageName;
			}
			set
			{
				_PressedImageName = value;
				SetImage(ref _PressedImageName, ref _PressedImageContent);
			}
		}

		protected ImageLayout _PressedImageLayout;
		public ImageLayout PressedImageLayout
		{
			get
			{
				return _PressedImageLayout;
			}
			set
			{
				_PressedImageLayout = value;
			}
		}

		protected string _HoverImageName;
		protected Image _HoverImageContent;
		[Editor(typeof(ImageLocationEditor), typeof(UITypeEditor))]
		public string HoverImage
		{
			get
			{
				return _HoverImageName;
			}
			set
			{
				_HoverImageName = value;
				SetImage(ref _HoverImageName, ref _HoverImageContent);
			}
		}

		protected ImageLayout _HoverImageLayout;
		public ImageLayout HoverImageLayout
		{
			get
			{
				return _HoverImageLayout;
			}
			set
			{
				_HoverImageLayout = value;
			}
		}

		public GUIButton(GUIObject parent)
			: base(parent)
		{
		}

		public override void Draw(Graphics g)
		{
			base.Draw(g);

			if (_PressedImageContent != null)
				g.DrawImage(_PressedImageContent, new Rectangle(AbsolutePosition, Size));
		}
	}
}
