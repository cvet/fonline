using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;
using System.ComponentModel;
using System.Windows.Forms;
using System.Drawing.Design;
using System.IO;
using System.Windows.Forms.Design;
using System.Text.RegularExpressions;
using FastColoredTextBoxNS;

namespace InterfaceEditor
{
	class GUIObject
	{
		private bool _Expanded;
		[Browsable(false)]
		public bool Expanded
		{
			get
			{
				if (_HierarchyNode != null)
					return _HierarchyNode.IsExpanded;
				return _Expanded;
			}
			set
			{
				_Expanded = value;
				if (_HierarchyNode != null)
				{
					if (_Expanded)
						_HierarchyNode.Expand();
					else
						_HierarchyNode.Collapse();
				}
			}
		}

		private bool _Selected;
		[Browsable(false)]
		public bool Selected
		{
			get
			{
				if (_HierarchyNode != null)
					return _HierarchyNode.IsSelected;
				return _Selected;
			}
			set
			{
				_Selected = value;
				if (_HierarchyNode != null && _Selected)
					MainForm.Instance.Hierarchy.SelectedNode = _HierarchyNode;
			}
		}

		protected bool _Active = true;
		public bool Active
		{
			get
			{
				return _Active;
			}
			set
			{
				// Store value
				_Active = value;

				// Mark name
				Name = Name;

				// Refresh design
				RequestRedraw();

				// Refresh children visibility state
				foreach (GUIObject child in Children)
					child.Active = child.Active;
			}
		}

		protected string _Name;
		public string Name
		{
			get
			{
				return _Name;
			}
			set
			{
				_Name = null;

				string name = Utilites.MakeValidIdentifierName(value);
				GUIObject root = GetRoot();
				if (root.Find(name) != null)
				{
					int maxIndex = -1;
					string indexStr = Regex.Match(name, @"\d+$").Value;
					if (indexStr.Length == 0 || !int.TryParse(indexStr, out maxIndex) || maxIndex <= 0)
						maxIndex = 1;
					if (indexStr.Length > 0)
						name = name.Remove(name.Length - indexStr.Length, indexStr.Length);

					Action<GUIObject> checkMaxIndex = null;
					checkMaxIndex = delegate(GUIObject obj)
					{
						int index;
						if (obj._Name != null && obj._Name.StartsWith(name) && int.TryParse(obj._Name.Substring(name.Length), out index))
							maxIndex = Math.Max(index, maxIndex);
						foreach (GUIObject nextObj in obj.Children)
							checkMaxIndex(nextObj);
					};
					checkMaxIndex(root);

					maxIndex++;
					name = name + maxIndex;
				}

				_Name = name;

				if (_HierarchyNode != null)
				{
					_HierarchyNode.Text = _Name;

					// Check hidden state
					bool inactive = false;
					GUIObject obj = this;
					while (obj != null && !inactive)
					{
						inactive = !obj.Active;
						obj = obj._Parent;
					}
					string hiddenMark = " (inactive)";
					if (inactive && !_HierarchyNode.Text.Contains(hiddenMark))
						_HierarchyNode.Text += hiddenMark;
					else if (!inactive && _HierarchyNode.Text.Contains(hiddenMark))
						_HierarchyNode.Text = Name.Remove(_HierarchyNode.Text.IndexOf(hiddenMark), hiddenMark.Length);

					// Type
					string typeStr = GetType().ToString();
					typeStr = typeStr.Substring(typeStr.IndexOf(".GUI") + 4);
					_HierarchyNode.Text += " (" + typeStr + ")";
				}
			}
		}

		protected Point _Position;
		public Point Position
		{
			get
			{
				return _Position;
			}
			set
			{
				_Position = value;
				RequestRedraw();

				// Refresh children position
				foreach (GUIObject child in Children)
					child.Position = child.Position;
			}
		}
		protected Point AbsolutePosition
		{
			get
			{
				Point position = Point.Empty;
				GUIObject obj = this;
				while (obj != null)
				{
					position.X += obj._Position.X;
					position.Y += obj._Position.Y;
					obj = obj._Parent;
				}
				return position;
			}
		}
		public void SetAbsolutePosition(int x, int y)
		{
			_Position = Point.Empty;
			Point absPos = AbsolutePosition;
			Position = new Point(x - absPos.X, y - absPos.Y);
		}

