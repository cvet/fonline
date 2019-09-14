using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Drawing.Design;
using System.ComponentModel;

namespace InterfaceEditor
{
	class GUIScreen : GUIPanel
	{
		public bool IsModal { get; set; }
		public bool IsMultiinstance { get; set; }
		public bool IsCloseOnMiss { get; set; }
		public string AvailableCursors { get; set; }
		public bool IsCanMove { get; set; }
		public bool IsMoveIgnoreBorders { get; set; }

		public GUIScreen(GUIPanel parent)
			: base(parent)
		{
			IsModal = true;
			IsCanMove = true;
		}
	}
}
