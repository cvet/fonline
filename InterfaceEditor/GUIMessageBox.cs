using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InterfaceEditor
{
	class GUIMessageBox : GUIText
	{
		public string InvertMessages { get; set; }
		public string Filters { get; set; }

		public GUIMessageBox(GUIObject parent)
			: base(parent)
		{
		}
	}
}
