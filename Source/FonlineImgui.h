#ifndef FONLINE_IMGUI_H
#define FONLINE_IMGUI_H
#include "StdAfx.h"
#include "imgui.h"

#ifdef FO_D3D
#include "backends/imgui_impl_dx9.h"
#endif
#ifndef FO_D3D
#include "backends/imgui_impl_opengl3.h"
#endif
#ifdef FO_WINDOWS
#include "backends/imgui_impl_win32.h"
#endif

class FonlineImgui
{

	ImGuiContext* Context;

	void ScriptFrame( );
	bool show_demo_window;
public:
	FonlineImgui( );

	void ShowDemo( );
	void Init( FOWindow* window, Device_ device );
	void Finish( );
	void PrepareFrame( );
	void Render( );

	void MouseEvent( int event, int button, int dy );
	void MouseMoveEvent( int x, int y );

	inline ImGuiIO& GetIO( )
	{ 
		return ImGui::GetIO( );
	};
};

extern FonlineImgui FoImgui;
#endif // FONLINE_IMGUI_H
