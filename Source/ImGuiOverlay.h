#ifndef IM_GUI_OVERLAY_H
#define IM_GUI_OVERLAY_H

#include "StdAfx.h"
#include "FonlineImgui.h"

namespace FOnline
{
	class Overlay;
	struct OverlayFlag;
	
	extern Overlay* CreateOverlay( );
	extern void DestroyOverlay( Overlay* overlay );
	extern Overlay* GetOverlay( );
	extern void FinishOverlay( );
	
	struct OverlayFlag
	{
		bool IsInit : 1;
		bool IsFinish : 1;
	};

	class Overlay
	{
		friend Overlay* GetOverlay( );
		friend void FinishOverlay( );

		virtual void Finish( ) = 0;

	protected:
		Overlay( );

		OverlayFlag* GetFlag( );
	
	public:
		virtual void Init( ) = 0;
	};
}

#endif
