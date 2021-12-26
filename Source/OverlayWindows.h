#ifndef OVERLAY_WINDOWS_H
#define OVERLAY_WINDOWS_H
#include "StdAfx.h"
#include "ImGuiOverlay.h"

namespace FOnline
{
	class OverlayWindows: public FOnline::Overlay
	{
		friend Overlay* FOnline::CreateOverlay( );

		FonlineImgui* Imgui;
		FOWindow* Window;

	protected:
		OverlayWindows( );

		virtual void CreateDevice( ) = 0;
		virtual Device_ GetDevice( ) = 0;
		virtual void DestroyDevice( ) = 0;

		void Finish( ) override;

		inline FOWindow* GetWindow( ) { return Window; }
		inline FonlineImgui* GetImgui( ) { return Imgui; }

	public:
		void Init( ) override;
	};
}
#endif // !OVERLAY_WINDOWS_H
