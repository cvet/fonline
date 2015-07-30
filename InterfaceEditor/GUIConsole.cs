using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InterfaceEditor
{
	class GUIConsole : GUITextInput
	{
		public bool DisableDeactivation { get; set; }
		public string HistoryStorageName { get; set; }
		public string HistoryMaxLength { get; set; }

		public GUIConsole(GUIObject parent)
			: base(parent)
		{
		}
	}
}
