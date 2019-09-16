#include "GraphicApi.h"
#include "Log.h"
#include "Testing.h"

bool OGL_version_2_0 = false;
bool OGL_vertex_buffer_object = false;
bool OGL_framebuffer_object = false;
bool OGL_framebuffer_object_ext = false;
bool OGL_framebuffer_multisample = false;
bool OGL_texture_multisample = false;
bool OGL_vertex_array_object = false;
bool OGL_get_program_binary = false;

bool GraphicApi::Init()
{
    // Initialize GLEW
    #ifndef FO_OGL_ES
    GLenum glew_result = glewInit();
    if( glew_result != GLEW_OK )
    {
        WriteLog( "GLEW not initialized, result {}.\n", glew_result );
        return false;
    }
    OGL_version_2_0 = GLEW_VERSION_2_0 != 0;
    OGL_vertex_buffer_object = GLEW_ARB_vertex_buffer_object != 0;
    OGL_framebuffer_object = GLEW_ARB_framebuffer_object != 0;
    OGL_framebuffer_object_ext = GLEW_EXT_framebuffer_object != 0;
    OGL_framebuffer_multisample = GLEW_EXT_framebuffer_multisample != 0;
    OGL_texture_multisample = GLEW_ARB_texture_multisample != 0;
    # ifdef FO_MAC
    OGL_vertex_array_object = GLEW_APPLE_vertex_array_object != 0;
    # else
    OGL_vertex_array_object = GLEW_ARB_vertex_array_object != 0;
    # endif
    OGL_get_program_binary = GLEW_ARB_get_program_binary != 0;
    #endif

    // OpenGL ES extensions
    #ifdef FO_OGL_ES
    OGL_version_2_0 = true;
    OGL_vertex_buffer_object = true;
    OGL_framebuffer_object = true;
    OGL_framebuffer_object_ext = false;
    OGL_framebuffer_multisample = false;
    OGL_texture_multisample = false;
    OGL_vertex_array_object = false;
    OGL_get_program_binary = false;
    # ifdef FO_ANDROID
    OGL_vertex_array_object = true;
    OGL_framebuffer_multisample = true;
    OGL_texture_multisample = true;
    # endif
    # ifdef FO_IOS
    OGL_vertex_array_object = true;
    OGL_framebuffer_multisample = true;
    # endif
    #endif

    return true;
}
