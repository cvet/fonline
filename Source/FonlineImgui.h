#ifndef FONLINE_IMGUI_H
#define FONLINE_IMGUI_H
#include "StdAfx.h"
#include "imgui.h"

namespace FOnline
{
	class FonlineImgui;
	extern FonlineImgui* GetMainImgui( );
	extern FonlineImgui* CreateImgui( );
	extern void DestroyImgui( FonlineImgui* imgui );

	struct FormatTextFlag
	{
		union
		{
			uint flag;
			struct
			{
				uint OnlyLastLine : 1;
			};
		};
	};

	extern char* FormatText( char* text, FormatTextFlag flag, uint& color );

	struct colorize
	{
		union
		{
			struct
			{
				UINT8 r;
				UINT8 g;
				UINT8 b;
				UINT8 a;
			};
			uint color;
		};
	};

	class FonlineImgui
	{
		ImGuiContext* Context;
		FOWindow* Window;

		uint WorkCounter;

		bool IsDemoWindow;

		void ScriptFrame( );

		virtual void InitOS( FOWindow* window ) = 0;
		virtual void InitGraphics( Device_ device ) = 0;

		virtual void FinishOS( ) = 0;
		virtual void FinishGraphics( ) = 0;

		virtual void NewFrameOS( ) = 0;
		virtual void NewFrameGraphics( ) = 0;

		virtual void RenderGraphics( ) = 0;

		static ImFontAtlas FontAtlas;

	protected:
		inline ImDrawData *GetDrawData( ) { return ImGui::GetDrawData( ); }

		void WorkContext( );
		void DropContext( );

	public:
		FonlineImgui( );

		void ShowDemo( );
		void Init( FOWindow* window, Device_ device );
		void Finish( );
		void Frame( );
		void RenderIface( );

		void NewFrame( );
		void EndFrame( );

		inline ImGuiIO& GetIO( ) { return ImGui::GetIO( ); }

		static void RenderAll( );

		inline bool IsMain( ) { return this == GetMainImgui( ); }
		inline FOWindow* GetWindow( ) { return Window; }

		virtual void MouseEvent( int event, int button, int dy ) = 0;
		virtual void MouseMoveEvent( int x, int y ) = 0;
	};
}
#endif // FONLINE_IMGUI_H