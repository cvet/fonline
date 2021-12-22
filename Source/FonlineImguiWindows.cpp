#include "FonlineImguiWindows.h"

#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

using namespace FOnline;

void FOnline::FonlineImguiWindows::InitOS( FOWindow * window )
{
	if( !ImGui_ImplWin32_Init( fl_xid( window ) ) )
	{
		WriteLog( "Error imgui::win32 init\n" );
		return;
	}
}

void FOnline::FonlineImguiWindows::FinishOS( )
{
	ImGui_ImplWin32_Shutdown( );
}

void FOnline::FonlineImguiWindows::NewFrameOS( )
{
	ImGui_ImplWin32_NewFrame( );
}

void FOnline::FonlineImguiWindows::MouseEvent( int event, int button, int dy )
{
	struct
	{
		union
		{
			struct
			{
				short int low;
				short int hi;
			};
			int w;
		};
	} param;
	if( event == FL_PUSH || event == FL_RELEASE )
	{
		switch( button )
		{
			case FL_LEFT_MOUSE:
				event = event == FL_PUSH ? WM_LBUTTONDOWN : WM_LBUTTONUP;
				break;
			case FL_RIGHT_MOUSE:
				event = event == FL_PUSH ? WM_RBUTTONDOWN : WM_RBUTTONUP;
				break;
			case FL_MIDDLE_MOUSE:
				event = event == FL_PUSH ? WM_MBUTTONDOWN : WM_MBUTTONUP;
				break;
			default: return;
		}
	}

	if( event == FL_MOUSEWHEEL )
	{
		event = WM_MOUSEWHEEL;
		param.hi = dy * WHEEL_DELTA;
	}

	ImGui_ImplWin32_WndProcHandler( fl_xid( GetWindow() ), event, param.w, 0 );
}

void FOnline::FonlineImguiWindows::MouseMoveEvent( int x, int y )
{
	ImGui_ImplWin32_WndProcHandler( fl_xid( GetWindow( ) ), WM_MOUSEMOVE, 0, 0 );
}
