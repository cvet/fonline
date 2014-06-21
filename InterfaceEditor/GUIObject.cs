using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;
using System.ComponentModel;
using System.Windows.Forms;
using System.Drawing.Design;
using System.IO;
using System.Windows.Forms.Design;

namespace InterfaceEditor
{
	class GUIObject
	{
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

				// Check hidden state for parents
				bool inactive = false;
				GUIObject obj = this;
				while (obj != null && !inactive)
				{
					inactive = !obj.Active;
					obj = obj._Parent;
				}

				// Mark name
				string hiddenMark = " (inactive)";
				if (inactive && !Name.Contains(hiddenMark))
					Name += hiddenMark;
				else if (!inactive && Name.Contains(hiddenMark))
					Name = Name.Remove(Name.IndexOf(hiddenMark), hiddenMark.Length);

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
				_Name = value;
				while (_Parent != null && _Parent._Children.Find(n => n != this && n.Name == _Name) != null)
					_Name += "2";
				if (_HierarchyNode != null)
					_HierarchyNode.Text = Name;
			}
		}
		public string GetFullName()
		{
			GUIObject obj = this;
			string name = "";
			while (obj != null)
			{
				name = obj.Name + "/" + name;
				obj = obj._Parent;
			}
			return name.Trim('/');
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

		public AnchorStyles Anchor { get; set; }
		public DockStyle Dock { get; set; }

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
		public string OnMouseClick { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnLMouseClick { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnRMouseClick { get; set; }
		[Editor(typeof(StringEditor), typeof(UITypeEditor))]
		public string OnInput { get; set; }

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
			// Generate name
			if (parent != null)
			{
				_Name = GetType().ToString();
				_Name = _Name.Substring(_Name.IndexOf(".GUI") + 4);

				int maxIndex = 0;
				Action<GUIObject> checkMaxIndex = null;
				checkMaxIndex = delegate(GUIObject obj)
				{
					int index;
					if (obj._Name.StartsWith(_Name) && int.TryParse(obj._Name.Substring(_Name.Length), out index))
						maxIndex = Math.Max(index, maxIndex);
					foreach (GUIObject nextObj in obj.Children)
						checkMaxIndex(nextObj);
				};
				checkMaxIndex(parent.GetRoot());

				maxIndex++;
				_Name += maxIndex.ToString();
			}
			else
			{
				_Name = "Root Panel";
			}

			// Set initial position
			if (parent != null)
				_Position = parent.AbsolutePosition;

			// Set parent
			AssignParent(parent);
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

		public GUIScreen GetRoot()
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

		public virtual void Draw(Graphics g)
		{
			if (_HierarchyNode.IsSelected)
			{
				TreeNode node = _HierarchyNode;
				while (node != null)
				{
					GUIObject nodeObj = (GUIObject)node.Tag;
					g.DrawRectangle(new Pen(Color.Red, 1f), new Rectangle(nodeObj.AbsolutePosition, nodeObj.Size));
					node = node.Parent;
				}
			}

			foreach (GUIObject child in Children)
				child.Draw(g);
		}

		public void RequestRedraw()
		{
			MainForm.Instance.Design.Invalidate();
		}

		public bool IsHit(int x, int y)
		{
			Point pos = AbsolutePosition;
			return x >= pos.X && y >= pos.Y && x < pos.X + Size.Width && y < pos.Y + Size.Height;
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
				TextBox txt = new TextBox
				{
					Text = (string)value,
					Dock = DockStyle.Fill,
					Multiline = true,
					AcceptsReturn = true,
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
					Text = "Script editor",
					Size = new Size(800, 600),
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
