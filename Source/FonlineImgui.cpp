#include "FonlineImgui.h"

using namespace FOnline;

std::vector<FonlineImgui*> ScriptInstance;

void FonlineImgui::ScriptFrame( )
{
	if( IsDemoWindow )
		ImGui::ShowDemoWindow( &IsDemoWindow );
}

FonlineImgui::FonlineImgui( ) : Context( nullptr ), IsDemoWindow( false ), Window( nullptr )
{
}

void FonlineImgui::ShowDemo( )
{
	IsDemoWindow = true;
}

void FonlineImgui::Init( FOWindow* window, Device_ device )
{
	// imgui
	IMGUI_CHECKVERSION( );
	WriteLog( "Init imgui. Version: %s\n", IMGUI_VERSION );
	Context = ImGui::CreateContext( );
	ImGuiIO& io = GetIO( );
	io.IniFilename = "FOnlineImgui.ini";

	InitOS( window );
	InitGraphics( device );

	if( window == MainWindow )
		IsDemoWindow = Str::Substring( CommandLine, "-ImguiDemo" ) != nullptr;
	Window = window;
}

void FonlineImgui::Finish( )
{
	FinishGraphics( );
	FinishOS( );
	ImGui::DestroyContext( Context );
	Context = nullptr;

	// ImGui::DestroyContext( );
}

void FonlineImgui::PrepareFrame( )
{
	NewFrameGraphics( );
	NewFrameOS( );
	ImGui::NewFrame( );
	ScriptFrame( );
	ImGui::EndFrame( );
}

void FonlineImgui::RenderAll( )
{
	GetMainImgui( )->RenderIface( );
	for( uint i = 0, iend = ScriptInstance.size(); i < iend; i++ )
	{
		ScriptInstance[i]->RenderIface( );
	}
}

void FonlineImgui::RenderIface( )
{
	ImGui::Render( );
	RenderGraphics( );	
}

