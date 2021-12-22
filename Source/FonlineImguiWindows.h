#ifndef FONLINE_IMGUI_WINDOWS_H
#define FONLINE_IMGUI_WINDOWS_H
#include "StdAfx.h"
#include "FonlineImgui.h"

#include "backends/imgui_impl_win32.h"

namespace FOnline
{
	class FonlineImguiWindows : public FonlineImgui
	{
		void InitOS( FOWindow* window ) override;
		virtual void InitGraphics( Device_ device ) = 0;

		void FinishOS( ) override;
		virtual void FinishGraphics( ) = 0;

		void NewFrameOS( ) override;
		virtual void NewFrameGraphics( ) = 0;

		virtual void RenderGraphics( ) = 0;
	
	public:
		void MouseEvent( int event, int button, int dy ) override;
		void MouseMoveEvent( int x, int y ) override;
	};
#endif // FONLINE_IMGUI_WINDOWS_H
}