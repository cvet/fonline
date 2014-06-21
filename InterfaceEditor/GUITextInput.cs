using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InterfaceEditor
{
	class GUITextInput : GUIText
	{
		public string InputLength { get; set; }
		public bool Password { get; set; }

		public GUITextInput(GUIObject parent)
			: base(parent)
		{
		}
	}
}
