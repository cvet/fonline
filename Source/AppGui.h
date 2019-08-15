#pragma once

#include "Common.h"
#include "imgui.h"

class AppGui
{
public:
    static bool Init( const string& app_name, bool use_dx, bool maximized );
    static bool BeginFrame();
    static void EndFrame();

private:
    #ifdef FO_HAVE_DX
    static bool InitDX( bool maximized );
    static bool BeginFrameDX();
    static void EndFrameDX();
    #endif
};
