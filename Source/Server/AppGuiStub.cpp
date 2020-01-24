#include "AppGui.h"

bool AppGui::Init(const string& app_name, bool use_dx, bool docking, bool maximized)
{
    return true;
}

bool AppGui::BeginFrame()
{
    return true;
}

void AppGui::EndFrame()
{
}

#ifdef FO_HAVE_DX
bool AppGui::InitDX(const string& app_name, bool docking, bool maximized)
{
    return true;
}

bool AppGui::BeginFrameDX()
{
    return true;
}

void AppGui::EndFrameDX()
{
}
#endif
