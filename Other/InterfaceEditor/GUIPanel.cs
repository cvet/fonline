using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Drawing;
using System.Drawing.Design;
using System.ComponentModel;
using System.IO;

namespace InterfaceEditor
{
	class GUIPanel : GUIObject
	{
		protected string _BackgroundImageName;
		protected Image _BackgroundImageContent;
		[Editor(typeof(ImageLocationEditor), typeof(UITypeEditor))]
		public string BackgroundImage
		{
			get
			{
				return _BackgroundImageName;
			}
			set
			{
				_BackgroundImageName = value;
				SetImage(_BackgroundImageName, ref _BackgroundImageContent);
			}
		}

		protected ImageLayout _BackgroundImageLayout;
		public ImageLayout BackgroundImageLayout
		{
			get
			{
				return _BackgroundImageLayout;
			}
			set
			{
				_BackgroundImageLayout = value;
				RequestRedraw();
			}
		}

		public bool CropContent { get; set; }
		public bool IsVerticalScroll { get; set; }
		public bool IsHorizontalScroll { get; set; }

		public GUIPanel(GUIObject parent)
			: base(parent)
		{
		}

		override public bool IsAutoSize()
		{
			return _BackgroundImageLayout == ImageLayout.None;
		}
		override public void DoAutoSize()
		{
			Size = Size.Empty;
			if (_BackgroundImageContent != null)
				Size = _BackgroundImageContent.Size;
		}
		public void SetBackgroundSize()
		{
			if (_BackgroundImageContent != null)
				Size = _BackgroundImageContent.Size;
		}

		protected void SetImage(string name, ref Image content)
		{
			content = null;
			if (!string.IsNullOrEmpty(name))
			{
				try
				{
					content = Image.FromFile(Utilites.MakeFullPath(name));
				}
				catch (FileNotFoundException e)
				{
					Log.Write(e);
				}
				catch (Exception)
				{
				}
			}

			// Set size
			DoAutoSize();

			RequestRedraw();
		}

		public override void DrawPass1(Graphics g)
		{
			if (_BackgroundImageContent != null)
				DrawImage(g, _BackgroundImageContent, _BackgroundImageLayout);

			base.DrawPass1(g);
		}

		public void DrawImage(Graphics g, Image image, ImageLayout layout)
		{
			switch (layout)
			{
				case ImageLayout.Tile:
					{
						Rectangle r = new Rectangle(AbsolutePosition, Size);
						g.DrawImage(image, r);

						StringFormat format = new StringFormat();
						format.LineAlignment = StringAlignment.Center;
						format.Alignment = StringAlignment.Center;
						g.DrawString("TILED", new Font("Courier New", 8f), new SolidBrush(Color.Red), r, format);
					}
					break;
				case ImageLayout.Center:
					{
						float w = (float)image.Width;
						float h = (float)image.Height;
						float x = (float)AbsolutePosition.X + Utilites.Round(((float)Size.Width - w) / 2.0f);
						float y = (float)AbsolutePosition.Y + Utilites.Round(((float)Size.Height - h) / 2.0f);
						g.DrawImage(image, x, y, w, h);
					}
					break;
				case ImageLayout.Stretch:
					{
						g.DrawImage(image, new Rectangle(AbsolutePosition, Size));
					}
					break;
				case ImageLayout.Zoom:
					{
						float k = Math.Min((float)Size.Width / (float)image.Width, (float)Size.Height / (float)image.Height);
						float w = Utilites.Round((float)image.Width * k);
						float h = Utilites.Round((float)image.Height * k);
						float x = (float)AbsolutePosition.X + Utilites.Round(((float)Size.Width - w) / 2.0f);
						float y = (float)AbsolutePosition.Y + Utilites.Round(((float)Size.Height - h) / 2.0f);
						g.DrawImage(image, x, y, w, h);
					}
					break;
				default:
					{
						g.DrawImage(image, new Rectangle(AbsolutePosition, image.Size));
					}
					break;
			}
		}
	}
}
