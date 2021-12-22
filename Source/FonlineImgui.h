#ifndef FONLINE_IMGUI_H
#define FONLINE_IMGUI_H
#include "StdAfx.h"
#include "imgui.h"

namespace FOnline
{
	class FonlineImgui;
	extern FonlineImgui* GetMainImgui( );

	class FonlineImgui
	{
		ImGuiContext* Context;
		FOWindow* Window;

		bool IsDemoWindow;

		void ScriptFrame( );

		virtual void InitOS( FOWindow* window ) = 0;
		virtual void InitGraphics( Device_ device ) = 0;

		virtual void FinishOS( ) = 0;
		virtual void FinishGraphics( ) = 0;

		virtual void NewFrameOS( ) = 0;
		virtual void NewFrameGraphics( ) = 0;

		virtual void RenderGraphics( ) = 0;

	protected:
		inline ImDrawData *GetDrawData( ) { return ImGui::GetDrawData( ); }

	public:
		FonlineImgui( );

		void ShowDemo( );
		void Init( FOWindow* window, Device_ device );
		void Finish( );
		void PrepareFrame( );
		void RenderIface( );

		inline ImGuiIO& GetIO( ) { return ImGui::GetIO( ); }

		static void RenderAll( );

		inline FOWindow* GetWindow( ) { return Window; }

		virtual void MouseEvent( int event, int button, int dy ) = 0;
		virtual void MouseMoveEvent( int x, int y ) = 0;
	};
#endif // FONLINE_IMGUI_H
}