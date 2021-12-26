#ifndef OVERLAY_WINDOWS_D3D_H
#define OVERLAY_WINDOWS_D3D_H
#include "StdAfx.h"
#include "OverlayWindows.h"

namespace FOnline
{
	class OverlayWindowsD3D: public FOnline::OverlayWindows
	{
		friend Overlay* FOnline::CreateOverlay( );

		OverlayWindowsD3D( );

		void Finish( ) override;

		LPDIRECT3D9              g_pD3D;
		LPDIRECT3DDEVICE9        g_pd3dDevice;
		D3DPRESENT_PARAMETERS    g_d3dpp;

	protected:
		void CreateDevice( ) override;
		Device_ GetDevice( ) override;
		void DestroyDevice( ) override;

	public:
		void Init( ) override;
	};
}
#endif // !OVERLAY_WINDOWS_D3D_H
