#include "OverlayWindowsD3D.h"

using namespace FOnline;

const string TextMessage[] =
{
	""
};

namespace IndexMessage
{
	enum Enum
	{
		None
	};
};

inline void Message( const IndexMessage::Enum index )
{
	WriteLog( "%s\n", TextMessage[ ( size_t )index ].c_str( ) );
}

Overlay* FOnline::CreateOverlay( )
{
	OverlayWindows* windowsOverlay = new OverlayWindowsD3D( );

	return windowsOverlay;
}

void FOnline::DestroyOverlay( Overlay* overlay )
{
	if( overlay )
		delete overlay;
}

OverlayWindowsD3D::OverlayWindowsD3D( ): g_pD3D( nullptr ), g_pd3dDevice( nullptr ), g_d3dpp( )
{

}

void OverlayWindowsD3D::Finish( )
{
	OverlayWindows::Finish( );
}

void FOnline::OverlayWindowsD3D::CreateDevice( )
{
	if( ( g_pD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) == NULL )
	{
		WriteLog( "Error create overlay device #0.\n" );
		return;
	}
	ZeroMemory( &g_d3dpp, sizeof( g_d3dpp ) );
	g_d3dpp.Windowed = TRUE;
	g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	g_d3dpp.EnableAutoDepthStencil = TRUE;
	g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	if( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, fl_xid( GetWindow( ) ), D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice ) < 0 )
	{
		WriteLog( "Error create overlay device #1.\n" );
		return;
	}
}

Device_ FOnline::OverlayWindowsD3D::GetDevice( )
{
	return g_pd3dDevice;
}

void FOnline::OverlayWindowsD3D::DestroyDevice( )
{
	if( g_pd3dDevice ) 
	{ 
		g_pd3dDevice->Release( );
		g_pd3dDevice = NULL;
	}

	if( g_pD3D )
	{
		g_pD3D->Release( );
		g_pD3D = NULL;
	}
}

void OverlayWindowsD3D::Init( )
{
	OverlayWindows::Init( );
}
