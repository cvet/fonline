using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;
using System.Drawing.Design;

namespace InterfaceEditor
{
	class GUICheckBox : GUIButton
	{
		public string IsChecked { get; set; }

		public GUICheckBox(GUIObject parent)
			: base(parent)
		{
		}

		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnCheckedChanged { get; set; }
	}
}
