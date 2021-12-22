#include "FonlineImgui.h"

#ifdef FO_WINDOWS
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
#endif

FonlineImgui FoImgui;

void FonlineImgui::ScriptFrame( )
{
	if( show_demo_window )
		ImGui::ShowDemoWindow( &show_demo_window );
}

FonlineImgui::FonlineImgui( ) : Context( nullptr ), show_demo_window( false )
{
}

void FonlineImgui::ShowDemo( )
{
	show_demo_window = true;
}

void FonlineImgui::Init( FOWindow* window, Device_ device )
{

	// imgui
	IMGUI_CHECKVERSION( );
	WriteLog( "Init imgui. Version: %s\n", IMGUI_VERSION );
	Context = ImGui::CreateContext( );
	ImGuiIO& io = GetIO( );
	io.IniFilename = "FOnlineImgui.ini";

#ifdef FO_WINDOWS
	if( !ImGui_ImplWin32_Init( fl_xid( window ) ) )
	{
		WriteLog( "Error imgui::win32 init\n" );
		return;
	}
#endif
#ifdef FO_D3D
	ImGui_ImplDX9_Init( device );
#endif
#ifndef FO_D3D
	//if( !ImGui_ImplOpenGL2_Init( ) )
	if( !ImGui_ImplOpenGL3_Init( ) )
	{
		WriteLog( "Error imgui::opengl3 init\n" );
		return;
	}
#endif	

	show_demo_window = Str::Substring( CommandLine, "-ImguiDemo" ) != nullptr;
}

void FonlineImgui::Finish( )
{
#ifdef FO_D3D
	ImGui_ImplDX9_Shutdown( );
#endif
#ifndef FO_D3D
	ImGui_ImplOpenGL3_Shutdown( );
#endif	
#ifdef FO_WINDOWS
	ImGui_ImplWin32_Shutdown( );
#endif
	ImGui::DestroyContext( Context );
	Context = nullptr;

	// ImGui::DestroyContext( );
}

void FonlineImgui::PrepareFrame( )
{

#ifdef FO_D3D
	ImGui_ImplDX9_NewFrame( );
#endif
#ifndef FO_D3D
	ImGui_ImplOpenGL3_NewFrame( );
#endif	
#ifdef FO_WINDOWS
	ImGui_ImplWin32_NewFrame( );
#endif
	ImGui::NewFrame( );
	ScriptFrame( );
	ImGui::EndFrame( );
}

void FonlineImgui::Render( )
{
	ImGui::Render( );

#ifdef FO_D3D
	ImGui_ImplDX9_RenderDrawData( ImGui::GetDrawData( ) );
#endif
#ifndef FO_D3D
	ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData( ) );
#endif	
	
}

void FonlineImgui::MouseEvent( int event, int button, int dy )
{
	struct
	{
		union
		{
			struct
			{
				short int low;
				short int hi;
			};
			int w;
		};
	} param;
	if( event == FL_PUSH || event == FL_RELEASE )
	{
		switch( button )
		{
			case FL_LEFT_MOUSE:
				event = event == FL_PUSH ? WM_LBUTTONDOWN : WM_LBUTTONUP;
				break;
			case FL_RIGHT_MOUSE:
				event = event == FL_PUSH ? WM_RBUTTONDOWN : WM_RBUTTONUP;
				break;
			case FL_MIDDLE_MOUSE:
				event = event == FL_PUSH ? WM_MBUTTONDOWN : WM_MBUTTONUP;
				break;
			default: return;
		}
	}

	if( event == FL_MOUSEWHEEL )
	{
		event = WM_MOUSEWHEEL;
		param.hi = dy * WHEEL_DELTA;
	}

#ifdef FO_WINDOWS
	ImGui_ImplWin32_WndProcHandler( fl_xid( MainWindow ), event, param.w, 0 );
#endif
}

void FonlineImgui::MouseMoveEvent( int x, int y )
{

#ifdef FO_WINDOWS
	ImGui_ImplWin32_WndProcHandler( fl_xid( MainWindow ), WM_MOUSEMOVE, 0, 0 );
#endif
}