		protected Size _Size;
		public Size Size
		{
			get
			{
				return _Size;
			}
			set
			{
				_Size = value;
				RequestRedraw();
			}
		}
		virtual public bool IsAutoSize()
		{
			return false;
		}
		virtual public void DoAutoSize()
		{
		}

		public AnchorStyles Anchor { get; set; }
		public DockStyle Dock { get; set; }
		public bool IsDraggable { get; set; }
		public bool IsNotHittable { get; set; }
		public bool CheckTransparentOnHit { get; set; }

		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string GlobalScope { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string ClassFields { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnInit { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnShow { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnHide { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnAppear { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnDisappear { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnDraw { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnMove { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnMouseDown { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnMouseUp { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnMousePressed { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnLMousePressed { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnRMousePressed { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnMouseClick { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnLMouseClick { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnRMouseClick { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnMouseMove { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnGlobalMouseDown { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnGlobalMouseUp { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnGlobalMousePressed { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnGlobalMouseClick { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnGlobalMouseMove { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnInput { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnGlobalInput { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnActiveChanged { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnFocusChanged { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnHoverChanged { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnDragChanged { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnResizeGrid { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnDrawItem { get; set; }

		protected GUIObject _Parent = null;
		protected List<GUIObject> _Children = new List<GUIObject>();
		[BrowsableAttribute(false)]
		public List<GUIObject> Children
		{
			get
			{
				return _Children;
			}
			set
			{
				_Children = value;
				foreach (GUIObject child in _Children)
					child.AssignParent(this);
			}
		}

		protected TreeNode _HierarchyNode;

		public GUIObject(GUIObject parent)
		{
			// Set initial position
			if (parent != null)
				_Position = parent.AbsolutePosition;

			// Set parent
			AssignParent(parent);

			// Generate name
			if (parent != null)
				Name = GetType().ToString().Substring(GetType().ToString().IndexOf(".GUI") + 4) + "1";
			else
				Name = "Screen";
		}

		public virtual void AssignParent(GUIObject parent)
		{
			// Change parent
			if (_Parent != parent)
			{
				if (_Parent != null)
				{
					_Position += new Size(_Parent.AbsolutePosition);
					_Parent.Children.Remove(this);
				}
				_Parent = parent;
				if (_Parent != null && !_Parent.Children.Contains(this))
				{
					_Position -= new Size(_Parent.AbsolutePosition);
					_Parent.Children.Add(this);
				}
			}

			// Refresh representation
			if (_HierarchyNode != null)
				RefreshRepresentation(false);
		}

		public virtual void RefreshRepresentation(bool recursive)
		{
			// Process hierarchy node
			if (_HierarchyNode == null)
			{
				if (_Parent == null)
					_HierarchyNode = MainForm.Instance.Hierarchy.Nodes.Add(Name);
				else
					_HierarchyNode = _Parent._HierarchyNode.Nodes.Add(Name);
				_HierarchyNode.Tag = this;
			}
			else
			{
				_HierarchyNode.Remove();
				if (_Parent == null)
					MainForm.Instance.Hierarchy.Nodes.Add(_HierarchyNode);
				else
					_Parent._HierarchyNode.Nodes.Add(_HierarchyNode);
			}
			if (!_HierarchyNode.IsSelected)
				MainForm.Instance.Hierarchy.SelectedNode = _HierarchyNode;

			// Refresh children position
			if (recursive)
			{
				foreach (GUIObject child in Children)
					child.RefreshRepresentation(recursive);
			}

			// Refresh states
			Name = _Name;
			Expanded = _Expanded;
		}

		public void MoveUp()
		{
			int index = _HierarchyNode.Index;
			if (_Parent != null && index > 0)
			{
				index--;
				_Parent.Children.Remove(this);
				_Parent.Children.Insert(index, this);
				TreeNode parent = _HierarchyNode.Parent;
				parent.Nodes.Remove(_HierarchyNode);
				parent.Nodes.Insert(index, _HierarchyNode);
				MainForm.Instance.Hierarchy.SelectedNode = _HierarchyNode;
			}
			RequestRedraw();
		}

		public void MoveDown()
		{
			int index = _HierarchyNode.Index;
			if (_Parent != null && index < _Parent.Children.Count - 1)
			{
				index++;
				_Parent.Children.Remove(this);
				_Parent.Children.Insert(index, this);
				TreeNode parent = _HierarchyNode.Parent;
				parent.Nodes.Remove(_HierarchyNode);
				parent.Nodes.Insert(index, _HierarchyNode);
				MainForm.Instance.Hierarchy.SelectedNode = _HierarchyNode;
			}
			RequestRedraw();
		}

		public virtual void Delete()
		{
			if (_Parent != null)
				_Parent.Children.Remove(this);
			_HierarchyNode.Remove();
			RequestRedraw();
		}

		public GUIObject Find(string name)
		{
			if (_Name == name)
				return this;
			foreach (GUIObject child in _Children)
			{
				GUIObject obj = child.Find(name);
				if (obj != null)
					return obj;
			}
			return null;
		}

		public GUIObject FindByBackground(string backgroundName)
		{
			if (this is GUIPanel && ((GUIPanel)this).BackgroundImage != null && ((GUIPanel)this).BackgroundImage.Contains(backgroundName))
				return this;
			foreach (GUIObject child in _Children)
			{
				GUIObject obj = child.FindByBackground(backgroundName);
				if (obj != null)
					return obj;
			}
			return null;
		}

		public GUIObject FindSelected()
		{
			if (_Selected)
				return this;
			foreach (GUIObject child in _Children)
			{
				GUIObject obj = child.FindSelected();
				if (obj != null)
					return obj;
			}
			return null;
		}

		public GUIObject GetRoot()
		{
			GUIObject obj = this;
			while (obj._Parent != null)
				obj = obj._Parent;
			return obj;
		}

		public GUIScreen GetScreen()
		{
			GUIObject obj = this;
			while (obj._Parent != null)
				obj = obj._Parent;
			return (GUIScreen)obj;
		}

		public GUIObject GetParent()
		{
			return _Parent;
		}

		public virtual void DrawPass1(Graphics g)
		{
			DrawStroke(g, Color.Green, false);

			foreach (GUIObject child in Children)
				if (child.Active)
					child.DrawPass1(g);
		}

		public virtual void DrawPass2(Graphics g)
		{
			if (_HierarchyNode.IsSelected)
				DrawStroke(g, Color.Red, true);

			foreach (GUIObject child in Children)
				if (child.Active)
					child.DrawPass2(g);
		}

		private void DrawStroke(Graphics g, Color c, bool recursive)
		{
			if (!Active)
				return;

			Point pos = AbsolutePosition;
			if (Size.IsEmpty)
			{
				g.DrawLine(new Pen(c, 1f), new Point(pos.X - 10, pos.Y), new Point(pos.X + 10, pos.Y));
				g.DrawLine(new Pen(c, 1f), new Point(pos.X, pos.Y - 10), new Point(pos.X, pos.Y + 10));
			}
			else if (Size.Width <= 1)
			{
				g.DrawLine(new Pen(c, 1f), new Point(pos.X, pos.Y), new Point(pos.X, pos.Y + Size.Height));
			}
			else if (Size.Height <= 1)
			{
				g.DrawLine(new Pen(c, 1f), new Point(pos.X, pos.Y), new Point(pos.X + Size.Width, pos.Y));
			}
			else
			{
				g.DrawRectangle(new Pen(c, 1f), pos.X, pos.Y, Size.Width - 1, Size.Height - 1);
			}

			if (recursive)
			{
				foreach (GUIObject child in _Children)
					child.DrawStroke(g, c, recursive);
			}
		}

		public void RequestRedraw()
		{
			if (MainForm.Instance.LoadedTree == GetRoot())
			{
				MainForm.Instance.Properties.Refresh();
				MainForm.Instance.Design.Invalidate();
			}
		}

		public bool IsHit(int x, int y)
		{
			Point pos = AbsolutePosition;
			if (!Size.IsEmpty)
				return x >= pos.X && y >= pos.Y && x < pos.X + Size.Width && y < pos.Y + Size.Height;
			return Math.Abs(x - pos.X) <= 10 && Math.Abs(y - pos.Y) <= 10;
		}

		public void FixAfterLoad()
		{
			// Fix names
			Name = Name;

			// Fix children
			foreach (GUIObject child in Children)
				child.FixAfterLoad();
		}
	}

	public class ImageLocationEditor : UITypeEditor
	{
		private static string _InitialDirectory = null;

		public override UITypeEditorEditStyle GetEditStyle(ITypeDescriptorContext context)
		{
			return UITypeEditorEditStyle.Modal;
		}

		public override object EditValue(ITypeDescriptorContext context, IServiceProvider provider, object value)
		{
			using (OpenFileDialog openFileDialog = new OpenFileDialog())
			{
				openFileDialog.Filter = "PNG files (*.png) | *.png";
				if (_InitialDirectory == null)
					_InitialDirectory = Utilites.DataPath;
				openFileDialog.InitialDirectory = _InitialDirectory;
				if (openFileDialog.ShowDialog() == DialogResult.OK)
				{
					string fileName = Utilites.MakeFileName(openFileDialog.FileName);
					if (fileName != null)
					{
						_InitialDirectory = Path.GetDirectoryName(openFileDialog.FileName);
						return fileName;
					}
				}
			}
			return value;
		}
	}

	public class StringEditor : UITypeEditor
	{
		public override UITypeEditorEditStyle GetEditStyle(ITypeDescriptorContext context)
		{
			return UITypeEditorEditStyle.Modal;
		}

		public override object EditValue(ITypeDescriptorContext context, IServiceProvider provider, object value)
		{
			var svc = (IWindowsFormsEditorService)provider.GetService(typeof(IWindowsFormsEditorService));
			if (svc != null)
			{
				FastColoredTextBox txt = new FastColoredTextBox
				{
					Dock = DockStyle.Fill,
					Multiline = true,
					Font = new Font("Courier New", 10f),
					Language = Language.CSharp,
				};
				txt.Text = (string)value;
				Button ok = new Button
				{
					Text = "OK",
					Dock = DockStyle.Bottom,
					DialogResult = DialogResult.OK,
				};
				Form frm = new Form
				{
					Text = "Script editor",
					Size = new Size(800, 600),
					//WindowState = FormWindowState.Maximized,
					AcceptButton = ok,
				};
				frm.Controls.Add(txt);
				frm.Controls.Add(ok);

				if (svc.ShowDialog(frm) == DialogResult.OK)
					value = txt.Text;
			}
			return value;
		}
	}

	public class ListValueEditor : UITypeEditor
	{
		public override UITypeEditorEditStyle GetEditStyle(ITypeDescriptorContext context)
		{
			return UITypeEditorEditStyle.Modal;
		}

		public override object EditValue(ITypeDescriptorContext context, IServiceProvider provider, object value)
		{
			var svc = (IWindowsFormsEditorService)provider.GetService(typeof(IWindowsFormsEditorService));
			if (svc != null)
			{
				ComboBox cb = new ComboBox
				{
					Text = (string)value,
					Dock = DockStyle.Fill,
					Font = new Font("Courier New", 10f),
				};
				Button ok = new Button
				{
					Text = "OK",
					Dock = DockStyle.Bottom,
					DialogResult = DialogResult.OK,
				};
				Form frm = new Form
				{
					Text = "Value editor",
					Size = new Size(300, 90),
					AcceptButton = ok,
				};
				frm.Controls.Add(cb);
				frm.Controls.Add(ok);

				if (context.PropertyDescriptor.ComponentType == typeof(GUIText) && context.PropertyDescriptor.Name == "Font")
					cb.Items.AddRange(GUIText.FontNames);

				if (svc.ShowDialog(frm) == DialogResult.OK)
					value = cb.Text;
			}
			return value;
		}
	}
}
