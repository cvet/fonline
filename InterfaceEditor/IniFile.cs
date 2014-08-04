using System;
using System.Runtime.InteropServices;
using System.Text;
using System.IO;
using System.Windows.Forms;

namespace InterfaceEditor
{
	public class IniFile
	{
		public string Path;

		public IniFile(string path)
		{
			Path = path;
		}

		public string IniReadValue(string section, string key, string defaultValue)
		{
			string ini = File.ReadAllText(Path);

			int startIndex = ini.IndexOf(key);
			if (startIndex == -1)
				return defaultValue;

			int endIndex = ini.IndexOf('\n', startIndex);
			if (endIndex == -1)
				endIndex = ini.Length;

			startIndex += key.Length;
			string value = ini.Substring(startIndex, endIndex - startIndex);
			value = value.Trim();
			if (value[0] != '=')
				return defaultValue;

			return value.Substring(1).Trim();
		}
	}
}
