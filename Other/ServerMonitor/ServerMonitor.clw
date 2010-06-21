; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CServerMonitorDlg
LastTemplate=CDialog
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "ServerMonitor.h"

ClassCount=4
Class1=CServerMonitorApp
Class2=CServerMonitorDlg
Class3=CAboutDlg

ResourceCount=5
Resource1=IDD_ABOUTBOX
Resource2=IDR_MAINFRAME
Resource3=IDD_SERVERMONITOR_DIALOG
Resource4=IDD_ABOUTBOX (English (U.S.))
Resource5=IDD_SERVERMONITOR_DIALOG (English (U.S.))

[CLS:CServerMonitorApp]
Type=0
HeaderFile=ServerMonitor.h
ImplementationFile=ServerMonitor.cpp
Filter=N

[CLS:CServerMonitorDlg]
Type=0
HeaderFile=ServerMonitorDlg.h
ImplementationFile=ServerMonitorDlg.cpp
Filter=D
BaseClass=CDialog
VirtualFilter=dWC

[CLS:CAboutDlg]
Type=0
HeaderFile=ServerMonitorDlg.h
ImplementationFile=ServerMonitorDlg.cpp
Filter=D

[DLG:IDD_ABOUTBOX]
Type=1
ControlCount=4
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308352
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889
Class=CAboutDlg


[DLG:IDD_SERVERMONITOR_DIALOG]
Type=1
ControlCount=3
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_STATIC,static,1342308352
Class=CServerMonitorDlg

[DLG:IDD_SERVERMONITOR_DIALOG (English (U.S.))]
Type=1
Class=?
ControlCount=4
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=ID_HELP,button,1342242816
Control4=IDC_EDIT1,edit,1350631552

[DLG:IDD_ABOUTBOX (English (U.S.))]
Type=1
Class=?
ControlCount=4
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308480
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889

