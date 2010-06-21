//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
USEFORM("main.cpp", frmEnv);
USEFORM("pbar.cpp", frmPBar);
USEFORM("mdi.cpp", frmMDI);
USEFORM("config.cpp", frmConfig);
USEFORM("change.cpp", fmChange);
USEFORM("frameHeader.cpp", frmHdr); /* TFrame: File Type */
USEFORM("frmInven.cpp", frmInv); /* TFrame: File Type */
USEFORM("frmObj.cpp", frmObject); /* TFrame: File Type */
USEFORM("frmMiniMap.cpp", frmMMap); /* TFrame: File Type */
USEFORM("FormAskLevel.cpp", frmSelLevel);
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    try
    {
        Application->Initialize();
        Application->Title = "Mapper";
        Application->CreateForm(__classid(TfrmMDI), &frmMDI);
		Application->CreateForm(__classid(TfrmConfig), &frmConfig);
		Application->CreateForm(__classid(TfmChange), &fmChange);
		Application->CreateForm(__classid(TfrmSelLevel), &frmSelLevel);
		Application->Run();
    }
    catch (Exception &exception)
    {
        Application->ShowException(&exception);
    }
    return 0;
}
//---------------------------------------------------------------------------




