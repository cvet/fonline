//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop
//---------------------------------------------------------------------------
USEFORM("Config.cpp", ConfigForm);
USEFORM("Updater.cpp", UpdaterForm);
USEFORM("Selector.cpp", SelectorForm);
USEFORM("TypeServer.cpp", TypeServerForm);
USEFORM("TypeProxy.cpp", TypeProxyForm);
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	setlocale(LC_ALL, "Russian");

	try
	{
		Application->Initialize();
		SetApplicationMainFormOnTaskBar(Application, true);
		Application->Title = "Launcher";
		Application->CreateForm(__classid(TUpdaterForm), &UpdaterForm);
		Application->CreateForm(__classid(TTypeProxyForm), &TypeProxyForm);
		Application->Run();
	}
	catch (Exception &exception)
	{
		Application->ShowException(&exception);
	}
	catch (...)
	{
		try
		{
			throw Exception("");
		}
		catch (Exception &exception)
		{
			Application->ShowException(&exception);
		}
	}
	return 0;
}
//---------------------------------------------------------------------------
