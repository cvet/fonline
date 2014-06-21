using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace InterfaceEditor
{
	class ScriptWriter
	{
		private StringBuilder _Script;
		private string _BaseIdent;

		public string Write(GUIScreen root, string ident)
		{
			_Script = new StringBuilder(100000);
			_BaseIdent = ident;

			ProcessObject(root);

			_Script.AppendLine();
			_Script.AppendLine(_BaseIdent + "void Init( int screenIndex )");
			_Script.AppendLine(_BaseIdent + "{");
			ProcessObjectCreation(root);
			_Script.AppendLine(_BaseIdent + "}");

			return _Script.ToString();
		}

		private void ProcessObject(GUIObject obj)
		{
			WriteClass(obj);
			foreach (GUIObject child in obj.Children)
				ProcessObject(child);
		}

		private void ProcessObjectCreation(GUIObject obj)
		{
			WriteClassCreation(obj);
			foreach (GUIObject child in obj.Children)
				ProcessObjectCreation(child);
		}

		private void WriteClass(GUIObject obj)
		{
			string className = MakeClassName(obj.GetFullName());
			GUIScreen root = (obj is GUIScreen ? (GUIScreen)obj : null);

			// Global scope
			if (root == null)
				_Script.AppendLine();
			if (!string.IsNullOrEmpty(obj.GlobalScope))
			{
				AppendCode(obj.GlobalScope, _BaseIdent);
				_Script.AppendLine();
			}

			// Class name
			_Script.AppendLine(_BaseIdent + "class " + className + " : " + obj.GetType().ToString().Substring(obj.GetType().ToString().IndexOf("GUI")));
			_Script.AppendLine(_BaseIdent + "{");

			// Constructor
			_Script.AppendLine(_BaseIdent + "    void OnInit() override");
			_Script.AppendLine(_BaseIdent + "    {");
			if (root != null)
			{
				if (root.IsModal)
					_Script.AppendLine(_BaseIdent + "        SetModal( true );");
				if (root.IsMultiinstance)
					_Script.AppendLine(_BaseIdent + "        SetMultiinstance( true );");
				if (root.IsCloseOnMiss)
					_Script.AppendLine(_BaseIdent + "        SetCloseOnMiss( true );");
				if (root.IsAutoCursor)
					_Script.AppendLine(_BaseIdent + "        SetAutoCursor( " + root.AutoCursorType + " );");
			}

			if (!obj.Active)
				_Script.AppendLine(_BaseIdent + "        SetActive( false );");
			_Script.AppendLine(_BaseIdent + "        SetName( \"" + obj.GetFullName() + "\" );");
			_Script.AppendLine(_BaseIdent + "        SetPosition( " + obj.Position.X + ", " + obj.Position.Y + " );");
			_Script.AppendLine(_BaseIdent + "        SetSize( " + obj.Size.Width + ", " + obj.Size.Height + " );");
			if (obj.Anchor != AnchorStyles.None)
				_Script.AppendLine(_BaseIdent + "        SetAnchor( " + ConvertAnchorStyles(obj.Anchor) + " );");
			if (obj.Dock != DockStyle.None)
				_Script.AppendLine(_BaseIdent + "        SetDock( " + ConvertDockStyle(obj.Dock) + " );");

			if (obj is GUIImage)
			{
				GUIImage image = (GUIImage)obj;
				if (!string.IsNullOrEmpty(image.BackgroundImage))
				{
					if (image.BackgroundImageLayout != ImageLayout.None)
						_Script.AppendLine(_BaseIdent + "        SetBackgroundImage( \"" + image.BackgroundImage + "\", " + ConvertImageLayout(image.BackgroundImageLayout) + " );");
					else
						_Script.AppendLine(_BaseIdent + "        SetBackgroundImage( \"" + image.BackgroundImage + "\" );");
				}
			}
			if (obj is GUIPanel)
			{
				GUIPanel panel = (GUIPanel)obj;
				if (panel.IsCanMove)
					_Script.AppendLine(_BaseIdent + "        SetCanMove( true, " + panel.IsMoveIgnoreBorders.ToString().ToLower() + " );");
			}
			if (obj is GUIButton)
			{
				GUIButton button = (GUIButton)obj;
				if (!string.IsNullOrEmpty(button.PressedImage))
				{
					if (button.PressedImageLayout != ImageLayout.None)
						_Script.AppendLine(_BaseIdent + "        SetPressedImage( \"" + button.PressedImage + "\", " + ConvertImageLayout(button.PressedImageLayout) + " );");
					else
						_Script.AppendLine(_BaseIdent + "        SetPressedImage( \"" + button.PressedImage + "\" );");
				}
				if (!string.IsNullOrEmpty(button.HoverImage))
				{
					if (button.HoverImageLayout != ImageLayout.None)
						_Script.AppendLine(_BaseIdent + "        SetHoverImage( \"" + button.HoverImage + "\", " + ConvertImageLayout(button.HoverImageLayout) + " );");
					else
						_Script.AppendLine(_BaseIdent + "        SetHoverImage( \"" + button.HoverImage + "\" );");
				}
			}
			if (obj is GUIText)
			{
				GUIText text = (GUIText)obj;
				string textStr = (!string.IsNullOrEmpty(text.Text) ? text.Text : "\"\"");
				string font = (!string.IsNullOrEmpty(text.Font) ? text.Font : "FONT_DEFAULT");
				_Script.AppendLine(_BaseIdent + "        SetText( " + textStr + ", " + font + ", " + ConvertTextStyle(text.Style) + " );");

				if (!string.IsNullOrEmpty(text.NormalColor))
					_Script.AppendLine(_BaseIdent + "        SetTextColor( " + text.NormalColor + " );");
				if (!string.IsNullOrEmpty(text.FocusedColor))
					_Script.AppendLine(_BaseIdent + "        SetTextFocusedColor( " + text.FocusedColor + " );");
			}
			if (obj is GUITextInput)
			{
				GUITextInput textInput = (GUITextInput)obj;
				if (!string.IsNullOrEmpty(textInput.InputLength))
					_Script.AppendLine(_BaseIdent + "        SetInputLength( " + textInput.InputLength + " );");
				if (textInput.Password)
					_Script.AppendLine(_BaseIdent + "        SetInputPassword( \"#\" );");
			}
			if (obj is GUIMessageBox)
			{
				GUIMessageBox messageBox = (GUIMessageBox)obj;
				if (!string.IsNullOrEmpty(messageBox.InvertMessages))
					_Script.AppendLine(_BaseIdent + "        SetInvertMessages( " + messageBox.InvertMessages + " );");
				if (!string.IsNullOrEmpty(messageBox.Filters))
					_Script.AppendLine(_BaseIdent + "        SetFilters( " + messageBox.Filters + " );");
			}
			if (obj is GUIConsole)
			{
				GUIConsole console = (GUIConsole)obj;
				if (!string.IsNullOrEmpty(console.HistoryStorageName))
					_Script.AppendLine(_BaseIdent + "        SetHistoryStorage( " + console.HistoryStorageName + " );");
				if (!string.IsNullOrEmpty(console.HistoryMaxLength))
					_Script.AppendLine(_BaseIdent + "        SetHistoryMaxLength( " + console.HistoryMaxLength + " );");
			}

			if(!string.IsNullOrEmpty(obj.OnInit))
			{
				_Script.AppendLine();
				AppendCode(obj.OnInit, _BaseIdent + "        ");
			}
			_Script.AppendLine(_BaseIdent + "    }");

			// Class fields
			if (!string.IsNullOrEmpty(obj.ClassFields))
			{
				_Script.AppendLine();
				AppendCode(obj.ClassFields, _BaseIdent + "    ");
			}

			// Callbacks
			WriteClassFunction("void OnShow( dictionary@ params ) override", obj.OnShow);
			WriteClassFunction("void OnHide() override", obj.OnHide);
			WriteClassFunction("void OnDraw() override", obj.OnDraw);
			WriteClassFunction("void OnMove( int deltaX, int deltaY ) override", obj.OnMove);
			WriteClassFunction("void OnMouseDown( int button ) override", obj.OnMouseDown);
			WriteClassFunction("void OnMouseUp( int button, bool lost ) override", obj.OnMouseUp);
			WriteClassFunction("void OnMousePressed( int button ) override", obj.OnMousePressed);
			WriteClassFunction("void OnMouseClick( int button ) override", obj.OnMouseClick);
			WriteClassFunction("void OnLMouseClick() override", obj.OnLMouseClick);
			WriteClassFunction("void OnRMouseClick() override", obj.OnRMouseClick);
			WriteClassFunction("void OnInput( uint8 key, string@ text ) override", obj.OnInput);

			// Text
			if (obj is GUIText)
			{
				GUIText text = (GUIText)obj;
				if (!string.IsNullOrEmpty(text.DynamicText))
				{
					_Script.AppendLine();
					_Script.AppendLine(_BaseIdent + "    string@ get_Text() override");
					_Script.AppendLine(_BaseIdent + "    {");
					AppendCode(text.DynamicText, _BaseIdent + "        ");
					_Script.AppendLine(_BaseIdent + "    }");
				}
			}

			// Close
			_Script.AppendLine(_BaseIdent + "}");
		}

		private void WriteClassFunction(string methodDecl, string code)
		{
			if (!string.IsNullOrEmpty(code))
			{
				_Script.AppendLine();
				_Script.AppendLine(_BaseIdent + "    " + methodDecl);
				_Script.AppendLine(_BaseIdent + "    {");
				AppendCode(code, _BaseIdent + "        ");
				_Script.AppendLine(_BaseIdent + "    }");
			}
		}

		private void WriteClassCreation(GUIObject obj)
		{
			string className = MakeClassName(obj.GetFullName());
			string instanceName = className.Substring(0, 1).ToLower() + className.Substring(1);
			GUIScreen root = (obj is GUIScreen ? (GUIScreen)obj : null);
			string identPrefix = _BaseIdent + "    " + instanceName;

			if (root != null)
			{
				_Script.AppendLine(_BaseIdent + "    " + className + " " + instanceName + "();");
				_Script.AppendLine(_BaseIdent + "    " + instanceName + ".Init( null );");
				_Script.AppendLine(_BaseIdent + "    GUI_RegisterScreen( screenIndex, " + instanceName + " );");
			}
			else
			{
				string parentClassName = MakeClassName(obj.GetParent().GetFullName());
				string parentInstanceName = parentClassName.Substring(0, 1).ToLower() + parentClassName.Substring(1);
				_Script.AppendLine();
				_Script.AppendLine(_BaseIdent + "    " + className + " " + instanceName + "();");
				_Script.AppendLine(_BaseIdent + "    " + instanceName + ".Init( " + parentInstanceName + " );");
			}
		}

		private void AppendCode(string code, string ident)
		{
			code = ident + code.Replace(Environment.NewLine, Environment.NewLine + ident);
			if (!code.EndsWith(Environment.NewLine))
				code += Environment.NewLine;
			_Script.Append(code);
		}

		private string MakeClassName(string name)
		{
			return Utilites.MakeValidIdentifierName(name);
		}

		private string ConvertTextStyle(GUIText.TextStyle style)
		{
			string result = "";
			if ((style & GUIText.TextStyle.NoBreak) != 0)
				result += "FT_NOBREAK | ";
			if ((style & GUIText.TextStyle.OneLine) != 0)
				result += "FT_NOBREAK_LINE | ";
			if ((style & GUIText.TextStyle.CenterX) != 0)
				result += "FT_CENTERX | ";
			if ((style & GUIText.TextStyle.CenterY) != 0)
				result += "FT_CENTERY | ";
			if ((style & GUIText.TextStyle.Right) != 0)
				result += "FT_CENTERR | ";
			if ((style & GUIText.TextStyle.Bottom) != 0)
				result += "FT_BOTTOM | ";
			if ((style & GUIText.TextStyle.Upper) != 0)
				result += "FT_UPPER | ";
			if ((style & GUIText.TextStyle.NoColorize) != 0)
				result += "FT_NO_COLORIZE | ";
			if ((style & GUIText.TextStyle.Align) != 0)
				result += "FT_ALIGN | ";
			if ((style & GUIText.TextStyle.Bordered) != 0)
				result += "FT_BORDERED | ";
			if (result == "")
				result = "0";
			else
				result = result.Remove(result.Length - 3);
			return result;
		}

		private string ConvertAnchorStyles(AnchorStyles styles)
		{
			string result = "";
			if (styles.HasFlag(AnchorStyles.Left))
				result += (result.Length > 0 ? "| " : "") + "ANCHOR_LEFT";
			if (styles.HasFlag(AnchorStyles.Right))
				result += (result.Length > 0 ? "| " : "") + "ANCHOR_RIGHT";
			if (styles.HasFlag(AnchorStyles.Top))
				result += (result.Length > 0 ? "| " : "") + "ANCHOR_TOP";
			if (styles.HasFlag(AnchorStyles.Bottom))
				result += (result.Length > 0 ? "| " : "") + "ANCHOR_BOTTOM";
			return result;
		}

		private string ConvertDockStyle(DockStyle style)
		{
			if (style == DockStyle.Left)
				return "DOCK_LEFT";
			if (style == DockStyle.Right)
				return "DOCK_RIGHT";
			if (style == DockStyle.Top)
				return "DOCK_TOP";
			if (style == DockStyle.Bottom)
				return "DOCK_BOTTOM";
			if (style == DockStyle.Fill)
				return "DOCK_FILL";
			return "";
		}

		private string ConvertImageLayout(ImageLayout layout)
		{
			if (layout == ImageLayout.Tile)
				return "IMAGE_LAYOUT_TILE";
			if (layout == ImageLayout.Center)
				return "IMAGE_LAYOUT_CENTER";
			if (layout == ImageLayout.Stretch)
				return "IMAGE_LAYOUT_STRETCH";
			if (layout == ImageLayout.Zoom)
				return "IMAGE_LAYOUT_ZOOM";
			return "IMAGE_LAYOUT_NONE";
		}
	}
}
