using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Drawing;

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
			_Script.AppendLine(_BaseIdent + "    GUI_RegisterScreen( screenIndex, _" + root.Name + " );");
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
			string className = obj.Name;
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

			// Class fields
			if (!string.IsNullOrEmpty(obj.ClassFields))
			{
				AppendCode(obj.ClassFields, _BaseIdent + "    ");
				_Script.AppendLine();
			}

			// Constructor
			_Script.AppendLine(_BaseIdent + "    void OnConstruct() override");
			_Script.AppendLine(_BaseIdent + "    {");
			if (root != null)
			{
				if (root.IsModal)
					_Script.AppendLine(_BaseIdent + "        SetModal( true );");
				if (root.IsMultiinstance)
					_Script.AppendLine(_BaseIdent + "        SetMultiinstance( true );");
				if (root.IsCloseOnMiss)
					_Script.AppendLine(_BaseIdent + "        SetCloseOnMiss( true );");
				if (!string.IsNullOrEmpty(root.Cursor))
					_Script.AppendLine(_BaseIdent + "        SetCursor( " + root.Cursor + " );");
			}

			if (!obj.Active)
				_Script.AppendLine(_BaseIdent + "        SetActive( false );");
			if (!obj.Position.IsEmpty)
				_Script.AppendLine(_BaseIdent + "        SetPosition( " + obj.Position.X + ", " + obj.Position.Y + " );");
			if (!obj.Size.IsEmpty /*&& !obj.IsAutoSize()*/)
				_Script.AppendLine(_BaseIdent + "        SetSize( " + obj.Size.Width + ", " + obj.Size.Height + " );");
			if (obj.Anchor != AnchorStyles.None)
				_Script.AppendLine(_BaseIdent + "        SetAnchor( " + ConvertAnchorStyles(obj.Anchor) + " );");
			if (obj.Dock != DockStyle.None)
				_Script.AppendLine(_BaseIdent + "        SetDock( " + ConvertDockStyle(obj.Dock) + " );");
			if (obj.IsNotHittable)
				_Script.AppendLine(_BaseIdent + "        SetNotHittable( true );");
			if (obj.CheckTransparentOnHit)
				_Script.AppendLine(_BaseIdent + "        SetCheckTransparentOnHit( true );");

			if (obj is GUIPanel)
			{
				GUIPanel panel = (GUIPanel)obj;
				if (!string.IsNullOrEmpty(panel.BackgroundImage))
				{
					if (panel.BackgroundImageLayout != ImageLayout.None)
						_Script.AppendLine(_BaseIdent + "        SetBackgroundImage( \"" + panel.BackgroundImage + "\", " + ConvertImageLayout(panel.BackgroundImageLayout) + " );");
					else
						_Script.AppendLine(_BaseIdent + "        SetBackgroundImage( \"" + panel.BackgroundImage + "\" );");
				}
				if (panel.IsCanMove)
					_Script.AppendLine(_BaseIdent + "        SetCanMove( true, " + panel.IsMoveIgnoreBorders.ToString().ToLower() + " );");
			}
			if (obj is GUIButton)
			{
				GUIButton button = (GUIButton)obj;
				if (button.IsDisabled)
					_Script.AppendLine(_BaseIdent + "        SetCondition( false );");
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
				if (!string.IsNullOrEmpty(button.DisabledImage))
				{
					if (button.DisabledImageLayout != ImageLayout.None)
						_Script.AppendLine(_BaseIdent + "        SetDisabledImage( \"" + button.DisabledImage + "\", " + ConvertImageLayout(button.DisabledImageLayout) + " );");
					else
						_Script.AppendLine(_BaseIdent + "        SetDisabledImage( \"" + button.DisabledImage + "\" );");
				}
			}
			if (obj is GUICheckBox)
			{
				GUICheckBox checkBox = (GUICheckBox)obj;
				if (!string.IsNullOrEmpty(checkBox.IsChecked))
					_Script.AppendLine(_BaseIdent + "        SetChecked( " + checkBox.IsChecked + " );");
			}
			if (obj is GUIText)
			{
				GUIText text = (GUIText)obj;

				if (!string.IsNullOrEmpty(text.Text))
					_Script.AppendLine(_BaseIdent + "        SetText( " + text.Text + " );");
				if (!string.IsNullOrEmpty(text.Font))
					_Script.AppendLine(_BaseIdent + "        SetTextFont( " + text.Font + " );");

				string textFlags = "";
				if (text.HorisontalAlignment == StringAlignment.Center)
					textFlags += "FT_CENTERX | ";
				if (text.VerticalAlignment == StringAlignment.Center)
					textFlags += "FT_CENTERY | ";
				if (text.HorisontalAlignment == StringAlignment.Far)
					textFlags += "FT_CENTERR | ";
				if (text.VerticalAlignment == StringAlignment.Far)
					textFlags += "FT_BOTTOM | ";
				if (text.DrawFromBottom)
					textFlags += "FT_UPPER | ";
				if (text.NoColorize)
					textFlags += "FT_NO_COLORIZE | ";
				if (text.Align)
					textFlags += "FT_ALIGN | ";
				if (text.Bordered)
					textFlags += "FT_BORDERED | ";
				if (textFlags != "")
				{
					textFlags = textFlags.Remove(textFlags.Length - 3);
					_Script.AppendLine(_BaseIdent + "        SetTextFlags( " + textFlags + " );");
				}

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
				if (!string.IsNullOrEmpty(messageBox.DisplayedMessages))
					_Script.AppendLine(_BaseIdent + "        SetDisplayedMessages( " + messageBox.DisplayedMessages + ", true );");
			}
			if (obj is GUIConsole)
			{
				GUIConsole console = (GUIConsole)obj;
				if (!string.IsNullOrEmpty(console.HistoryStorageName))
					_Script.AppendLine(_BaseIdent + "        SetHistoryStorage( " + console.HistoryStorageName + " );");
				if (!string.IsNullOrEmpty(console.HistoryMaxLength))
					_Script.AppendLine(_BaseIdent + "        SetHistoryMaxLength( " + console.HistoryMaxLength + " );");
			}
			if (obj is GUIGrid)
			{
				GUIGrid grid = (GUIGrid)obj;
				if (!string.IsNullOrEmpty(grid.CellPrototype))
					_Script.AppendLine(_BaseIdent + "        SetCellPrototype( " + grid.CellPrototype + " );");
				if (!string.IsNullOrEmpty(grid.GridSize))
					_Script.AppendLine(_BaseIdent + "        SetGridSize( " + grid.GridSize + " );");
				if (grid.Columns != 0)
					_Script.AppendLine(_BaseIdent + "        SetColumns( " + grid.Columns + " );");
				if (!grid.Padding.IsEmpty)
					_Script.AppendLine(_BaseIdent + "        SetPadding( " + grid.Padding.Width + ", " + grid.Padding.Height + " );");
			}
			if (obj is GUIItemView)
			{
				GUIItemView itemView = (GUIItemView)obj;
				if (itemView.UseSorting)
					_Script.AppendLine(_BaseIdent + "        SetUseSorting( true );");
			}
			_Script.AppendLine(_BaseIdent + "    }");

			// Callbacks
			WriteClassFunction("void OnInit() override", obj.OnInit);
			WriteClassFunction("void OnShow( dictionary@ params ) override", obj.OnShow);
			WriteClassFunction("void OnHide() override", obj.OnHide);
			WriteClassFunction("void OnAppear( dictionary@ params ) override", obj.OnAppear);
			WriteClassFunction("void OnDisappear() override", obj.OnDisappear);
			WriteClassFunction("void OnDraw() override", obj.OnDraw);
			WriteClassFunction("void OnMove( int deltaX, int deltaY ) override", obj.OnMove);
			WriteClassFunction("void OnMouseDown( int button ) override", obj.OnMouseDown);
			WriteClassFunction("void OnMouseUp( int button, bool lost ) override", obj.OnMouseUp);
			WriteClassFunction("void OnMousePressed( int button ) override", obj.OnMousePressed);
			WriteClassFunction("void OnLMousePressed() override", obj.OnLMousePressed);
			WriteClassFunction("void OnRMousePressed() override", obj.OnRMousePressed);
			WriteClassFunction("void OnMouseClick( int button ) override", obj.OnMouseClick);
			WriteClassFunction("void OnLMouseClick() override", obj.OnLMouseClick);
			WriteClassFunction("void OnRMouseClick() override", obj.OnRMouseClick);
			WriteClassFunction("void OnMouseMove() override", obj.OnMouseMove);
			WriteClassFunction("void OnGlobalMouseDown( int button ) override", obj.OnGlobalMouseDown);
			WriteClassFunction("void OnGlobalMouseUp( int button ) override", obj.OnGlobalMouseUp);
			WriteClassFunction("void OnGlobalMousePressed( int button ) override", obj.OnGlobalMousePressed);
			WriteClassFunction("void OnGlobalMouseClick( int button ) override", obj.OnGlobalMouseClick);
			WriteClassFunction("void OnGlobalMouseMove() override", obj.OnGlobalMouseMove);
			WriteClassFunction("void OnInput( uint8 key, string@ text ) override", obj.OnInput);
			WriteClassFunction("void OnGlobalInput( uint8 key, string@ text ) override", obj.OnGlobalInput);
			WriteClassFunction("void OnActiveChanged() override", obj.OnActiveChanged);
			WriteClassFunction("void OnFocusChanged() override", obj.OnFocusChanged);
			WriteClassFunction("void OnHoverChanged() override", obj.OnHoverChanged);
			WriteClassFunction("void OnResizeGrid( GUIObject@ cell, uint cellIndex ) override", obj.OnResizeGrid);
			WriteClassFunction("void OnDrawItem( ItemCl@ item, GUIObject@ cell, uint cellIndex ) override", obj.OnDrawItem);
			if (obj is GUICheckBox)
				WriteClassFunction("void OnCheckedChanged() override", ((GUICheckBox)obj).OnCheckedChanged);
			if (obj is GUIItemView)
				WriteClassFunction("int OnCheckItem( ItemCl@ item ) override", ((GUIItemView)obj).OnCheckItem);

			// Subtypes
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
			string className = obj.Name;
			string instanceName = "_" + className;
			GUIScreen root = (obj is GUIScreen ? (GUIScreen)obj : null);
			string identPrefix = _BaseIdent + "    " + instanceName;

			if (root != null)
			{
				_Script.AppendLine(_BaseIdent + "    " + className + " " + instanceName + "();");
				_Script.AppendLine(_BaseIdent + "    " + instanceName + ".Init( null );");
			}
			else
			{
				string parentClassName = obj.GetParent().Name;
				string parentInstanceName = "_" + parentClassName;
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
