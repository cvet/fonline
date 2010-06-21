// ServerMonitorDlg.h : header file
//

#if !defined(AFX_SERVERMONITORDLG_H__355F677E_F023_4A5F_A229_5BF9F47AF149__INCLUDED_)
#define AFX_SERVERMONITORDLG_H__355F677E_F023_4A5F_A229_5BF9F47AF149__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CServerMonitorDlg dialog

class CServerMonitorDlg : public CDialog
{
// Construction
public:
	CServerMonitorDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CServerMonitorDlg)
	enum { IDD = IDD_SERVERMONITOR_DIALOG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CServerMonitorDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CServerMonitorDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnDestroy();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SERVERMONITORDLG_H__355F677E_F023_4A5F_A229_5BF9F47AF149__INCLUDED_)
