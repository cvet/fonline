using System;
using System.Collections.Generic;
using System.Text;
using System.ComponentModel;
using System.Drawing.Design;

namespace InterfaceEditor
{
	class GUIItemView : GUIGrid
	{
		static public string[] ItemsCollectionNames = new string[]
		{
			"ITEMS_INVENTORY",
			"ITEMS_USE",
			"ITEMS_BARTER",
			"ITEMS_BARTER_OFFER",
			"ITEMS_BARTER_OPPONENT",
			"ITEMS_BARTER_OPPONENT_OFFER",
			"ITEMS_PICKUP",
			"ITEMS_PICKUP_FROM",
			"ITEMS_CHOSEN_ALL",
		};

		[Editor(typeof(ListValueEditor), typeof(UITypeEditor))]
		public string ItemsCollection { get; set; }
		public string UserData { get; set; }
		public bool UseSorting { get; set; }

		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnCheckItem { get; set; }

		public GUIItemView(GUIObject parent)
			: base(parent)
		{
		}
	}
}
