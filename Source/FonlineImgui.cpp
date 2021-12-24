#include "FonlineImgui.h"

using namespace FOnline;

ImFontAtlas FOnline::FonlineImgui::FontAtlas;

std::vector<FonlineImgui*> ScriptInstance;
std::vector<FonlineImgui*> Pool;

void FonlineImgui::ScriptFrame( )
{
}

void FOnline::FonlineImgui::WorkContext( )
{
	auto context = ImGui::GetCurrentContext( );
	if( context == Context )
	{
		if( !WorkCounter )
		{
			WriteLog( "Error imgui <work context counter>, current context invalid\n" );
			return;
		}
		WorkCounter++;
		return;
	}
	if( ImGui::GetCurrentContext( ) != nullptr )
	{
		WriteLog( "Error imgui <work context>, current context invalid\n" );
		return;
	}
	WorkCounter = 1;
	ImGui::SetCurrentContext( Context );
}

void FOnline::FonlineImgui::DropContext( )
{
	if( ImGui::GetCurrentContext( ) != Context )
	{
		WriteLog( "Error imgui <drop context>, current context invalid\n" );
		return;
	}

	if( !WorkCounter )
	{
		WriteLog( "Error imgui <drop context counter>, current context invalid\n" );
	}
	else
	{
		WorkCounter -= 1;
		return;
	}
	ImGui::SetCurrentContext( nullptr );
}

FonlineImgui::FonlineImgui( ) : Context( nullptr ), IsDemoWindow( false ), Window( nullptr ), WorkCounter( 0 )
{
}

void FonlineImgui::ShowDemo( )
{
	IsDemoWindow = true;
}

void FonlineImgui::Init( FOWindow* window, Device_ device )
{
	// imgui
	bool isMain = IsMain( );
	if( isMain )
	{
		IMGUI_CHECKVERSION( );
		WriteLog( "Init imgui. Version: %s\n", IMGUI_VERSION );
	}
	Context = ImGui::CreateContext( &FontAtlas );
	WorkCounter = 1;

	ImGuiIO& io = GetIO( );
	io.IniFilename = "FOnlineImgui.ini";
	io.LogFilename = "FOnlineImgui.log";

	if( isMain )
	{
		IsDemoWindow = Str::Substring( CommandLine, "-ImguiDemo" ) != nullptr;

		ImFontConfig font_config;
		font_config.OversampleH = 1; //or 2 is the same
		font_config.OversampleV = 1;
		font_config.PixelSnapH = 1;

		static const ImWchar ranges[] =
		{
			0x0020, 0x00FF, // Basic Latin + Latin Supplement
			0x0400, 0x044F, // Cyrillic
			0,
		};

		io.Fonts->AddFontFromFileTTF( "C:\\Windows\\Fonts\\Courier new.ttf", 12.0f/*, &font_config, ranges*/ );

	}

	InitOS( window );
	InitGraphics( device );
	Window = window;

	// ImGui::LogToFile( );
	DropContext( );
}

void FonlineImgui::Finish( )
{
	WorkContext( );
	FinishGraphics( );
	FinishOS( );
	ImGui::SetCurrentContext( nullptr );
	ImGui::DestroyContext( Context );
	DropContext( );
}

void FonlineImgui::Frame( )
{
	NewFrame( );

	ScriptFrame( );
	if( IsDemoWindow )
		ImGui::ShowDemoWindow( &IsDemoWindow );

	EndFrame( );
}

void FonlineImgui::RenderIface( )
{
	WorkContext( );
	ImGui::Render( );
	RenderGraphics( );
	DropContext( );
}

void FonlineImgui::NewFrame( )
{
	WorkContext( );
	NewFrameGraphics( );
	NewFrameOS( );
	ImGui::NewFrame( );
}

void FonlineImgui::EndFrame( )
{
	ImGui::EndFrame( );
	DropContext( );
}

void FonlineImgui::RenderAll( )
{
	GetMainImgui( )->RenderIface( );
	for( uint i = 0, iend = ScriptInstance.size(); i < iend; i++ )
	{
		ScriptInstance[i]->RenderIface( );
	}
}

