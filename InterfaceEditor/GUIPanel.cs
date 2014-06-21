using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Drawing;
using System.Drawing.Design;
using System.ComponentModel;

namespace InterfaceEditor
{
	class GUIPanel : GUIImage
	{
		public bool IsCanMove { get; set; }
		public bool IsMoveIgnoreBorders { get; set; }

		public GUIPanel(GUIObject parent)
			: base(parent)
		{
		}
	}
}
