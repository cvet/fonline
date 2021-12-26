#include "OverlayWindows.h"

using namespace FOnline;

const string TextMessage[] =
{
	"Init overlay.",
	"Finish overlay."
};

namespace IndexMessage
{
	enum Enum
	{
		Info_Init,
		Info_Finish
	};
};

inline void Message( const IndexMessage::Enum index )
{
	WriteLog( "%s\n", TextMessage[ ( size_t )index ].c_str( ) );
}

OverlayWindows::OverlayWindows( ) : Imgui( nullptr )
{

}

void OverlayWindows::Finish( )
{
	Message( IndexMessage::Info_Finish );
	DestroyImgui( Imgui );
	DestroyDevice( );

}

void OverlayWindows::Init( )
{
	Message( IndexMessage::Info_Init );
	Imgui = CreateImgui( );
	Window = new FOWindow( );

	Window->label( "FOOverlay" );
	//ImGuiViewport* viewport = ImGui::GetMainViewport( );
	Window->position( 0, 0 );
	Window->size( Fl::w( ), Fl::h( ) );
	Window->icon( ( char* )LoadIcon( fl_display, MAKEINTRESOURCE( 101 ) ) );

	Window->show( );

	SetWindowLong( fl_xid( Window ), GWL_STYLE, GetWindowLong( fl_xid( Window ), GWL_STYLE ) & ( ~WS_SYSMENU ) );
	SetWindowPos( fl_xid( Window ), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE );

	CreateDevice( );
	Imgui->Init( Window, GetDevice( ) );
}
