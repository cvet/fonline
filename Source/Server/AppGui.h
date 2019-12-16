#pragma once

#include "Common.h"
#include "imgui.h"
#include "imgui_internal.h"

class AppGui
{
public:
    static bool Init(const string& app_name, bool use_dx, bool docking, bool maximized);
    static bool BeginFrame();
    static void EndFrame();

private:
#ifdef FO_HAVE_DX
    static bool InitDX(const string& app_name, bool docking, bool maximized);
    static bool BeginFrameDX();
    static void EndFrameDX();
#endif
};
