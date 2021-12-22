#ifndef FONLINE_IMGUI_WINDOWSD3D_H
#define FONLINE_IMGUI_WINDOWSD3D_H
#include "StdAfx.h"
#include "FonlineImguiWindows.h"

namespace FOnline
{
	class FonlineImguiWindowsD3D : public FonlineImguiWindows
	{
		void InitGraphics( Device_ device ) override;
		void FinishGraphics( ) override;
		void NewFrameGraphics( ) override;
		void RenderGraphics( ) override;
	};
#endif // FONLINE_IMGUI_WINDOWS_H
}