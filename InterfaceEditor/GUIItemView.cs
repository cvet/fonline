using System;
using System.Collections.Generic;
using System.Text;
using System.ComponentModel;
using System.Drawing.Design;

namespace InterfaceEditor
{
	class GUIItemView : GUIGrid
	{
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnCheckItem { get; set; }

		public GUIItemView(GUIObject parent)
			: base(parent)
		{
		}
	}
}
