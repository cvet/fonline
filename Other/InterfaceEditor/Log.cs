using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace InterfaceEditor
{
	class Log
	{
		static private int _LogLine = 0;

		static public void Write(object obj)
		{
			_LogLine++;
			RichTextBox textBox = MainForm.Instance.LogText;
			textBox.Text = _LogLine + ") " + (obj != null ? obj.ToString() : "(null)") + "\n" + textBox.Text;
		}
	}
}
