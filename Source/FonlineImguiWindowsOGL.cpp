#include "FonlineImguiWindowsOGL.h"

#include "backends/imgui_impl_opengl3.h"

using namespace FOnline;

FonlineImguiWindowsOGL MainImgui;

FonlineImgui* FOnline::GetMainImgui( )
{
	return &MainImgui;
}

void FOnline::FonlineImguiWindowsOGL::InitGraphics( Device_ device )
{
	if( !ImGui_ImplOpenGL3_Init( ) )
	{
		WriteLog( "Error imgui::opengl3 init\n" );
		return;
	}
}

void FOnline::FonlineImguiWindowsOGL::FinishGraphics( )
{
	ImGui_ImplOpenGL3_Shutdown( );
}

void FOnline::FonlineImguiWindowsOGL::NewFrameGraphics( )
{
	ImGui_ImplOpenGL3_NewFrame( );
}

void FOnline::FonlineImguiWindowsOGL::RenderGraphics( )
{
	ImGui_ImplOpenGL3_RenderDrawData( GetDrawData( ) );
}
