namespace InterfaceEditor
{
	partial class MainForm
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.tabControl2 = new System.Windows.Forms.TabControl();
			this.tabPage4 = new System.Windows.Forms.TabPage();
			this.Hierarchy = new System.Windows.Forms.TreeView();
			this.tabPage3 = new System.Windows.Forms.TabPage();
			this.panel1 = new System.Windows.Forms.Panel();
			this.buttonLoadScheme = new System.Windows.Forms.Button();
			this.buttonSaveScheme = new System.Windows.Forms.Button();
			this.buttonGenerateScheme = new System.Windows.Forms.Button();
			this.dataGridViewScheme = new System.Windows.Forms.DataGridView();
			this.Column1 = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.Column2 = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.tabControl1 = new System.Windows.Forms.TabControl();
			this.tabPage2 = new System.Windows.Forms.TabPage();
			this.Properties = new System.Windows.Forms.PropertyGrid();
			this.LogText = new System.Windows.Forms.RichTextBox();
			this.splitter1 = new System.Windows.Forms.Splitter();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.Design = new System.Windows.Forms.Panel();
			this.splitter2 = new System.Windows.Forms.Splitter();
			this.splitter3 = new System.Windows.Forms.Splitter();
			this.tabControl2.SuspendLayout();
			this.tabPage4.SuspendLayout();
			this.tabPage3.SuspendLayout();
			this.panel1.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.dataGridViewScheme)).BeginInit();
			this.tabControl1.SuspendLayout();
			this.tabPage2.SuspendLayout();
			this.groupBox1.SuspendLayout();
			this.SuspendLayout();
			// 
			// tabControl2
			// 
			this.tabControl2.Controls.Add(this.tabPage4);
			this.tabControl2.Controls.Add(this.tabPage3);
			this.tabControl2.Dock = System.Windows.Forms.DockStyle.Left;
			this.tabControl2.Location = new System.Drawing.Point(0, 0);
			this.tabControl2.Name = "tabControl2";
			this.tabControl2.SelectedIndex = 0;
			this.tabControl2.Size = new System.Drawing.Size(300, 573);
			this.tabControl2.TabIndex = 4;
			// 
			// tabPage4
			// 
			this.tabPage4.Controls.Add(this.Hierarchy);
			this.tabPage4.Location = new System.Drawing.Point(4, 22);
			this.tabPage4.Name = "tabPage4";
			this.tabPage4.Padding = new System.Windows.Forms.Padding(3);
			this.tabPage4.Size = new System.Drawing.Size(292, 547);
			this.tabPage4.TabIndex = 1;
			this.tabPage4.Text = "GUI";
			this.tabPage4.UseVisualStyleBackColor = true;
			// 
			// Hierarchy
			// 
			this.Hierarchy.AllowDrop = true;
			this.Hierarchy.BackColor = System.Drawing.SystemColors.Window;
			this.Hierarchy.Dock = System.Windows.Forms.DockStyle.Fill;
			this.Hierarchy.ForeColor = System.Drawing.SystemColors.MenuHighlight;
			this.Hierarchy.HideSelection = false;
			this.Hierarchy.Location = new System.Drawing.Point(3, 3);
			this.Hierarchy.Name = "Hierarchy";
			this.Hierarchy.Size = new System.Drawing.Size(286, 541);
			this.Hierarchy.TabIndex = 1;
			// 
			// tabPage3
			// 
			this.tabPage3.Controls.Add(this.panel1);
			this.tabPage3.Controls.Add(this.dataGridViewScheme);
			this.tabPage3.Location = new System.Drawing.Point(4, 22);
			this.tabPage3.Name = "tabPage3";
			this.tabPage3.Padding = new System.Windows.Forms.Padding(3);
			this.tabPage3.Size = new System.Drawing.Size(292, 547);
			this.tabPage3.TabIndex = 0;
			this.tabPage3.Text = "Scheme";
			this.tabPage3.UseVisualStyleBackColor = true;
			// 
			// panel1
			// 
			this.panel1.Controls.Add(this.buttonLoadScheme);
			this.panel1.Controls.Add(this.buttonSaveScheme);
			this.panel1.Controls.Add(this.buttonGenerateScheme);
			this.panel1.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.panel1.Location = new System.Drawing.Point(3, 455);
			this.panel1.Name = "panel1";
			this.panel1.Size = new System.Drawing.Size(286, 89);
			this.panel1.TabIndex = 4;
			// 
			// buttonLoadScheme
			// 
			this.buttonLoadScheme.Location = new System.Drawing.Point(3, 3);
			this.buttonLoadScheme.Name = "buttonLoadScheme";
			this.buttonLoadScheme.Size = new System.Drawing.Size(278, 23);
			this.buttonLoadScheme.TabIndex = 0;
			this.buttonLoadScheme.Text = "Load";
			this.buttonLoadScheme.UseVisualStyleBackColor = true;
			this.buttonLoadScheme.Click += new System.EventHandler(this.buttonLoadScheme_Click);
			// 
			// buttonSaveScheme
			// 
			this.buttonSaveScheme.Location = new System.Drawing.Point(3, 32);
			this.buttonSaveScheme.Name = "buttonSaveScheme";
			this.buttonSaveScheme.Size = new System.Drawing.Size(278, 23);
			this.buttonSaveScheme.TabIndex = 1;
			this.buttonSaveScheme.Text = "Save";
			this.buttonSaveScheme.UseVisualStyleBackColor = true;
			this.buttonSaveScheme.Click += new System.EventHandler(this.buttonSaveScheme_Click);
			// 
			// buttonGenerateScheme
			// 
			this.buttonGenerateScheme.Location = new System.Drawing.Point(3, 61);
			this.buttonGenerateScheme.Name = "buttonGenerateScheme";
			this.buttonGenerateScheme.Size = new System.Drawing.Size(278, 23);
			this.buttonGenerateScheme.TabIndex = 2;
			this.buttonGenerateScheme.Text = "Generate script";
			this.buttonGenerateScheme.UseVisualStyleBackColor = true;
			this.buttonGenerateScheme.Click += new System.EventHandler(this.buttonGenerateScheme_Click);
			// 
			// dataGridViewScheme
			// 
			this.dataGridViewScheme.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
			this.dataGridViewScheme.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.Column1,
            this.Column2});
			this.dataGridViewScheme.Dock = System.Windows.Forms.DockStyle.Fill;
			this.dataGridViewScheme.Location = new System.Drawing.Point(3, 3);
			this.dataGridViewScheme.Name = "dataGridViewScheme";
			this.dataGridViewScheme.Size = new System.Drawing.Size(286, 541);
			this.dataGridViewScheme.TabIndex = 3;
			// 
			// Column1
			// 
			this.Column1.HeaderText = "Screen name";
			this.Column1.Name = "Column1";
			// 
			// Column2
			// 
			this.Column2.HeaderText = "GUI file";
			this.Column2.Name = "Column2";
			// 
			// tabControl1
			// 
			this.tabControl1.Controls.Add(this.tabPage2);
			this.tabControl1.Dock = System.Windows.Forms.DockStyle.Right;
			this.tabControl1.Location = new System.Drawing.Point(738, 0);
			this.tabControl1.Name = "tabControl1";
			this.tabControl1.SelectedIndex = 0;
			this.tabControl1.Size = new System.Drawing.Size(300, 573);
			this.tabControl1.TabIndex = 7;
			// 
			// tabPage2
			// 
			this.tabPage2.Controls.Add(this.Properties);
			this.tabPage2.Location = new System.Drawing.Point(4, 22);
			this.tabPage2.Name = "tabPage2";
			this.tabPage2.Padding = new System.Windows.Forms.Padding(3);
			this.tabPage2.Size = new System.Drawing.Size(292, 547);
			this.tabPage2.TabIndex = 1;
			this.tabPage2.Text = "Properties";
			this.tabPage2.UseVisualStyleBackColor = true;
			// 
			// Properties
			// 
			this.Properties.Dock = System.Windows.Forms.DockStyle.Fill;
			this.Properties.Location = new System.Drawing.Point(3, 3);
			this.Properties.Name = "Properties";
			this.Properties.PropertySort = System.Windows.Forms.PropertySort.NoSort;
			this.Properties.Size = new System.Drawing.Size(286, 541);
			this.Properties.TabIndex = 0;
			this.Properties.ToolbarVisible = false;
			// 
			// LogText
			// 
			this.LogText.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.LogText.Font = new System.Drawing.Font("Courier New", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(204)));
			this.LogText.Location = new System.Drawing.Point(303, 437);
			this.LogText.Name = "LogText";
			this.LogText.Size = new System.Drawing.Size(435, 136);
			this.LogText.TabIndex = 9;
			this.LogText.Text = "";
			this.LogText.WordWrap = false;
			// 
			// splitter1
			// 
			this.splitter1.Location = new System.Drawing.Point(300, 0);
			this.splitter1.Name = "splitter1";
			this.splitter1.Size = new System.Drawing.Size(3, 573);
			this.splitter1.TabIndex = 11;
			this.splitter1.TabStop = false;
			// 
			// groupBox1
			// 
			this.groupBox1.Controls.Add(this.Design);
			this.groupBox1.Dock = System.Windows.Forms.DockStyle.Fill;
			this.groupBox1.Location = new System.Drawing.Point(303, 0);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(432, 434);
			this.groupBox1.TabIndex = 12;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "Design";
			// 
			// Design
			// 
			this.Design.Dock = System.Windows.Forms.DockStyle.Fill;
			this.Design.Location = new System.Drawing.Point(3, 16);
			this.Design.Name = "Design";
			this.Design.Size = new System.Drawing.Size(426, 415);
			this.Design.TabIndex = 5;
			this.Design.Paint += new System.Windows.Forms.PaintEventHandler(this.Design_Paint);
			// 
			// splitter2
			// 
			this.splitter2.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.splitter2.Location = new System.Drawing.Point(303, 434);
			this.splitter2.Name = "splitter2";
			this.splitter2.Size = new System.Drawing.Size(435, 3);
			this.splitter2.TabIndex = 13;
			this.splitter2.TabStop = false;
			// 
			// splitter3
			// 
			this.splitter3.Dock = System.Windows.Forms.DockStyle.Right;
			this.splitter3.Location = new System.Drawing.Point(735, 0);
			this.splitter3.Name = "splitter3";
			this.splitter3.Size = new System.Drawing.Size(3, 434);
			this.splitter3.TabIndex = 14;
			this.splitter3.TabStop = false;
			// 
			// MainForm
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(1038, 573);
			this.Controls.Add(this.groupBox1);
			this.Controls.Add(this.splitter3);
			this.Controls.Add(this.splitter2);
			this.Controls.Add(this.LogText);
			this.Controls.Add(this.splitter1);
			this.Controls.Add(this.tabControl2);
			this.Controls.Add(this.tabControl1);
			this.DoubleBuffered = true;
			this.Name = "MainForm";
			this.Text = "Interface Editor v.1.4";
			this.tabControl2.ResumeLayout(false);
			this.tabPage4.ResumeLayout(false);
			this.tabPage3.ResumeLayout(false);
			this.panel1.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)(this.dataGridViewScheme)).EndInit();
			this.tabControl1.ResumeLayout(false);
			this.tabPage2.ResumeLayout(false);
			this.groupBox1.ResumeLayout(false);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.TabControl tabControl2;
		private System.Windows.Forms.TabPage tabPage3;
		private System.Windows.Forms.TabPage tabPage4;
		private System.Windows.Forms.TabControl tabControl1;
		private System.Windows.Forms.TabPage tabPage2;
		public System.Windows.Forms.TreeView Hierarchy;
		public System.Windows.Forms.PropertyGrid Properties;
		public System.Windows.Forms.RichTextBox LogText;
		private System.Windows.Forms.Splitter splitter1;
		private System.Windows.Forms.GroupBox groupBox1;
		public System.Windows.Forms.Panel Design;
		private System.Windows.Forms.Splitter splitter2;
		private System.Windows.Forms.Splitter splitter3;
		private System.Windows.Forms.Button buttonGenerateScheme;
		private System.Windows.Forms.Button buttonSaveScheme;
		private System.Windows.Forms.Button buttonLoadScheme;
		private System.Windows.Forms.DataGridView dataGridViewScheme;
		private System.Windows.Forms.DataGridViewTextBoxColumn Column1;
		private System.Windows.Forms.DataGridViewTextBoxColumn Column2;
		private System.Windows.Forms.Panel panel1;

	}
}

