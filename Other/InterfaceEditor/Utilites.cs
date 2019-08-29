using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Text.RegularExpressions;

namespace InterfaceEditor
{
	static public class Utilites
	{
		static public string GuiPath;
		static public string GuiResourcesPath;
		static public string GuiOutputPath;

		static public string MakeFileName(string fullPath)
		{
			if (!fullPath.StartsWith(GuiResourcesPath))
				return null;
			return fullPath.Substring(GuiResourcesPath.Length).Replace('\\', '/');
		}

		static public string MakeFullPath(string fileName)
		{
			return GuiResourcesPath + fileName.Replace('/', '\\');
		}

		static public string MakeValidIdentifierName(string name)
		{
			string invalidChars = Regex.Escape(new string(System.IO.Path.GetInvalidFileNameChars())) + " ";
			string invalidRegStr = string.Format(@"([{0}]*\.+$)|([{0}]+)", invalidChars);
			return Regex.Replace(name, invalidRegStr, "");
		}

		static public float Round(float value)
		{
			return (float)Math.Round((double)value);
		}
	}
}
