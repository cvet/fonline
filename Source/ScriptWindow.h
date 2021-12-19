#include "StdAfx.h"
#include "AngelScript/angelscript.h"

class ScriptWindow : public FOWindow
{

	
public:
	void Show( );
	void Hide( );

	void Resize( int x, int y );
	void Focus( );
	void Unfocus( );

	bool IsFocus( );

	static void Registration( asIScriptEngine* engine );
	static void Create( ScriptWindow* windows, std::string name );
};
