using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;
using System.Drawing.Design;
using System.Drawing;

namespace InterfaceEditor
{
	class GUIGrid : GUIPanel
	{
		public string CellPrototype { get; set; }
		public string GridSize { get; set; }
		public int Columns { get; set; }
		public Size Padding { get; set; }

		public GUIGrid(GUIObject parent)
			: base(parent)
		{
		}
	}
}
