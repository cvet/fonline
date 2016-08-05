using System;
using System.Collections.Generic;
using System.Text;
using System.ComponentModel;
using System.Drawing.Design;

namespace InterfaceEditor
{
	class GUIItemView : GUIGrid
	{
		public string UserData { get; set; }
		public string UserDataExt { get; set; }
		public bool UseSorting { get; set; }

		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnGetItems { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnCheckItem { get; set; }

		public GUIItemView(GUIObject parent)
			: base(parent)
		{
		}
	}
}
