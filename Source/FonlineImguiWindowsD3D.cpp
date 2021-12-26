#include "FonlineImguiWindowsD3D.h"

#include "backends/imgui_impl_dx9.h"

using namespace FOnline;

FonlineImguiWindowsD3D MainImgui;

FonlineImgui* FOnline::CreateImgui( )
{
	return new FonlineImguiWindowsD3D( );
}

void FOnline::DestroyImgui( FonlineImgui* imgui )
{
	if( imgui )
	{
		imgui->Finish( );
		delete imgui;
	}
}

FonlineImgui* FOnline::GetMainImgui( )
{
	return &MainImgui;
}

void FOnline::FonlineImguiWindowsD3D::InitGraphics( Device_ device )
{
	ImGui_ImplDX9_Init( device );
}

void FOnline::FonlineImguiWindowsD3D::FinishGraphics( )
{
	ImGui_ImplDX9_Shutdown( );
}

void FOnline::FonlineImguiWindowsD3D::NewFrameGraphics( )
{
	ImGui_ImplDX9_NewFrame( );
}

void FOnline::FonlineImguiWindowsD3D::RenderGraphics( )
{
	ImGui_ImplDX9_RenderDrawData( GetDrawData( ) );
}
