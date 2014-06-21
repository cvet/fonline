using System;
using System.Runtime.InteropServices;
using System.Text;

namespace InterfaceEditor
{
	public class IniFile
	{
		public string Path;

		[DllImport("kernel32")]
		private static extern long WritePrivateProfileString(string section, string key, string val, string filePath);
		[DllImport("kernel32")]
		private static extern int GetPrivateProfileString(string section, string key, string def, StringBuilder retVal, int size, string filePath);

		public IniFile(string path)
		{
			Path = path;
		}

		public void IniWriteValue(string section, string key, string value)
		{
			WritePrivateProfileString(section, key, value, Path);
		}

		public string IniReadValue(string section, string key, string defaultValue)
		{
			StringBuilder temp = new StringBuilder(255);
			int i = GetPrivateProfileString(section, key, defaultValue, temp, 255, Path);
			return temp.ToString();
		}
	}
}
