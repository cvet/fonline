#include "StdAfx.h"
#include "SpriteManager.h"
#include "IniParser.h"
#include "Crypt.h"
#include "F2Palette.h"

SpriteManager SprMngr;
AnyFrames*    SpriteManager::DummyAnimation = NULL;

#define MAX_ATLAS_SIZE           ( 4096 )
#define SPRITES_BUFFER_SIZE      ( 10000 )
#define ATLAS_SPRITES_PADDING    ( 1 )
#define ARRAY_BUFFERS_COUNT      ( 300 )

#define FAST_FORMAT_SIGNATURE    ( 0xDEADBEEF ) // Must be really unique
#define FAST_FORMAT_VERSION      ( 1 )

bool OGL_version_2_0 = false;
bool OGL_vertex_buffer_object = false;
bool OGL_framebuffer_object = false;
bool OGL_framebuffer_object_ext = false;
bool OGL_framebuffer_multisample = false;
bool OGL_packed_depth_stencil = false;
bool OGL_texture_multisample = false;
bool OGL_vertex_array_object = false;
bool OGL_get_program_binary = false;

SpriteManager::SpriteManager()
{
    drawQuadCount = 0;
    curDrawQuad = 0;
    sceneBeginned = false;

    sprEgg = NULL;
    eggValid = false;
    eggHx = eggHy = eggX = eggY = 0;
    eggAtlasWidth = 0;
    eggAtlasHeight = 0;
    eggSprWidth = 0;
    eggSprHeight = 0;

    contoursAdded = false;
    modeWidth = 0;
    modeHeight = 0;
    curViewportWidth = 0;
    curViewportHeight = 0;

    baseColor = COLOR_RGBA( 255, 128, 128, 128 );
    allAtlases.reserve( 100 );
    dipQueue.reserve( 1000 );

    rtMain = NULL;
    rtContours = rtContoursMid = NULL;

    quadsVertexArray = NULL;
    pointsVertexArray = NULL;

    for( uint i = 0; i < sizeof( FoPalette ); i += 4 )
        std::swap( FoPalette[ i ], FoPalette[ i + 2 ] );

    atlasWidth = atlasHeight = 0;
    accumulatorActive = false;
}

bool SpriteManager::Init()
{
    WriteLog( "Sprite manager initialization...\n" );

    drawQuadCount = 1024;
    curDrawQuad = 0;
    modeWidth = GameOpt.ScreenWidth;
    modeHeight = GameOpt.ScreenHeight;

    // Initialize window
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 0 );
    SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 0 );
    SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
    uint window_create_flags = ( SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN );
    window_create_flags |= ( GameOpt.FullScreen ? SDL_WINDOW_FULLSCREEN : 0 );
    #ifdef FO_OSX_IOS
    window_create_flags |= SDL_WINDOW_FULLSCREEN;
    window_create_flags |= SDL_WINDOW_BORDERLESS;
    #endif
    #ifdef FO_OGL_ES
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
    #endif
    MainWindow = SDL_CreateWindow( GetWindowName(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, modeWidth, modeHeight, window_create_flags );
    if( !MainWindow )
    {
        WriteLog( "SDL Window not created, error<%s>.\n", SDL_GetError() );
        return false;
    }

    #ifdef FO_OSX_IOS
    GLContext = SDL_GL_CreateContext( MainWindow );     // Strange workaround, does not wont work without it
    #endif
    GLContext = SDL_GL_CreateContext( MainWindow );
    if( !GLContext )
    {
        WriteLog( "OpenGL context not created, error<%s>.\n", SDL_GetError() );
        return false;
    }
    if( SDL_GL_MakeCurrent( MainWindow, GLContext ) < 0 )
    {
        WriteLog( "Can't set current context, error<%s>.\n", SDL_GetError() );
        return false;
    }

    SDL_ShowCursor( 0 );
    if( GameOpt.FullScreen )
        SDL_SetWindowFullscreen( MainWindow, 1 );
    SDL_GL_SetSwapInterval( GameOpt.VSync ? 1 : 0 );
    GL( glGetIntegerv( GL_FRAMEBUFFER_BINDING, &baseFBO ) );

    // Calculate atlas size
    GLint max_texture_size;
    GL( glGetIntegerv( GL_MAX_TEXTURE_SIZE, &max_texture_size ) );
    max_texture_size = min( max_texture_size, MAX_ATLAS_SIZE );
    atlasWidth = atlasHeight = max_texture_size;
    #ifdef FO_OSX_IOS
    atlasWidth = min( 2048, max_texture_size );
    atlasHeight = min( 3584, max_texture_size );
    #endif

    // Initialize GLEW
    #ifndef FO_OGL_ES
    GLenum glew_result = glewInit();
    if( glew_result != GLEW_OK )
    {
        WriteLog( "GLEW not initialized, result<%u>.\n", glew_result );
        return false;
    }
    OGL_version_2_0 = GLEW_VERSION_2_0 != 0;
    OGL_vertex_buffer_object = GLEW_ARB_vertex_buffer_object != 0;
    OGL_framebuffer_object = GLEW_ARB_framebuffer_object != 0;
    OGL_framebuffer_object_ext = GLEW_EXT_framebuffer_object != 0;
    OGL_framebuffer_multisample = GLEW_EXT_framebuffer_multisample != 0;
    OGL_packed_depth_stencil = GLEW_EXT_packed_depth_stencil != 0;
    OGL_texture_multisample = GLEW_ARB_texture_multisample != 0;
    OGL_vertex_array_object = GLEW_ARB_vertex_array_object != 0;
    OGL_get_program_binary = GLEW_ARB_get_program_binary != 0;
    #endif

    // OpenGL ES extensions
    #ifdef FO_OGL_ES
    OGL_version_2_0 = true;
    OGL_vertex_buffer_object = true;
    OGL_framebuffer_object = true;       // GL_OES_framebuffer_object
    OGL_framebuffer_object_ext = false;
    OGL_framebuffer_multisample = false; // APPLE_framebuffer_multisample
    OGL_packed_depth_stencil = false;
    OGL_texture_multisample = false;
    OGL_vertex_array_object = true;      // OES_vertex_array_object
    OGL_get_program_binary = false;      // GL_OES_get_program_binary
    #endif

    // Check OpenGL extensions
    #define CHECK_EXTENSION( ext, critical  )                                     \
        if( !GL_HAS( ext ) )                                                      \
        {                                                                         \
            const char* msg = ( critical ? "Critical" : "Not critical" );         \
            WriteLog( "OpenGL extension '" # ext "' not supported. %s.\n", msg ); \
            if( critical )                                                        \
                extension_errors++;                                               \
        }
    uint extension_errors = 0;
    CHECK_EXTENSION( version_2_0, true );
    CHECK_EXTENSION( vertex_buffer_object, true );
    CHECK_EXTENSION( vertex_array_object, false );
    CHECK_EXTENSION( framebuffer_object, false );
    if( !GL_HAS( framebuffer_object ) )
    {
        CHECK_EXTENSION( framebuffer_object_ext, true );
        CHECK_EXTENSION( framebuffer_multisample, false );
        CHECK_EXTENSION( packed_depth_stencil, false );
    }
    CHECK_EXTENSION( texture_multisample, false );
    CHECK_EXTENSION( get_program_binary, false );
    #undef CHECK_EXTENSION
    if( extension_errors )
        return false;

    // Default effects
    if( !GraphicLoader::LoadDefaultEffects() )
        return false;

    // 3d stuff
    if( !GameOpt.Disable3dRendering && !Animation3d::StartUp() )
    {
        WriteLog( "Can't initialize 3d rendering stuff.\n" );
        return false;
    }

    // Render states
    GL( gluStuffOrtho( projectionMatrixCM[ 0 ], 0.0f, (float) modeWidth, (float) modeHeight, 0.0f, -1.0f, 1.0f ) );
    projectionMatrixCM.Transpose();         // Convert to column major order
    GL( glClearColor( 0.0f, 0.0f, 0.0f, 1.0f ) );
    GL( glDisable( GL_DEPTH_TEST ) );
    GL( glEnable( GL_BLEND ) );
    GL( glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ) );
    GL( glDisable( GL_CULL_FACE ) );
    GL( glActiveTexture( GL_TEXTURE0 ) );
    GL( glPixelStorei( GL_PACK_ALIGNMENT, 1 ) );
    GL( glPixelStorei( GL_UNPACK_ALIGNMENT, 1 ) );
    #ifndef FO_OGL_ES
    GL( glEnable( GL_POINT_SMOOTH ) );
    GL( glEnable( GL_LINE_SMOOTH ) );
    #endif

    // Vertex arrays
    quadsVertexArray = new VertexArray[ ARRAY_BUFFERS_COUNT ];
    pointsVertexArray = new VertexArray[ ARRAY_BUFFERS_COUNT ];
    for( uint i = 0; i <  ARRAY_BUFFERS_COUNT; i++ )
    {
        VertexArray* qva = &quadsVertexArray[ i ];
        memzero( qva, sizeof( VertexArray ) );
        qva->Next = &quadsVertexArray[ i + 1 < ARRAY_BUFFERS_COUNT ? i + 1 : 0 ];
        InitVertexArray( qva, true, drawQuadCount );

        VertexArray* dva = &pointsVertexArray[ i ];
        memzero( dva, sizeof( VertexArray ) );
        dva->Next = &pointsVertexArray[ i + 1 < ARRAY_BUFFERS_COUNT ? i + 1 : 0 ];
        InitVertexArray( dva, false, drawQuadCount );
    }

    // Vertex buffer
    vBuffer.resize( drawQuadCount * 4 );

    // Sprites buffer
    sprData.resize( SPRITES_BUFFER_SIZE );
    for( auto it = sprData.begin(), end = sprData.end(); it != end; ++it )
        ( *it ) = NULL;

    // Render targets
    rtMain = CreateRenderTarget( false, false, 0, 0, true );
    rtContours = CreateRenderTarget( false, false, 0, 0, false );
    rtContoursMid = CreateRenderTarget( false, false, 0, 0, false );

    // Clear scene
    GL( glClear( GL_COLOR_BUFFER_BIT ) );
    SDL_GL_SwapWindow( MainWindow );
    if( rtMain )
        PushRenderTarget( rtMain );

    // Generate dummy animation
    if( !DummyAnimation )
        DummyAnimation = AnyFrames::Create( 1, 100 );

    WriteLog( "Sprite manager initialization complete.\n" );
    return true;
}

void SpriteManager::Finish()
{
    WriteLog( "Sprite manager finish...\n" );

    DeleteRenderTarget( rtMain );
    DeleteRenderTarget( rtContours );
    DeleteRenderTarget( rtContoursMid );
    for( auto it = rt3D.begin(), end = rt3D.end(); it != end; ++it )
        DeleteRenderTarget( *it );
    rt3D.clear();

    for( auto it = allAtlases.begin(), end = allAtlases.end(); it != end; ++it )
        SAFEDEL( *it );
    allAtlases.clear();
    for( auto it = sprData.begin(), end = sprData.end(); it != end; ++it )
        SAFEDEL( *it );
    sprData.clear();
    dipQueue.clear();

    if( !GameOpt.Disable3dRendering )
        Animation3d::Finish();

    WriteLog( "Sprite manager finish complete.\n" );
}

bool SpriteManager::BeginScene( uint clear_color )
{
    // Render 3d animations
    if( !GameOpt.Disable3dRendering && !autoRedrawAnim3d.empty() )
    {
        for( auto it = autoRedrawAnim3d.begin(), end = autoRedrawAnim3d.end(); it != end; ++it )
        {
            Animation3d* anim3d = *it;
            if( anim3d->NeedDraw() )
                Render3d( anim3d );
        }
    }

    // Clear window
    if( clear_color )
        ClearCurrentRenderTarget( clear_color );

    sceneBeginned = true;
    return true;
}

void SpriteManager::EndScene()
{
    Flush();


    if( rtMain )
    {
        PopRenderTarget();
        DrawRenderTarget( rtMain, false );
        PushRenderTarget( rtMain );
    }

    SDL_GL_SwapWindow( MainWindow );

    if( GameOpt.OpenGLDebug && glGetError() != GL_NO_ERROR )
    {
        WriteLogF( _FUNC_, " - Unknown place of OpenGL error.\n" );
        ExitProcess( 0 );
    }
    sceneBeginned = false;
}

RenderTarget* SpriteManager::CreateRenderTarget( bool depth_stencil, bool multisampling, uint width, uint height, bool tex_linear )
{
    // Zero data
    RenderTarget* rt = new RenderTarget();
    memzero( rt, sizeof( RenderTarget ) );
    width = ( width ? width : modeWidth );
    height = ( height ? height : modeHeight );

    // Multisampling
    static int samples = -1;
    if( multisampling && samples == -1 && ( GL_HAS( framebuffer_object ) || ( GL_HAS( framebuffer_object_ext ) && GL_HAS( framebuffer_multisample ) ) ) )
    {
        if( GL_HAS( texture_multisample ) && GameOpt.MultiSampling != 0 )
        {
            // Samples count
            GLint max_samples = 0;
            GL( glGetIntegerv( GL_MAX_COLOR_TEXTURE_SAMPLES, &max_samples ) );
            if( GameOpt.MultiSampling < 0 )
                GameOpt.MultiSampling = 8;
            samples = min( GameOpt.MultiSampling, max_samples );

            // Flush effect
            Effect::FlushRenderTargetMSDefault = GraphicLoader::LoadEffect( "Flush_RenderTargetMS.glsl", true );
            if( Effect::FlushRenderTargetMSDefault )
                Effect::FlushRenderTargetMS = new Effect( *Effect::FlushRenderTargetMSDefault );
            else
                samples = 0;
        }
        else
        {
            samples = 0;
        }
    }
    if( multisampling && samples <= 1 )
    {
        multisampling = false;
        samples = 0;
    }

    // Framebuffer
    if( GL_HAS( framebuffer_object ) )
    {
        GL( glGenFramebuffers( 1, &rt->FBO ) );
        GL( glBindFramebuffer( GL_FRAMEBUFFER, rt->FBO ) );
    }
    else // framebuffer_object_ext
    {
        GL( glGenFramebuffersEXT( 1, &rt->FBO ) );
        GL( glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, rt->FBO ) );
    }

    // Texture
    Texture* tex = new Texture();
    tex->Width = width;
    tex->Height = height;
    tex->SizeData[ 0 ] = (float) width;
    tex->SizeData[ 1 ] = (float) height;
    tex->SizeData[ 2 ] = 1.0f / tex->SizeData[ 0 ];
    tex->SizeData[ 3 ] = 1.0f / tex->SizeData[ 1 ];
    GL( glGenTextures( 1, &tex->Id ) );
    if( !multisampling )
    {
        GL( glBindTexture( GL_TEXTURE_2D, tex->Id ) );
        GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex_linear ? GL_LINEAR : GL_NEAREST ) );
        GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex_linear ? GL_LINEAR : GL_NEAREST ) );
        GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP ) );
        GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP ) );
        GL( glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL ) );
        if( GL_HAS( framebuffer_object ) )
        {
            GL( glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex->Id, 0 ) );
        }
        else // framebuffer_object_ext
        {
            GL( glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, tex->Id, 0 ) );
        }
    }
    else
    {
        tex->Samples = (float) samples;
        GL( glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, tex->Id ) );
        GL( glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA, width, height, GL_TRUE ) );
        if( GL_HAS( framebuffer_object ) )
        {
            GL( glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, tex->Id, 0 ) );
        }
        else // framebuffer_object_ext
        {
            GL( glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D_MULTISAMPLE, tex->Id, 0 ) );
        }
    }
    rt->TargetTexture = tex;

    // Depth / stencil
    if( depth_stencil )
    {
        if( GL_HAS( framebuffer_object ) )
        {
            GL( glGenRenderbuffers( 1, &rt->DepthStencilBuffer ) );
            GL( glBindRenderbuffer( GL_RENDERBUFFER, rt->DepthStencilBuffer ) );
            if( !multisampling )
            {
                GL( glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height ) );
            }
            else
            {
                GL( glRenderbufferStorageMultisample( GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height ) );
            }
            GL( glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->DepthStencilBuffer ) );
            GL( glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt->DepthStencilBuffer ) );
        }
        else // framebuffer_object_ext
        {
            GL( glGenRenderbuffersEXT( 1, &rt->DepthStencilBuffer ) );
            GL( glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, rt->DepthStencilBuffer ) );
            if( !multisampling )
            {
                if( GL_HAS( packed_depth_stencil ) )
                {
                    GL( glRenderbufferStorageEXT( GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT, width, height ) );
                }
                else
                {
                    GL( glRenderbufferStorageEXT( GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, width, height ) );
                }
            }
            else
            {
                if( GL_HAS( packed_depth_stencil ) )
                {
                    GL( glRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER_EXT, samples, GL_DEPTH24_STENCIL8_EXT, width, height ) );
                }
                else
                {
                    GL( glRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER_EXT, samples, GL_DEPTH_COMPONENT, width, height ) );
                }
            }
            GL( glFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rt->DepthStencilBuffer ) );
            if( GL_HAS( packed_depth_stencil ) )
                GL( glFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rt->DepthStencilBuffer ) );
        }
    }

    // Check framebuffer creation status
    if( GL_HAS( framebuffer_object ) )
    {
        GLenum status;
        GL( status = glCheckFramebufferStatus( GL_FRAMEBUFFER ) );
        if( status != GL_FRAMEBUFFER_COMPLETE )
        {
            WriteLogF( _FUNC_, " - Framebuffer not created, status<%08X>.\n", status );
            return NULL;
        }
    }
    else // framebuffer_object_ext
    {
        GLenum status;
        GL( status = glCheckFramebufferStatusEXT( GL_FRAMEBUFFER_EXT ) );
        if( status != GL_FRAMEBUFFER_COMPLETE_EXT )
        {
            WriteLogF( _FUNC_, " - FramebufferExt not created, status<%08X>.\n", status );
            return NULL;
        }
    }

    // Effect
    if( !multisampling )
        rt->DrawEffect = Effect::FlushRenderTarget;
    else
        rt->DrawEffect = Effect::FlushRenderTargetMS;

    // Clear
    if( GL_HAS( framebuffer_object ) )
    {
        GL( glBindFramebuffer( GL_FRAMEBUFFER, baseFBO ) );
    }
    else // framebuffer_object_ext
    {
        GL( glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, baseFBO ) );
    }
    PushRenderTarget( rt );
    ClearCurrentRenderTarget( 0 );
    if( depth_stencil )
        ClearCurrentRenderTargetDS( true, true );
    PopRenderTarget();

    return rt;
}

void SpriteManager::DeleteRenderTarget( RenderTarget*& rt )
{
    if( !rt )
        return;

    if( GL_HAS( framebuffer_object ) )
    {
        if( rt->DepthStencilBuffer )
            GL( glDeleteRenderbuffers( 1, &rt->DepthStencilBuffer ) );
        GL( glDeleteFramebuffers( 1, &rt->FBO ) );
    }
    else // framebuffer_object_ext
    {
        if( rt->DepthStencilBuffer )
            GL( glDeleteRenderbuffersEXT( 1, &rt->DepthStencilBuffer ) );
        GL( glDeleteFramebuffersEXT( 1, &rt->FBO ) );
    }
    SAFEDEL( rt->TargetTexture );
    SAFEDEL( rt );
}

void SpriteManager::PushRenderTarget( RenderTarget* rt )
{
    Flush();

    bool redundant = ( !rtStack.empty() && rtStack.back() == rt );
    rtStack.push_back( rt );
    if( !redundant )
    {
        Flush();
        if( GL_HAS( framebuffer_object ) )
        {
            GL( glBindFramebuffer( GL_FRAMEBUFFER, rt->FBO ) );
        }
        else // framebuffer_object_ext
        {
            GL( glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, rt->FBO ) );
        }
        RefreshViewport();
    }
}

void SpriteManager::PopRenderTarget()
{
    bool redundant = ( rtStack.size() > 2 && rtStack.back() == rtStack[ rtStack.size() - 2 ] );
    rtStack.pop_back();
    if( !redundant )
    {
        Flush();
        if( GL_HAS( framebuffer_object ) )
        {
            GL( glBindFramebuffer( GL_FRAMEBUFFER, rtStack.empty() ? baseFBO : rtStack.back()->FBO ) );
        }
        else // framebuffer_object_ext
        {
            GL( glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, rtStack.empty() ? baseFBO : rtStack.back()->FBO ) );
        }
        RefreshViewport();
    }
}

void SpriteManager::DrawRenderTarget( RenderTarget* rt, bool alpha_blend, const Rect* region_from /* = NULL */, const Rect* region_to /* = NULL */ )
{
    Flush();

    if( !region_from && !region_to )
    {
        float w = (float) rt->TargetTexture->Width;
        float h = (float) rt->TargetTexture->Height;
        uint  pos = 0;
        vBuffer[ pos ].X = 0.0f;
        vBuffer[ pos ].Y = h;
        vBuffer[ pos ].TU = 0.0f;
        vBuffer[ pos++ ].TV = 0.0f;
        vBuffer[ pos ].X = 0.0f;
        vBuffer[ pos ].Y = 0.0f;
        vBuffer[ pos ].TU = 0.0f;
        vBuffer[ pos++ ].TV = 1.0f;
        vBuffer[ pos ].X = w;
        vBuffer[ pos ].Y = 0.0f;
        vBuffer[ pos ].TU = 1.0f;
        vBuffer[ pos++ ].TV = 1.0f;
        vBuffer[ pos ].X = w;
        vBuffer[ pos ].Y = h;
        vBuffer[ pos ].TU = 1.0f;
        vBuffer[ pos++ ].TV = 0.0f;
    }
    else
    {
        RectF regionf = ( region_from ? *region_from :
                          Rect( 0, 0, rt->TargetTexture->Width, rt->TargetTexture->Height ) );
        RectF regiont = ( region_to ? *region_to :
                          Rect( 0, 0, rtStack.back()->TargetTexture->Width, rtStack.back()->TargetTexture->Height ) );
        float wf = (float) rt->TargetTexture->Width;
        float hf = (float) rt->TargetTexture->Height;
        uint  pos = 0;
        vBuffer[ pos ].X = regiont.L;
        vBuffer[ pos ].Y = regiont.B;
        vBuffer[ pos ].TU = regionf.L / wf;
        vBuffer[ pos++ ].TV = 1.0f - regionf.B / hf;
        vBuffer[ pos ].X = regiont.L;
        vBuffer[ pos ].Y = regiont.T;
        vBuffer[ pos ].TU = regionf.L / wf;
        vBuffer[ pos++ ].TV = 1.0f - regionf.T / hf;
        vBuffer[ pos ].X = regiont.R;
        vBuffer[ pos ].Y = regiont.T;
        vBuffer[ pos ].TU = regionf.R / wf;
        vBuffer[ pos++ ].TV = 1.0f - regionf.T / hf;
        vBuffer[ pos ].X = regiont.R;
        vBuffer[ pos ].Y = regiont.B;
        vBuffer[ pos ].TU = regionf.R / wf;
        vBuffer[ pos++ ].TV = 1.0f - regionf.B / hf;
    }

    curDrawQuad = 1;
    dipQueue.push_back( DipData( rt->TargetTexture, rt->DrawEffect ) );

    if( !alpha_blend )
        GL( glDisable( GL_BLEND ) );
    Flush();
    if( !alpha_blend )
        GL( glEnable( GL_BLEND ) );
}

uint SpriteManager::GetRenderTargetPixel( RenderTarget* rt, int x, int y )
{
    static RenderTarget* last_rt = NULL;
    static int           last_x = 0, last_y = 0;
    static uint          last_result = 0;
    if( rt == last_rt && x == last_x && y == last_y )
        return last_result;

    PushRenderTarget( rt );
    uint result = 0;
    GL( glReadPixels( x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &result ) );
    PopRenderTarget();

    last_rt = rt;
    last_x = x;
    last_y = y;
    last_result = result;
    return result;
}

void SpriteManager::ClearCurrentRenderTarget( uint color )
{
    float a = (float) ( ( color >> 24 ) & 0xFF ) / 255.0f;
    float r = (float) ( ( color >> 16 ) & 0xFF ) / 255.0f;
    float g = (float) ( ( color >>  8 ) & 0xFF ) / 255.0f;
    float b = (float) ( ( color >>  0 ) & 0xFF ) / 255.0f;
    GL( glClearColor( r, g, b, a ) );
    GL( glClear( GL_COLOR_BUFFER_BIT ) );
}

void SpriteManager::ClearCurrentRenderTargetDS( bool depth, bool stencil )
{
    int depth_stencil_bit = 0;
    depth_stencil_bit |= ( depth ? GL_DEPTH_BUFFER_BIT : 0 );     // Clear to value 1
    depth_stencil_bit |= ( stencil ? GL_STENCIL_BUFFER_BIT : 0 ); // Clear to value 0
    GL( glClear( depth_stencil_bit ) );
}

void SpriteManager::RefreshViewport( int w /* = 0 */, int h /* = 0 */ )
{
    if( !w && !h )
    {
        if( !rtStack.empty() )
        {
            w = rtStack.back()->TargetTexture->Width;
            h = rtStack.back()->TargetTexture->Height;
        }
        else
        {
            SDL_GetWindowSize( MainWindow, &w, &h );
        }
    }

    if( curViewportWidth == w && curViewportHeight == h )
        return;

    GL( glViewport( 0, 0, w, h ) );

    GL( gluStuffOrtho( projectionMatrixCM[ 0 ], 0.0f, (float) w, (float) h, 0.0f, -1.0f, 1.0f ) );
    projectionMatrixCM.Transpose();                 // Convert to column major order
}

RenderTarget* SpriteManager::Get3dRenderTarget( uint width, uint height )
{
    for( auto it = rt3D.begin(), end = rt3D.end(); it != end; ++it )
    {
        RenderTarget* rt = *it;
        if( rt->TargetTexture->Width == width && rt->TargetTexture->Height == height )
            return rt;
    }
    rt3D.push_back( CreateRenderTarget( true, true, width, height, false ) );
    return rt3D.back();
}

void SpriteManager::InitVertexArray( VertexArray* va, bool quads, uint count )
{
    // Resize indicies
    UShortVec& indicies = ( quads ? quadsIndicies : pointsIndicies );
    uint       indicies_count = ( quads ? count * 6 : count );
    uint       max_indicies_count = (uint) indicies.size();
    if( indicies_count > max_indicies_count )
    {
        indicies.resize( indicies_count );
        if( quads )
        {
            for( uint i = max_indicies_count / 6, j = indicies_count / 6; i < j; i++ )
            {
                indicies[ i * 6 + 0 ] = i * 4 + 0;
                indicies[ i * 6 + 1 ] = i * 4 + 1;
                indicies[ i * 6 + 2 ] = i * 4 + 3;
                indicies[ i * 6 + 3 ] = i * 4 + 1;
                indicies[ i * 6 + 4 ] = i * 4 + 2;
                indicies[ i * 6 + 5 ] = i * 4 + 3;
            }
        }
        else
        {
            for( uint i = max_indicies_count; i < indicies_count; i++ )
                indicies[ i ] = i;
        }
    }

    // Vertex buffer
    uint vertices_count = ( quads ? count * 4 : count );
    if( !va->VBO )
        GL( glGenBuffers( 1, &va->VBO ) );
    GL( glBindBuffer( GL_ARRAY_BUFFER, va->VBO ) );
    GL( glBufferData( GL_ARRAY_BUFFER, vertices_count * sizeof( Vertex2D ), NULL, GL_DYNAMIC_DRAW ) );
    va->VCount = vertices_count;

    // Index buffer
    if( !va->IBO )
        GL( glGenBuffers( 1, &va->IBO ) );
    GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, va->IBO ) );
    GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, indicies_count * sizeof( ushort ), &indicies[ 0 ], GL_STATIC_DRAW ) );
    va->ICount = indicies_count;

    // Vertex array
    if( !va->VAO && GL_HAS( vertex_array_object ) && ( GL_HAS( framebuffer_object ) || GL_HAS( framebuffer_object_ext ) ) )
    {
        GL( glGenVertexArrays( 1, &va->VAO ) );
        GL( glBindVertexArray( va->VAO ) );
        BindVertexArray( va );
        GL( glBindVertexArray( 0 ) );
    }
}

void SpriteManager::BindVertexArray( VertexArray* va )
{
    GL( glBindBuffer( GL_ARRAY_BUFFER, va->VBO ) );
    GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, va->IBO ) );
    GL( glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex2D ), (void*) (size_t) OFFSETOF( Vertex2D, X ) ) );
    GL( glVertexAttribPointer( 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( Vertex2D ), (void*) (size_t) OFFSETOF( Vertex2D, Diffuse ) ) );
    GL( glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex2D ), (void*) (size_t) OFFSETOF( Vertex2D, TU ) ) );
    GL( glEnableVertexAttribArray( 0 ) );
    GL( glEnableVertexAttribArray( 1 ) );
    GL( glEnableVertexAttribArray( 2 ) );
    #ifndef DISABLE_EGG
    GL( glVertexAttribPointer( 3, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex2D ), (void*) (size_t) OFFSETOF( Vertex2D, TUEgg ) ) );
    GL( glEnableVertexAttribArray( 3 ) );
    #endif
}

void SpriteManager::EnableVertexArray( VertexArray* va, uint vertices_count )
{
    if( va->VAO )
    {
        GL( glBindVertexArray( va->VAO ) );
        GL( glBindBuffer( GL_ARRAY_BUFFER, va->VBO ) );
        GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, va->IBO ) );
    }
    else
    {
        BindVertexArray( va );
    }
    GL( glBufferSubData( GL_ARRAY_BUFFER, 0, vertices_count * sizeof( Vertex2D ), &vBuffer[ 0 ] ) );
}

void SpriteManager::DisableVertexArray( VertexArray*& va )
{
    if( va->VAO )
    {
        GL( glBindVertexArray( 0 ) );
    }
    else
    {
        GL( glDisableVertexAttribArray( 0 ) );
        GL( glDisableVertexAttribArray( 1 ) );
        GL( glDisableVertexAttribArray( 2 ) );
        GL( glDisableVertexAttribArray( 3 ) );
    }
    if( va->Next )
        va = va->Next;
}

void SpriteManager::EnableStencil( RectF& stencil )
{
    uint pos = 0;
    vBuffer[ pos ].X = stencil.L;
    vBuffer[ pos ].Y = stencil.B;
    vBuffer[ pos++ ].Diffuse = 0xFFFFFFFF;
    vBuffer[ pos ].X = stencil.L;
    vBuffer[ pos ].Y = stencil.T;
    vBuffer[ pos++ ].Diffuse = 0xFFFFFFFF;
    vBuffer[ pos ].X = stencil.R;
    vBuffer[ pos ].Y = stencil.T;
    vBuffer[ pos++ ].Diffuse = 0xFFFFFFFF;
    vBuffer[ pos ].X = stencil.R;
    vBuffer[ pos ].Y = stencil.B;
    vBuffer[ pos++ ].Diffuse = 0xFFFFFFFF;

    GL( glEnable( GL_STENCIL_TEST ) );
    GL( glStencilFunc( GL_NEVER, 1, 0xFF ) );
    GL( glStencilOp( GL_REPLACE, GL_KEEP, GL_KEEP ) );

    curDrawQuad = 1;
    dipQueue.push_back( DipData( NULL, Effect::FlushPrimitive ) );
    Flush();

    GL( glStencilFunc( GL_NOTEQUAL, 0, 0xFF ) );
    GL( glStencilOp( GL_REPLACE, GL_KEEP, GL_KEEP ) );
}

void SpriteManager::DisableStencil( bool clear_stencil )
{
    if( clear_stencil )
        ClearCurrentRenderTargetDS( false, true );
    GL( glDisable( GL_STENCIL_TEST ) );
}

void SpriteManager::PushAtlasType( int atlas_type, bool one_image /* = false */ )
{
    atlasTypeStack.push_back( atlas_type );
    atlasOneImageStack.push_back( one_image );
}

void SpriteManager::PopAtlasType()
{
    atlasTypeStack.pop_back();
    atlasOneImageStack.pop_back();
}

void SpriteManager::AccumulateAtlasData()
{
    accumulatorActive = true;
}

void SpriteManager::FlushAccumulatedAtlasData()
{
    struct Sorter
    {
        static bool SortBySize( SpriteInfo* si1, SpriteInfo* si2 )
        {
            return si1->Width * si1->Height > si2->Width * si2->Height;
        }
    };
    std::sort( accumulatorSprInfo.begin(), accumulatorSprInfo.end(), Sorter::SortBySize );

    for( auto it = accumulatorSprInfo.begin(), end = accumulatorSprInfo.end(); it != end; ++it )
        FillAtlas( *it );
    accumulatorSprInfo.clear();
    accumulatorActive = false;
}

bool SpriteManager::IsAccumulateAtlasActive()
{
    return accumulatorActive;
}

TextureAtlas* SpriteManager::CreateAtlas( int w, int h )
{
    TextureAtlas* atlas = new TextureAtlas();
    atlas->Type = atlasTypeStack.back();

    if( !atlasOneImageStack.back() )
    {
        w = atlasWidth;
        h = atlasHeight;
    }

    atlas->RT = CreateRenderTarget( false, false, w, h, true );
    atlas->TextureOwner = atlas->RT->TargetTexture;
    atlas->Width = w;
    atlas->Height = h;
    atlas->RootNode = ( accumulatorActive ? new TextureAtlas::SpaceNode( 0, 0, w, h ) : NULL );
    allAtlases.push_back( atlas );
    return atlas;
}

TextureAtlas* SpriteManager::FindAtlasPlace( SpriteInfo* si, int& x, int& y )
{
    // Find place in already created atlas
    TextureAtlas* atlas = NULL;
    int           atlas_type = atlasTypeStack.back();
    uint          w = si->Width + ATLAS_SPRITES_PADDING * 2;
    uint          h = si->Height + ATLAS_SPRITES_PADDING * 2;
    for( auto it = allAtlases.begin(), end = allAtlases.end(); it != end; ++it )
    {
        TextureAtlas* a = *it;
        if( a->Type != atlas_type )
            continue;

        if( a->RootNode  )
        {
            if( a->RootNode->FindPosition( w, h, x, y ) )
                atlas = a;
        }
        else
        {
            if( w <= a->LineW && a->LineCurH + h <= a->LineMaxH )
            {
                x = a->CurX - a->LineW;
                y = a->CurY + a->LineCurH;
                a->LineCurH += h;
                atlas = a;
            }
            else if( a->Width - a->CurX >= w && a->Height - a->CurY >= h )
            {
                x = a->CurX;
                y = a->CurY;
                a->CurX += w;
                a->LineW = w;
                a->LineCurH = h;
                a->LineMaxH = max( a->LineMaxH, h );
                atlas = a;
            }
            else if( a->Width >= w && a->Height - a->CurY - a->LineMaxH >= h )
            {
                x = 0;
                y = a->CurY + a->LineMaxH;
                a->CurX = w;
                a->CurY = y;
                a->LineW = w;
                a->LineCurH = h;
                a->LineMaxH = h;
                atlas = a;
            }
        }

        if( atlas )
            break;
    }

    // Create new
    if( !atlas )
    {
        atlas = CreateAtlas( w, h );
        if( atlas->RootNode )
        {
            atlas->RootNode->FindPosition( w, h, x, y );
        }
        else
        {
            x = atlas->CurX;
            y = atlas->CurY;
            atlas->CurX += w;
            atlas->LineMaxH = h;
            atlas->LineCurH = h;
            atlas->LineW = w;
        }
    }

    // Return parameters
    x += ATLAS_SPRITES_PADDING;
    y += ATLAS_SPRITES_PADDING;
    return atlas;
}

void SpriteManager::DestroyAtlases( int atlas_type )
{
    for( auto it = allAtlases.begin(); it != allAtlases.end();)
    {
        TextureAtlas* atlas = *it;
        if( atlas->Type == atlas_type )
        {
            for( auto it_ = sprData.begin(), end_ = sprData.end(); it_ != end_; ++it_ )
            {
                SpriteInfo* si = *it_;
                if( si && si->Atlas == atlas )
                {
                    if( si->Anim3d )
                        si->Anim3d->SprId = 0;

                    delete si;
                    ( *it_ ) = NULL;
                }
            }

            delete atlas;
            it = allAtlases.erase( it );
        }
        else
        {
            ++it;
        }
    }
}

void SpriteManager::DumpAtlases()
{
    uint atlases_memory_size = 0;
    for( auto it = allAtlases.begin(), end = allAtlases.end(); it != end; ++it )
    {
        TextureAtlas* atlas = *it;
        atlases_memory_size += atlas->Width * atlas->Height * 4;
    }

    char path[ MAX_FOPATH ];
    Str::Format( path, DIR_SLASH_SD "%u_%u.%03umb" DIR_SLASH_S, (uint) time( NULL ), atlases_memory_size / 1000000, atlases_memory_size % 1000000 / 1000 );

    int cnt = 0;
    for( auto it = allAtlases.begin(), end = allAtlases.end(); it != end; ++it )
    {
        TextureAtlas* atlas = *it;
        char          fname[ MAX_FOPATH ];
        Str::Format( fname, "%s%d_%d_%ux%u.png", path, cnt, atlas->Type, atlas->Width, atlas->Height );
        SaveTexture( atlas->TextureOwner, fname, false );
        cnt++;
    }
}

void SpriteManager::SaveTexture( Texture* tex, const char* fname, bool flip )
{
    // Size
    int w = ( tex ? tex->Width : 0 );
    int h = ( tex ? tex->Height : 0 );
    if( !tex )
        SDL_GetWindowSize( MainWindow, &w, &h );

    // Get data
    uchar* data = new uchar[ w * h * 4 ];
    if( tex )
    {
        GL( glBindTexture( GL_TEXTURE_2D, tex->Id ) );
        GL( glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data ) );
        GL( glBindTexture( GL_TEXTURE_2D, 0 ) );
    }
    else
    {
        GL( glReadPixels( 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data ) );
    }

    // Flip image
    if( flip )
    {
        uint* data4 = (uint*) data;
        for( int y = 0; y < h / 2; y++ )
            for( int x = 0; x < w; x++ )
                std::swap( data4[ y * w + x ], data4[ ( h - y - 1 ) * w + x ] );
    }

    // Save
    GraphicLoader::SavePNG( fname, data, w, h );

    // Clean up
    SAFEDELA( data );
}

uint SpriteManager::RequestFillAtlas( SpriteInfo* si, uint w, uint h, uchar* data )
{
    // Sprite info
    if( !si )
        si = new SpriteInfo();

    // Get width, height
    si->Data = data;
    si->DataSize = w * h * 4;
    si->DataAtlasType = atlasTypeStack.back();
    si->DataAtlasOneImage = atlasOneImageStack.back();
    si->Width = w;
    si->Height = h;

    // Find place on atlas
    if( accumulatorActive )
        accumulatorSprInfo.push_back( si );
    else
        FillAtlas( si );

    // Store sprite
    uint index = 1;
    for( uint j = (uint) sprData.size(); index < j; index++ )
        if( !sprData[ index ] )
            break;
    if( index < (uint) sprData.size() )
        sprData[ index ] = si;
    else
        sprData.push_back( si );
    return index;
}

void SpriteManager::FillAtlas( SpriteInfo* si )
{
    uchar* data = si->Data;
    uint   w = si->Width;
    uint   h = si->Height;

    si->Data = NULL;
    si->DataSize = 0;

    PushAtlasType( si->DataAtlasType, si->DataAtlasOneImage );
    int           x, y;
    TextureAtlas* atlas = FindAtlasPlace( si, x, y );
    PopAtlasType();

    // Refresh texture
    if( data )
    {
        // Whole image
        Texture* tex = atlas->TextureOwner;
        tex->UpdateRegion( Rect( x, y, x + w - 1, y + h - 1 ), data );

        // 1px border for correct linear interpolation
        // Top
        tex->UpdateRegion( Rect( x, y - 1, x + w - 1, y - 1 ), data );

        // Bottom
        tex->UpdateRegion( Rect( x, y + h, x + w - 1, y + h ), data + ( h - 1 ) * w * 4 );

        // Left
        uint data_border[ MAX_ATLAS_SIZE ];
        for( uint i = 0; i < h; i++ )
            data_border[ i + 1 ] = *(uint*) ( data + i * w * 4 );
        data_border[ 0 ] = data_border[ 1 ];
        data_border[ h + 1 ] = data_border[ h ];
        tex->UpdateRegion( Rect( x - 1, y - 1, x - 1, y + h + 1 ), (uchar*) data_border );

        // Right
        for( uint i = 0; i < h; i++ )
            data_border[ i + 1 ] = *(uint*) ( data + i * w * 4 + ( w - 1 ) * 4 );
        data_border[ 0 ] = data_border[ 1 ];
        data_border[ h + 1 ] = data_border[ h ];
        tex->UpdateRegion( Rect( x + w, y - 1, x + w, y + h + 1 ), (uchar*) data_border );
    }

    // Set parameters
    si->Atlas = atlas;
    si->SprRect.L = float(x) / float(atlas->Width);
    si->SprRect.T = float(y) / float(atlas->Height);
    si->SprRect.R = float(x + w) / float(atlas->Width);
    si->SprRect.B = float(y + h) / float(atlas->Height);

    // Delete data
    SAFEDELA( data );
}

AnyFrames* SpriteManager::LoadAnimation( const char* fname, int path_type, bool use_dummy /* = false */, bool frm_anim_pix /* = false */ )
{
    AnyFrames* dummy = ( use_dummy ? DummyAnimation : NULL );

    if( !fname || !fname[ 0 ] )
        return dummy;

    const char* ext = FileManager::GetExtension( fname );
    if( !ext )
    {
        WriteLogF( _FUNC_, " - Extension not found, file<%s>.\n", fname );
        return dummy;
    }

    AnyFrames* result = NULL;
    if( Str::CompareCase( ext, "png" ) )
        result = LoadAnimationOther( fname, path_type, &GraphicLoader::LoadPNG );
    else if( Str::CompareCase( ext, "tga" ) )
        result = LoadAnimationOther( fname, path_type, &GraphicLoader::LoadTGA );
    else if( Str::CompareCase( ext, "frm" ) )
        result = LoadAnimationFrm( fname, path_type, frm_anim_pix );
    else if( Str::CompareCase( ext, "rix" ) )
        result = LoadAnimationRix( fname, path_type );
    else if( Str::CompareCase( ext, "fofrm" ) )
        result = LoadAnimationFofrm( fname, path_type );
    else if( Str::CompareCase( ext, "art" ) )
        result = LoadAnimationArt( fname, path_type );
    else if( Str::CompareCase( ext, "spr" ) )
        result = LoadAnimationSpr( fname, path_type );
    else if( Str::CompareCase( ext, "zar" ) )
        result = LoadAnimationZar( fname, path_type );
    else if( Str::CompareCase( ext, "til" ) )
        result = LoadAnimationTil( fname, path_type );
    else if( Str::CompareCase( ext, "mos" ) )
        result = LoadAnimationMos( fname, path_type );
    else if( Str::CompareCase( ext, "bam" ) )
        result = LoadAnimationBam( fname, path_type );
    else if( GraphicLoader::IsExtensionSupported( ext ) )
        result = LoadAnimation3d( fname, path_type );
    else
        WriteLogF( _FUNC_, " - Unsupported image file format<%s>, file<%s>.\n", ext, fname );

    return result ? result : dummy;
}

AnyFrames* SpriteManager::ReloadAnimation( AnyFrames* anim, const char* fname, int path_type )
{
    if( !fname || !fname[ 0 ] )
        return anim;

    // Release old images
    if( anim )
    {
        for( uint i = 0; i < anim->CntFrm; i++ )
        {
            SpriteInfo* si = GetSpriteInfo( anim->Ind[ i ] );
            if( si )
                DestroyAtlases( si->Atlas->Type );
        }
        AnyFrames::Destroy( anim );
    }

    // Load fresh
    return LoadAnimation( fname, path_type );
}

AnyFrames* SpriteManager::LoadAnimationFrm( const char* fname, int path_type, bool anim_pix /* = false */ )
{
    // Load file
    FileManager fm;
    AnyFrames*  fast_anim;
    if( TryLoadAnimationInFastFormat( fname, path_type, fm, fast_anim ) && fm.IsLoaded() )
        return fast_anim;

    // Load from fr0..5
    bool load_from_fr = false;
    if( !fm.IsLoaded() )
    {
        char  fname_[ MAX_FOPATH ];
        Str::Copy( fname_, fname );
        char* ext = (char*) FileManager::GetExtension( fname_ );
        *( ext + 2 ) = '0';
        if( !fm.LoadFile( fname_, path_type ) )
            return NULL;
        load_from_fr = true;
    }

    // Load from frm
    fm.SetCurPos( 0x4 );
    ushort frm_fps = fm.GetBEUShort();
    if( !frm_fps )
        frm_fps = 10;
    fm.SetCurPos( 0x8 );
    ushort     frm_num = fm.GetBEUShort();

    AnyFrames* base_anim = AnyFrames::Create( frm_num, 1000 / frm_fps * frm_num );

    for( int dir = 0; dir < DIRS_COUNT; dir++ )
    {
        if( !GameOpt.MapHexagonal )
        {
            switch( dir )
            {
            default:
            case 0:
                dir = 0;
                break;
            case 1:
                dir = 1;
                break;
            case 2:
            case 3:
                dir = 2;
                break;
            case 4:
                dir = 3;
                break;
            case 5:
                dir = 4;
                break;
            case 6:
            case 7:
                dir = 5;
                break;
            }
        }

        if( dir && load_from_fr )
        {
            char  fname_[ MAX_FOPATH ];
            Str::Copy( fname_, fname );
            char* ext = (char*) FileManager::GetExtension( fname_ );
            *( ext + 2 ) = '0' + dir;
            if( !fm.LoadFile( fname_, path_type ) )
            {
                if( dir > 1 )
                {
                    WriteLogF( _FUNC_, " - File<%s> not found.\n", fname_ );
                    AnyFrames::Destroy( base_anim );
                    return NULL;
                }
                break;
            }
        }

        fm.SetCurPos( 0xA + dir * 2 );
        short offs_x = fm.GetBEShort();
        fm.SetCurPos( 0x16 + dir * 2 );
        short offs_y = fm.GetBEShort();

        fm.SetCurPos( 0x22 + dir * 4 );
        uint offset = 0x3E + fm.GetBEUInt();
        if( offset == 0x3E && dir && !load_from_fr )
        {
            if( dir > 1 )
            {
                WriteLogF( _FUNC_, " - FRM file<%s> truncated.\n", fname );
                AnyFrames::Destroy( base_anim );
                return NULL;
            }
            break;
        }

        // Create animation
        if( dir == 1 )
            base_anim->CreateDirAnims();
        AnyFrames* anim = base_anim->GetDir( dir );

        // Make palette
        uint* palette = (uint*) FoPalette;
        uint  palette_entry[ 256 ];
        char  palette_name[ MAX_FOPATH ];
        Str::Copy( palette_name, fname );
        Str::Copy( (char*) FileManager::GetExtension( palette_name ), 4, "pal" );
        FileManager fm_palette;
        if( fm_palette.LoadFile( palette_name, path_type ) )
        {
            for( uint i = 0; i < 256; i++ )
            {
                uchar r = fm_palette.GetUChar() * 4;
                uchar g = fm_palette.GetUChar() * 4;
                uchar b = fm_palette.GetUChar() * 4;
                palette_entry[ i ] = COLOR_RGB( r, g, b );
            }
            palette = palette_entry;
        }

        // Animate pixels
        int anim_pix_type = 0;
        // 0x00 - None
        // 0x01 - Slime, 229 - 232, 4
        // 0x02 - Monitors, 233 - 237, 5
        // 0x04 - FireSlow, 238 - 242, 5
        // 0x08 - FireFast, 243 - 247, 5
        // 0x10 - Shoreline, 248 - 253, 6
        // 0x20 - BlinkingRed, 254, parse on 15 frames
        const uchar blinking_red_vals[ 10 ] = { 254, 210, 165, 120, 75, 45, 90, 135, 180, 225 };

        for( int frm = 0; frm < frm_num; frm++ )
        {
            SpriteInfo* si = new SpriteInfo();

            fm.SetCurPos( offset );
            ushort w = fm.GetBEUShort();
            ushort h = fm.GetBEUShort();

            fm.GoForward( 4 );                   // Frame size

            si->OffsX = offs_x;
            si->OffsY = offs_y;

            anim->NextX[ frm ] = fm.GetBEShort();
            anim->NextY[ frm ] = fm.GetBEShort();

            // Allocate data
            uchar* data = new uchar[ w * h * 4 ];
            uint*  ptr = (uint*) data;
            fm.SetCurPos( offset + 12 );

            if( !anim_pix_type )
            {
                for( int i = 0, j = w * h; i < j; i++ )
                    *( ptr + i ) = palette[ fm.GetUChar() ];
            }
            else
            {
                for( int i = 0, j = w * h; i < j; i++ )
                {
                    uchar index = fm.GetUChar();
                    if( index >= 229 && index < 255 )
                    {
                        if( index >= 229 && index <= 232 )
                        {
                            index -= frm % 4;
                            if( index < 229 )
                                index += 4;
                        }
                        else if( index >= 233 && index <= 237 )
                        {
                            index -= frm % 5;
                            if( index < 233 )
                                index += 5;
                        }
                        else if( index >= 238 && index <= 242 )
                        {
                            index -= frm % 5;
                            if( index < 238 )
                                index += 5;
                        }
                        else if( index >= 243 && index <= 247 )
                        {
                            index -= frm % 5;
                            if( index < 243 )
                                index += 5;
                        }
                        else if( index >= 248 && index <= 253 )
                        {
                            index -= frm % 6;
                            if( index < 248 )
                                index += 6;
                        }
                        else
                        {
                            *( ptr + i ) = COLOR_RGB( blinking_red_vals[ frm % 10 ], 0, 0 );
                            continue;
                        }
                    }
                    *( ptr + i ) = palette[ index ];
                }
            }

            // Check for animate pixels
            if( !frm && anim_pix && palette == (uint*) FoPalette )
            {
                fm.SetCurPos( offset + 12 );
                for( int i = 0, j = w * h; i < j; i++ )
                {
                    uchar index = fm.GetUChar();
                    if( index < 229 || index == 255 )
                        continue;
                    if( index >= 229 && index <= 232 )
                        anim_pix_type |= 0x01;
                    else if( index >= 233 && index <= 237 )
                        anim_pix_type |= 0x02;
                    else if( index >= 238 && index <= 242 )
                        anim_pix_type |= 0x04;
                    else if( index >= 243 && index <= 247 )
                        anim_pix_type |= 0x08;
                    else if( index >= 248 && index <= 253 )
                        anim_pix_type |= 0x10;
                    else
                        anim_pix_type |= 0x20;
                }

                if( anim_pix_type & 0x01 )
                    anim->Ticks = 200;
                if( anim_pix_type & 0x04 )
                    anim->Ticks = 200;
                if( anim_pix_type & 0x10 )
                    anim->Ticks = 200;
                if( anim_pix_type & 0x08 )
                    anim->Ticks = 142;
                if( anim_pix_type & 0x02 )
                    anim->Ticks = 100;
                if( anim_pix_type & 0x20 )
                    anim->Ticks = 100;

                if( anim_pix_type )
                {
                    int divs[ 4 ];
                    divs[ 0 ] = 1;
                    divs[ 1 ] = 1;
                    divs[ 2 ] = 1;
                    divs[ 3 ] = 1;
                    if( anim_pix_type & 0x01 )
                        divs[ 0 ] = 4;
                    if( anim_pix_type & 0x02 )
                        divs[ 1 ] = 5;
                    if( anim_pix_type & 0x04 )
                        divs[ 1 ] = 5;
                    if( anim_pix_type & 0x08 )
                        divs[ 1 ] = 5;
                    if( anim_pix_type & 0x10 )
                        divs[ 2 ] = 6;
                    if( anim_pix_type & 0x20 )
                        divs[ 3 ] = 10;

                    frm_num = 4;
                    for( int i = 0; i < 4; i++ )
                    {
                        if( !( frm_num % divs[ i ] ) )
                            continue;
                        frm_num++;
                        i = -1;
                    }

                    anim->Ticks *= frm_num;
                    anim->CntFrm = frm_num;
                }
            }

            if( !anim_pix_type )
                offset += w * h + 12;

            uint result = RequestFillAtlas( si, w, h, data );
            if( !result )
            {
                AnyFrames::Destroy( anim );
                return NULL;
            }
            anim->Ind[ frm ] = result;
        }
    }

    return base_anim;
}

AnyFrames* SpriteManager::LoadAnimationRix( const char* fname, int path_type )
{
    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return NULL;

    SpriteInfo* si = new SpriteInfo();
    fm.SetCurPos( 0x4 );
    ushort      w;
    fm.CopyMem( &w, 2 );
    ushort      h;
    fm.CopyMem( &h, 2 );

    // Allocate data
    uchar* data = new uchar[ w * h * 4 ];
    uint*  ptr = (uint*) data;
    fm.SetCurPos( 0xA );
    uchar* palette = fm.GetCurBuf();
    fm.SetCurPos( 0xA + 256 * 3 );
    for( int i = 0, j = w * h; i < j; i++ )
    {
        uint index = fm.GetUChar();
        uint r = *( palette + index * 3 + 2 ) * 4;
        uint g = *( palette + index * 3 + 1 ) * 4;
        uint b = *( palette + index * 3 + 0 ) * 4;
        *( ptr + i ) = COLOR_RGB( r, g, b );
    }

    uint result = RequestFillAtlas( si, w, h, data );
    if( !result )
        return NULL;

    AnyFrames* anim = AnyFrames::Create( 1, 100 );
    anim->Ind[ 0 ] = result;
    return anim;
}

AnyFrames* SpriteManager::LoadAnimationFofrm( const char* fname, int path_type )
{
    // Load file
    FileManager fm;
    AnyFrames*  fast_anim;
    if( TryLoadAnimationInFastFormat( fname, path_type, fm, fast_anim ) )
        return fast_anim;
    if( !fm.IsLoaded() )
        return NULL;

    // Load ini parser
    IniParser fofrm;
    if( !fofrm.LoadFilePtr( (char*) fm.GetBuf(), fm.GetFsize() ) )
        return NULL;

    ushort frm_fps = fofrm.GetInt( "fps", 0 );
    if( !frm_fps )
        frm_fps = 10;

    ushort frm_num = fofrm.GetInt( "count", 0 );
    if( !frm_num )
        frm_num = 1;

    int     ox = fofrm.GetInt( "offs_x", 0 );
    int     oy = fofrm.GetInt( "offs_y", 0 );

    Effect* effect = NULL;
    if( fofrm.IsKey( "effect" ) )
    {
        char effect_name[ MAX_FOPATH ];
        if( fofrm.GetStr( "effect", "", effect_name ) )
            effect = GraphicLoader::LoadEffect( effect_name, true );
    }

    AnimVec anims;
    IntVec  anims_offs;
    anims.reserve( 50 );
    anims_offs.reserve( 100 );

    AnyFrames* base_anim = NULL;
    for( int dir = 0; dir < DIRS_COUNT; dir++ )
    {
        anims.clear();
        anims_offs.clear();

        char dir_str[ 16 ];
        Str::Format( dir_str, "dir_%d", dir );
        bool no_app = !fofrm.IsApp( dir_str );

        if( no_app )
        {
            if( dir == 1 )
                break;

            if( dir > 1 )
            {
                WriteLogF( _FUNC_, " - FOFRM file<%s> invalid apps.\n", fname );
                AnyFrames::Destroy( base_anim );
                return NULL;
            }
        }
        else
        {
            ox = fofrm.GetInt( dir_str, "offs_x", ox );
            oy = fofrm.GetInt( dir_str, "offs_y", oy );
        }

        char  frm_fname[ MAX_FOPATH ];
        FileManager::ExtractPath( fname, frm_fname );
        char* frm_name = frm_fname + Str::Length( frm_fname );

        uint  frames = 0;
        bool  no_info = false;
        bool  load_fail = false;
        for( int frm = 0; frm < frm_num; frm++ )
        {
            if( !fofrm.GetStr( no_app ? NULL : dir_str, Str::FormatBuf( "frm_%d", frm ), "", frm_name ) &&
                ( frm != 0 || !fofrm.GetStr( no_app ? NULL : dir_str, Str::FormatBuf( "frm", frm ), "", frm_name ) ) )
            {
                no_info = true;
                break;
            }

            AnyFrames* anim = LoadAnimation( frm_fname, path_type );
            if( !anim )
            {
                load_fail = true;
                break;
            }

            frames += anim->CntFrm;
            anims.push_back( anim );

            anims_offs.push_back( fofrm.GetInt( no_app ? NULL : dir_str, Str::FormatBuf( "next_x_%d", frm ), 0 ) );
            anims_offs.push_back( fofrm.GetInt( no_app ? NULL : dir_str, Str::FormatBuf( "next_y_%d", frm ), 0 ) );
        }

        // No frames found or error
        if( no_info || load_fail || anims.empty() || ( dir > 0 && (uint) anims.size() != base_anim->GetCnt() ) )
        {
            if( no_info && dir == 1 )
                break;

            WriteLogF( _FUNC_, " - FOFRM file<%s> invalid data.\n", fname );
            for( uint i = 0, j = (uint) anims.size(); i < j; i++ )
                AnyFrames::Destroy( anims[ i ] );
            AnyFrames::Destroy( base_anim );
            return NULL;
        }

        // Allocate animation storage
        if( !base_anim )
            base_anim = AnyFrames::Create( frames, 1000 / frm_fps * frm_num );
        if( dir == 1 )
            base_anim->CreateDirAnims();
        AnyFrames* anim = base_anim->GetDir( dir );

        // Merge many animations in one
        uint frm = 0;
        for( uint i = 0, j = (uint) anims.size(); i < j; i++ )
        {
            AnyFrames* part = anims[ i ];

            for( uint j = 0; j < part->CntFrm; j++, frm++ )
            {
                anim->Ind[ frm ] = part->Ind[ j ];
                anim->NextX[ frm ] += anims_offs[ frm * 2 + 0 ];
                anim->NextY[ frm ] += anims_offs[ frm * 2 + 1 ];

                SpriteInfo* si = GetSpriteInfo( anim->Ind[ frm ] );
                si->OffsX += ox;
                si->OffsY += oy;
                si->DrawEffect = effect;
            }

            AnyFrames::Destroy( part );
        }
    }

    return base_anim;
}

AnyFrames* SpriteManager::LoadAnimation3d( const char* fname, int path_type )
{
    if( GameOpt.Disable3dRendering )
        return NULL;

    // Load file
    FileManager fm;
    AnyFrames*  fast_anim;
    if( TryLoadAnimationInFastFormat( fname, path_type, fm, fast_anim ) )
        return fast_anim;
    if( !fm.IsLoaded() )
        return NULL;

    // Load 3d animation
    Animation3d* anim3d = Animation3d::GetAnimation( fname, path_type, false );
    if( !anim3d )
        return NULL;
    anim3d->StartMeshGeneration();

    // Get animation data
    float period;
    int   proc_from, proc_to;
    int   dir;
    anim3d->GetRenderFramesData( period, proc_from, proc_to, dir );

    // Set fir
    if( dir < 0 || dir >= DIRS_COUNT )
        anim3d->SetDirAngle( dir );
    else
        anim3d->SetDir( dir );

    // If no animations available than render just one
    if( period == 0.0f || proc_from == proc_to )
    {
label_LoadOneSpr:
        anim3d->SetAnimation( 0, proc_from * 10, NULL, ANIMATION_ONE_TIME | ANIMATION_STAY );
        Render3d( anim3d );

        AnyFrames* anim = AnyFrames::Create( 1, 100 );
        anim->Ind[ 0 ] = anim3d->SprId;

        SAFEDEL( anim3d );
        return anim;
    }

    // Calculate need information
    float frame_time = 1.0f / (float) ( GameOpt.Animation3dFPS ? GameOpt.Animation3dFPS : 10 ); // 1 second / fps
    float period_from = period * (float) proc_from / 100.0f;
    float period_to = period * (float) proc_to / 100.0f;
    float period_len = fabs( period_to - period_from );
    float proc_step = (float) ( proc_to - proc_from ) / ( period_len / frame_time );
    int   frames_count = (int) ceil( period_len / frame_time );

    if( frames_count <= 1 )
        goto label_LoadOneSpr;

    AnyFrames* anim = AnyFrames::Create( frames_count, (uint) ( period_len * 1000.0f ) );

    float      cur_proc = (float) proc_from;
    int        prev_cur_proci = -1;
    for( int i = 0; i < frames_count; i++ )
    {
        int cur_proci = ( proc_to > proc_from ? (int) ( 10.0f * cur_proc + 0.5 ) : (int) ( 10.0f * cur_proc ) );

        // Previous frame is different
        if( cur_proci != prev_cur_proci )
        {
            anim3d->SetAnimation( 0, cur_proci, NULL, ANIMATION_ONE_TIME | ANIMATION_STAY );
            Render3d( anim3d );

            anim->Ind[ i ] = anim3d->SprId;
        }
        // Previous frame is same
        else
        {
            anim->Ind[ i ] = anim->Ind[ i - 1 ];
        }

        cur_proc += proc_step;
        prev_cur_proci = cur_proci;
    }

    SAFEDEL( anim3d );
    return anim;
}

AnyFrames* SpriteManager::LoadAnimationArt( const char* fname, int path_type )
{
    AnyFrames* base_anim = NULL;

    for( int dir = 0; dir < DIRS_COUNT; dir++ )
    {
        int dir_art = dir;
        if( GameOpt.MapHexagonal )
        {
            switch( dir_art )
            {
            case 0:
                dir_art = 1;
                break;
            case 1:
                dir_art = 2;
                break;
            case 2:
                dir_art = 3;
                break;
            case 3:
                dir_art = 5;
                break;
            case 4:
                dir_art = 6;
                break;
            case 5:
                dir_art = 7;
                break;
            default:
                dir_art = 0;
                break;
            }
        }
        else
        {
            dir_art = ( dir + 1 ) % 8;
        }

        char file_name[ MAX_FOPATH ];
        int  palette_index = 0; // 0..3
        bool transparent = false;
        bool mirror_hor = false;
        bool mirror_ver = false;
        uint frm_from = 0;
        uint frm_to = 100000;
        Str::Copy( file_name, fname );

        char* delim = Str::Substring( file_name, "$" );
        if( delim )
        {
            const char* ext = FileManager::GetExtension( file_name ) - 1;
            uint        len = (uint) ( (size_t) ext - (size_t) delim );

            for( uint i = 1; i < len; i++ )
            {
                switch( delim[ i ] )
                {
                case '0':
                    palette_index = 0;
                    break;
                case '1':
                    palette_index = 1;
                    break;
                case '2':
                    palette_index = 2;
                    break;
                case '3':
                    palette_index = 3;
                    break;
                case 'T':
                case 't':
                    transparent = true;
                    break;
                case 'H':
                case 'h':
                    mirror_hor = true;
                    break;
                case 'V':
                case 'v':
                    mirror_ver = true;
                    break;
                case 'F':
                case 'f':
                    // name$1vf5-7.art
                    uint a, b;
                    if( sscanf( &delim[ i + 1 ], "%u-%u", &a, &b ) == 2 )
                        frm_from = a, frm_to = b, i += 3;
                    else if( sscanf( &delim[ i + 1 ], "%u", &a ) == 1 )
                        frm_from = a, frm_to = a, i += 1;
                    break;
                default:
                    break;
                }
            }

            Str::EraseInterval( delim, len );
        }
        if( !file_name[ 0 ] )
            return NULL;

        FileManager fm;
        if( !fm.LoadFile( file_name, path_type ) )
            return NULL;

        struct ArtHeader
        {
            unsigned int flags;
            //      0x00000001 = Static - no rotation, contains frames only for south direction.
            //      0x00000002 = Critter - uses delta attribute, while playing walking animation.
            //      0x00000004 = Font - X offset is equal to number of pixels to advance horizontally for next character.
            //      0x00000008 = Facade - requires fackwalk file.
            //      0x00000010 = Unknown - used in eye candy, for example, DIVINATION.art.
            unsigned int frameRate;
            unsigned int rotationCount;
            unsigned int paletteList[ 4 ];
            unsigned int actionFrame;
            unsigned int frameCount;
            unsigned int infoList[ 8 ];
            unsigned int sizeList[ 8 ];
            unsigned int dataList[ 8 ];
        } header;
        struct ArtFrameInfo
        {
            unsigned int frameWidth;
            unsigned int frameHeight;
            unsigned int frameSize;
            signed int   offsetX;
            signed int   offsetY;
            signed int   deltaX;
            signed int   deltaY;
        } frame_info;
        typedef unsigned int ArtPalette[ 256 ];
        ArtPalette palette[ 4 ];

        if( !fm.CopyMem( &header, sizeof( header ) ) )
            return NULL;
        if( header.flags & 0x00000001 )
            header.rotationCount = 1;

        // Fix dir
        if( header.rotationCount == 1 )
            dir_art = 0;

        // Load palettes
        int palette_count = 0;
        for( int i = 0; i < 4; i++ )
        {
            if( header.paletteList[ i ] )
            {
                if( !fm.CopyMem( &palette[ i ], sizeof( ArtPalette ) ) )
                    return NULL;
                palette_count++;
            }
        }
        if( palette_index >= palette_count )
            palette_index = 0;

        uint frm_fps = header.frameRate;
        if( !frm_fps )
            frm_fps = 10;
        uint frm_count = header.frameCount;
        uint dir_count = header.rotationCount;

        if( frm_from >= frm_count )
            frm_from = frm_count - 1;
        if( frm_to >= frm_count )
            frm_to = frm_count - 1;
        uint frm_count_anim = max( frm_from, frm_to ) - min( frm_from, frm_to ) + 1;

        // Create animation
        if( !base_anim )
            base_anim = AnyFrames::Create( frm_count_anim, 1000 / frm_fps * frm_count_anim );
        if( dir == 1 )
            base_anim->CreateDirAnims();
        AnyFrames* anim = base_anim->GetDir( dir );

        // Calculate data offset
        uint data_offset = sizeof( ArtHeader ) + sizeof( ArtPalette ) * palette_count + sizeof( ArtFrameInfo ) * dir_count * frm_count;
        for( uint i = 0; i < (uint) dir_art; i++ )
        {
            for( uint j = 0; j < frm_count; j++ )
            {
                fm.SetCurPos( sizeof( ArtHeader ) + sizeof( ArtPalette ) * palette_count +
                              sizeof( ArtFrameInfo ) * i * frm_count + sizeof( ArtFrameInfo ) * j + sizeof( unsigned int ) * 2 );
                data_offset += fm.GetLEUInt();
            }
        }

        // Read data
        uint frm = frm_from;
        uint frm_cur = 0;
        while( true )
        {
            uint data_offset_cur = data_offset;
            for( uint i = 0; i < frm; i++ )
            {
                fm.SetCurPos( sizeof( ArtHeader ) + sizeof( ArtPalette ) * palette_count +
                              sizeof( ArtFrameInfo ) * dir_art * frm_count + sizeof( ArtFrameInfo ) * i + sizeof( uint ) * 2 );
                uint frame_size = fm.GetLEUInt();
                data_offset_cur += frame_size;
            }

            fm.SetCurPos( sizeof( ArtHeader ) + sizeof( ArtPalette ) * palette_count +
                          sizeof( ArtFrameInfo ) * dir_art * frm_count + sizeof( ArtFrameInfo ) * frm );

            SpriteInfo* si = new SpriteInfo();
            if( !si )
            {
                AnyFrames::Destroy( base_anim );
                return NULL;
            }

            if( !fm.CopyMem( &frame_info, sizeof( frame_info ) ) )
            {
                delete si;
                AnyFrames::Destroy( base_anim );
                return NULL;
            }

            si->OffsX = ( -frame_info.offsetX + frame_info.frameWidth / 2 ) * ( mirror_hor ? -1 : 1 );
            si->OffsY = ( -frame_info.offsetY + frame_info.frameHeight ) * ( mirror_ver ? -1 : 1 );
            anim->NextX[ frm_cur ] = 0; // frame_info.deltaX;
            anim->NextY[ frm_cur ] = 0; // frame_info.deltaY;

            // Allocate data
            uint   w = frame_info.frameWidth;
            uint   h = frame_info.frameHeight;
            uchar* data = new uchar[ w * h * 4 ];
            uint*  ptr = (uint*) data;

            fm.SetCurPos( data_offset_cur );

            // Decode
// =======================================================================
            #define ART_GET_COLOR                                                                              \
                uchar index = fm.GetUChar();                                                                   \
                uint color = palette[ palette_index ][ index ];                                                \
                std::swap( ( (uchar*) &color )[ 0 ], ( (uchar*) &color )[ 2 ] );                               \
                if( !index )                                                                                   \
                    color = 0;                                                                                 \
                else if( transparent )                                                                         \
                    color |= max( ( color >> 16 ) & 0xFF, max( ( color >> 8 ) & 0xFF, color & 0xFF ) ) << 24;  \
                else                                                                                           \
                    color |= 0xFF000000
            #define ART_WRITE_COLOR                                                                             \
                if( mirror )                                                                                    \
                {                                                                                               \
                    *( ptr + ( ( mirror_ver ? h - y - 1 : y ) * w + ( mirror_hor ? w - x - 1 : x ) ) ) = color; \
                    x++;                                                                                        \
                    if( x >= (int) w )                                                                          \
                        x = 0, y++;                                                                             \
                }                                                                                               \
                else                                                                                            \
                {                                                                                               \
                    *( ptr + pos ) = color;                                                                     \
                    pos++;                                                                                      \
                }
// =======================================================================

            uint pos = 0;
            int  x = 0, y = 0;
            bool mirror = ( mirror_hor || mirror_ver );

            if( w * h == frame_info.frameSize )
            {
                for( uint i = 0; i < frame_info.frameSize; i++ )
                {
                    ART_GET_COLOR;
                    ART_WRITE_COLOR;
                }
            }
            else
            {
                for( uint i = 0; i < frame_info.frameSize; i++ )
                {
                    uchar cmd = fm.GetUChar();
                    if( cmd > 128 )
                    {
                        cmd -= 128;
                        i += cmd;
                        for( ; cmd > 0; cmd-- )
                        {
                            ART_GET_COLOR;
                            ART_WRITE_COLOR;
                        }
                    }
                    else
                    {
                        ART_GET_COLOR;
                        for( ; cmd > 0; cmd-- )
                            ART_WRITE_COLOR;
                        i++;
                    }
                }
            }

            // Fill
            uint result = RequestFillAtlas( si, w, h, data );
            if( !result )
            {
                AnyFrames::Destroy( base_anim );
                return NULL;
            }
            anim->Ind[ frm_cur ] = result;

            // Next index
            if( frm == frm_to )
                break;
            if( frm_to > frm_from )
                frm++;
            else
                frm--;
            frm_cur++;
        }

        if( header.rotationCount != 8 )
            break;
    }

    return base_anim;
}

#define SPR_CACHED_COUNT    ( 10 )
AnyFrames* SpriteManager::LoadAnimationSpr( const char* fname, int path_type )
{
    AnyFrames* base_anim = NULL;

    for( int dir = 0; dir < DIRS_COUNT; dir++ )
    {
        int dir_spr = dir;
        if( GameOpt.MapHexagonal )
        {
            switch( dir_spr )
            {
            case 0:
                dir_spr = 2;
                break;
            case 1:
                dir_spr = 3;
                break;
            case 2:
                dir_spr = 4;
                break;
            case 3:
                dir_spr = 6;
                break;
            case 4:
                dir_spr = 7;
                break;
            case 5:
                dir_spr = 0;
                break;
            default:
                dir_spr = 0;
                break;
            }
        }
        else
        {
            dir_spr = ( dir + 2 ) % 8;
        }

        // Parameters
        char file_name[ MAX_FOPATH ];
        Str::Copy( file_name, fname );

        // Animation
        char seq_name[ MAX_FOPATH ] = { 0 };

        // Color offsets
        // 0 - other
        // 1 - skin
        // 2 - hair
        // 3 - armor
        int   rgb_offs[ 4 ][ 3 ] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

        char* delim = Str::Substring( file_name, "$" );
        if( delim )
        {
            // Format: fileName$[1,100,0,0][2,0,0,100]animName.spr
            const char* ext = FileManager::GetExtension( file_name ) - 1;
            uint        len = (uint) ( (size_t) ext - (size_t) delim );
            if( len > 1 )
            {
                memcpy( seq_name, delim + 1, len - 1 );
                seq_name[ len - 1 ] = 0;

                // Parse rgb offsets
                char* rgb_beg = Str::Substring( seq_name, "[" );
                while( rgb_beg )
                {
                    // Find end of offsets data
                    char* rgb_end = Str::Substring( rgb_beg + 1, "]" );
                    if( !rgb_end )
                        break;

                    // Parse numbers
                    int rgb[ 4 ];
                    if( sscanf( rgb_beg + 1, "%d,%d,%d,%d", &rgb[ 0 ], &rgb[ 1 ], &rgb[ 2 ], &rgb[ 3 ] ) != 4 )
                        break;

                    // To one part
                    if( rgb[ 0 ] >= 0 && rgb[ 0 ] <= 3 )
                    {
                        rgb_offs[ rgb[ 0 ] ][ 0 ] = rgb[ 1 ];                                   // R
                        rgb_offs[ rgb[ 0 ] ][ 1 ] = rgb[ 2 ];                                   // G
                        rgb_offs[ rgb[ 0 ] ][ 2 ] = rgb[ 3 ];                                   // B
                    }
                    // To all parts
                    else
                    {
                        for( int i = 0; i < 4; i++ )
                        {
                            rgb_offs[ i ][ 0 ] = rgb[ 1 ];                                             // R
                            rgb_offs[ i ][ 1 ] = rgb[ 2 ];                                             // G
                            rgb_offs[ i ][ 2 ] = rgb[ 3 ];                                             // B
                        }
                    }

                    // Erase from name and find again
                    Str::EraseInterval( rgb_beg, (uint) ( rgb_end - rgb_beg + 1 ) );
                    rgb_beg = Str::Substring( seq_name, "[" );
                }
            }
            Str::EraseInterval( delim, len );
        }
        if( !file_name[ 0 ] )
            return NULL;

        // Cache last 10 big SPR files (for critters)
        struct SprCache
        {
            char        fileName[ MAX_FOPATH ];
            int         pathType;
            FileManager fm;
        } static* cached[ SPR_CACHED_COUNT + 1 ] = { 0 };         // Last index for last loaded

        // Find already opened
        int index = -1;
        for( int i = 0; i <= SPR_CACHED_COUNT; i++ )
        {
            if( !cached[ i ] )
                break;
            if( Str::CompareCase( file_name, cached[ i ]->fileName ) && path_type == cached[ i ]->pathType )
            {
                index = i;
                break;
            }
        }

        // Open new
        if( index == -1 )
        {
            FileManager fm;
            if( !fm.LoadFile( file_name, path_type ) )
                return NULL;

            if( fm.GetFsize() > 1000000 )                 // 1mb
            {
                // Find place in cache
                for( int i = 0; i < SPR_CACHED_COUNT; i++ )
                {
                    if( !cached[ i ] )
                    {
                        index = i;
                        break;
                    }
                }

                // Delete old element
                if( index == -1 )
                {
                    delete cached[ 0 ];
                    index = SPR_CACHED_COUNT - 1;
                    for( int i = 0; i < SPR_CACHED_COUNT - 1; i++ )
                        cached[ i ] = cached[ i + 1 ];
                }

                cached[ index ] = new SprCache();
                if( !cached[ index ] )
                    return NULL;
                Str::Copy( cached[ index ]->fileName, file_name );
                cached[ index ]->pathType = path_type;
                cached[ index ]->fm = fm;
                fm.ReleaseBuffer();
            }
            else
            {
                index = SPR_CACHED_COUNT;
                if( !cached[ index ] )
                    cached[ index ] = new SprCache();
                else
                    cached[ index ]->fm.UnloadFile();
                Str::Copy( cached[ index ]->fileName, file_name );
                cached[ index ]->pathType = path_type;
                cached[ index ]->fm = fm;
                fm.ReleaseBuffer();
            }
        }

        FileManager& fm = cached[ index ]->fm;
        fm.SetCurPos( 0 );

        // Read header
        char head[ 11 ];
        if( !fm.CopyMem( head, 11 ) || head[ 8 ] || strcmp( head, "<sprite>" ) )
            return NULL;

        float dimension_left = (float) fm.GetUChar() * 6.7f;
        float dimension_up = (float) fm.GetUChar() * 7.6f;
        UNUSED_VARIABLE( dimension_up );
        float dimension_right = (float) fm.GetUChar() * 6.7f;
        int   center_x = fm.GetLEUInt();
        int   center_y = fm.GetLEUInt();
        fm.GoForward( 2 );                         // ushort unknown1  sometimes it is 0, and sometimes it is 3
        fm.GoForward( 1 );                         // CHAR unknown2  0x64, other values were not observed

        float   ta = RAD( 127.0f / 2.0f );         // Tactics grid angle
        int     center_x_ex = (int) ( ( dimension_left * sinf( ta ) + dimension_right * sinf( ta ) ) / 2.0f - dimension_left * sinf( ta ) );
        int     center_y_ex = (int) ( ( dimension_left * cosf( ta ) + dimension_right * cosf( ta ) ) / 2.0f );

        uint    anim_index = 0;
        UIntVec frames;
        frames.reserve( 1000 );

        // Find sequence
        bool seq_founded = false;
        char name[ MAX_FOTEXT ];
        uint seq_cnt = fm.GetLEUInt();
        for( uint seq = 0; seq < seq_cnt; seq++ )
        {
            // Find by name
            uint item_cnt = fm.GetLEUInt();
            fm.GoForward( sizeof( short ) * item_cnt );
            fm.GoForward( sizeof( uint ) * item_cnt );               // uint  unknown3[item_cnt]

            uint len = fm.GetLEUInt();
            fm.CopyMem( name, len );
            name[ len ] = 0;

            ushort index = fm.GetLEUShort();

            if( !seq_name[ 0 ] || Str::CompareCase( name, seq_name ) )
            {
                anim_index = index;

                // Read frame numbers
                fm.GoBack( sizeof( ushort ) + len + sizeof( uint ) +
                           sizeof( uint ) * item_cnt + sizeof( short ) * item_cnt );

                for( uint i = 0; i < item_cnt; i++ )
                {
                    short val = fm.GetLEUShort();
                    if( val >= 0 )
                        frames.push_back( val );
                    else if( val == -43 )
                    {
                        fm.GoForward( sizeof( short ) * 3 );
                        i += 3;
                    }
                }

                // Animation founded, go forward
                seq_founded = true;
                break;
            }
        }
        if( !seq_founded )
            return NULL;

        // Find animation
        fm.SetCurPos( 0 );
        for( uint i = 0; i <= anim_index; i++ )
        {
            if( !fm.FindFragment( (uchar*) "<spranim>", 9, fm.GetCurPos() ) )
                return NULL;
            fm.GoForward( 12 );
        }

        uint    file_offset = fm.GetLEUInt();
        fm.GoForward( fm.GetLEUInt() );           // Collection name
        uint    frame_cnt = fm.GetLEUInt();
        uint    dir_cnt = fm.GetLEUInt();
        UIntVec bboxes;
        bboxes.resize( frame_cnt * dir_cnt * 4 );
        fm.CopyMem( &bboxes[ 0 ], sizeof( uint ) * frame_cnt * dir_cnt * 4 );

        // Fix dir
        if( dir_cnt != 8 )
            dir_spr = dir_spr * ( dir_cnt * 100 / 8 ) / 100;
        if( (uint) dir_spr >= dir_cnt )
            dir_spr = 0;

        // Check wrong frames
        for( uint i = 0; i < frames.size();)
        {
            if( frames[ i ] >= frame_cnt )
                frames.erase( frames.begin() + i );
            else
                i++;
        }

        // Get images file
        fm.SetCurPos( file_offset );
        fm.GoForward( 14 );          // <spranim_img>\0
        uchar type = fm.GetUChar();
        fm.GoForward( 1 );           // \0

        uint data_len;
        uint cur_pos = fm.GetCurPos();
        if( fm.FindFragment( (uchar*) "<spranim_img>", 13, cur_pos ) )
            data_len = fm.GetCurPos() - cur_pos;
        else
            data_len = fm.GetFsize() - cur_pos;
        fm.SetCurPos( cur_pos );

        bool   packed = ( type == 0x32 );
        uchar* data = fm.GetCurBuf();
        if( packed )
        {
            // Unpack with zlib
            uint unpacked_len = fm.GetLEUInt();
            data = Crypt.Uncompress( fm.GetCurBuf(), data_len, unpacked_len / data_len + 1 );
            if( !data )
                return NULL;
            data_len = unpacked_len;
        }
        FileManager fm_images;
        fm_images.LoadStream( data, data_len );
        if( packed )
            delete[] data;

        // Read palette
        typedef uint Palette[ 256 ];
        Palette palette[ 4 ];
        for( int i = 0; i < 4; i++ )
        {
            uint palette_count = fm_images.GetLEUInt();
            if( palette_count <= 256 )
                fm_images.CopyMem( &palette[ i ], palette_count * 4 );
        }

        // Index data offsets
        UIntVec image_indices;
        image_indices.resize( frame_cnt * dir_cnt * 4 );
        for( uint cur = 0; !fm_images.IsEOF();)
        {
            uchar tag = fm_images.GetUChar();
            if( tag == 1 )
            {
                // Valid index
                image_indices[ cur ] = fm_images.GetCurPos();
                fm_images.GoForward( 8 + 8 + 8 + 1 );
                fm_images.GoForward( fm_images.GetLEUInt() );
                cur++;
            }
            else if( tag == 0 )
            {
                // Empty index
                cur++;
            }
            else
                break;
        }

        // Create animation
        if( frames.empty() )
            frames.push_back( 0 );
        if( !base_anim )
            base_anim = AnyFrames::Create( (uint) frames.size(), 1000 / 10 * (uint) frames.size() );
        if( dir == 1 )
            base_anim->CreateDirAnims();
        AnyFrames* anim = base_anim->GetDir( dir );

        for( uint f = 0, ff = (uint) frames.size(); f < ff; f++ )
        {
            uint frm = frames[ f ];
            anim->NextX[ f ] = 0;
            anim->NextY[ f ] = 0;

            // Optimization, share frames
            bool founded = false;
            for( uint i = 0, j = f; i < j; i++ )
            {
                if( frames[ i ] == frm )
                {
                    anim->Ind[ f ] = anim->Ind[ i ];
                    founded = true;
                    break;
                }
            }
            if( founded )
                continue;

            // Compute whole image size
            uint whole_w = 0, whole_h = 0;
            for( uint part = 0; part < 4; part++ )
            {
                uint frm_index = ( type == 0x32 ? frame_cnt * dir_cnt * part + dir_spr * frame_cnt + frm : ( ( frm * dir_cnt + dir_spr ) << 2 ) + part );
                if( !image_indices[ frm_index ] )
                    continue;
                fm_images.SetCurPos( image_indices[ frm_index ] );

                uint posx = fm_images.GetLEUInt();
                uint posy = fm_images.GetLEUInt();
                fm_images.GoForward( 8 );
                uint w = fm_images.GetLEUInt();
                uint h = fm_images.GetLEUInt();
                if( w + posx > whole_w )
                    whole_w = w + posx;
                if( h + posy > whole_h )
                    whole_h = h + posy;
            }
            if( !whole_w )
                whole_w = 1;
            if( !whole_h )
                whole_h = 1;

            // Allocate data
            uchar* img_data = new uchar[ whole_w * whole_h * 4 ];
            memzero( img_data, whole_w * whole_h * 4 );

            for( uint part = 0; part < 4; part++ )
            {
                uint frm_index = ( type == 0x32 ? frame_cnt * dir_cnt * part + dir_spr * frame_cnt + frm : ( ( frm * dir_cnt + dir_spr ) << 2 ) + part );
                if( !image_indices[ frm_index ] )
                    continue;
                fm_images.SetCurPos( image_indices[ frm_index ] );

                uint   posx = fm_images.GetLEUInt();
                uint   posy = fm_images.GetLEUInt();

                char   zar[ 8 ] = { 0 };
                fm_images.CopyMem( zar, 8 );
                uchar  subtype = zar[ 6 ];

                uint   w = fm_images.GetLEUInt();
                uint   h = fm_images.GetLEUInt();
                UNUSED_VARIABLE( h );
                uchar  palette_present = fm_images.GetUChar();
                uint   rle_size = fm_images.GetLEUInt();
                uchar* rle_buf = fm_images.GetCurBuf();
                fm_images.GoForward( rle_size );
                uchar  def_color = 0;

                if( zar[ 5 ] || strcmp( zar, "<zar>" ) || ( subtype != 0x34 && subtype != 0x35 ) || palette_present == 1 )
                {
                    AnyFrames::Destroy( anim );
                    return NULL;
                }

                uint* ptr = (uint*) img_data + ( posy * whole_w ) + posx;
                uint  x = posx, y = posy;
                while( rle_size )
                {
                    int control = *rle_buf;
                    rle_buf++;
                    rle_size--;

                    int control_mode = ( control & 3 );
                    int control_count = ( control >> 2 );

                    for( int i = 0; i < control_count; i++ )
                    {
                        uint col = 0;
                        switch( control_mode )
                        {
                        case 1:
                            col = palette[ part ][ rle_buf[ i ] ];
                            ( (uchar*) &col )[ 3 ] = 0xFF;
                            break;
                        case 2:
                            col = palette[ part ][ rle_buf[ 2 * i ] ];
                            ( (uchar*) &col )[ 3 ] = rle_buf[ 2 * i + 1 ];
                            break;
                        case 3:
                            col = palette[ part ][ def_color ];
                            ( (uchar*) &col )[ 3 ] = rle_buf[ i ];
                            break;
                        default:
                            break;
                        }

                        for( int j = 0; j < 3; j++ )
                        {
                            if( rgb_offs[ part ][ j ] )
                            {
                                int val = (int) ( ( (uchar*) &col )[ 2 - j ] ) + rgb_offs[ part ][ j ];
                                ( (uchar*) &col )[ 2 - j ] = CLAMP( val, 0, 255 );
                            }
                        }

                        std::swap( ( (uchar*) &col )[ 0 ], ( (uchar*) &col )[ 2 ] );

                        if( !part )
                            *ptr = col;
                        else if( ( col >> 24 ) >= 128 )
                            *ptr = col;
                        ptr++;

                        if( ++x >= w + posx )
                        {
                            x = posx;
                            y++;
                            ptr = (uint*) img_data + ( y * whole_w ) + x;
                        }
                    }

                    if( control_mode )
                    {
                        rle_size -= control_count;
                        rle_buf += control_count;
                        if( control_mode == 2 )
                        {
                            rle_size -= control_count;
                            rle_buf += control_count;
                        }
                    }
                }
            }

            SpriteInfo* si = new SpriteInfo();
            si->OffsX = bboxes[ frm * dir_cnt * 4 + dir_spr * 4 + 0 ] - center_x + center_x_ex + whole_w / 2;
            si->OffsY = bboxes[ frm * dir_cnt * 4 + dir_spr * 4 + 1 ] - center_y + center_y_ex + whole_h;
            uint result = RequestFillAtlas( si, whole_w, whole_h, img_data );
            if( !result )
            {
                AnyFrames::Destroy( base_anim );
                return NULL;
            }
            anim->Ind[ f ] = result;
        }

        if( dir_cnt == 1 )
            break;
    }

    return base_anim;
}

AnyFrames* SpriteManager::LoadAnimationZar( const char* fname, int path_type )
{
    // Open file
    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return NULL;

    // Read header
    char head[ 6 ];
    if( !fm.CopyMem( head, 6 ) || head[ 5 ] || strcmp( head, "<zar>" ) )
        return NULL;
    uchar type = fm.GetUChar();
    fm.GoForward( 1 );   // \0
    uint  w = fm.GetLEUInt();
    uint  h = fm.GetLEUInt();
    uchar palette_present = fm.GetUChar();

    // Read palette
    uint  palette[ 256 ] = { 0 };
    uchar def_color = 0;
    if( palette_present )
    {
        uint palette_count = fm.GetLEUInt();
        if( palette_count > 256 )
            return NULL;
        fm.CopyMem( palette, sizeof( uint ) * palette_count );
        if( type == 0x34 )
            def_color = fm.GetUChar();
    }

    // Read image
    uint   rle_size = fm.GetLEUInt();
    uchar* rle_buf = fm.GetCurBuf();
    fm.GoForward( rle_size );

    // Allocate data
    uchar* img_data = new uchar[ w * h * 4 ];
    uint*  ptr = (uint*) img_data;

    // Decode
    while( rle_size )
    {
        int control = *rle_buf;
        rle_buf++;
        rle_size--;

        int control_mode = ( control & 3 );
        int control_count = ( control >> 2 );

        for( int i = 0; i < control_count; i++ )
        {
            uint col = 0;
            switch( control_mode )
            {
            case 1:
                col = palette[ rle_buf[ i ] ];
                ( (uchar*) &col )[ 3 ] = 0xFF;
                break;
            case 2:
                col = palette[ rle_buf[ 2 * i ] ];
                ( (uchar*) &col )[ 3 ] = rle_buf[ 2 * i + 1 ];
                break;
            case 3:
                col = palette[ def_color ];
                ( (uchar*) &col )[ 3 ] = rle_buf[ i ];
                break;
            default:
                break;
            }

            *ptr++ = col;
        }

        if( control_mode )
        {
            rle_size -= control_count;
            rle_buf += control_count;
            if( control_mode == 2 )
            {
                rle_size -= control_count;
                rle_buf += control_count;
            }
        }
    }

    // Fill animation
    SpriteInfo* si = new SpriteInfo();
    uint        result = RequestFillAtlas( si, w, h, img_data );
    if( !result )
        return NULL;

    AnyFrames* anim = AnyFrames::Create( 1, 100 );
    anim->Ind[ 0 ] = result;
    return anim;
}

AnyFrames* SpriteManager::LoadAnimationTil( const char* fname, int path_type )
{
    // Open file
    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return NULL;

    // Read header
    char head[ 7 ];
    if( !fm.CopyMem( head, 7 ) || head[ 6 ] || strcmp( head, "<tile>" ) )
        return NULL;
    while( fm.GetUChar() )
        ;
    fm.GoForward( 7 + 4 ); // Unknown
    uint w = fm.GetLEUInt();
    UNUSED_VARIABLE( w );
    uint h = fm.GetLEUInt();
    UNUSED_VARIABLE( h );

    if( !fm.FindFragment( (uchar*) "<tiledata>", 10, fm.GetCurPos() ) )
        return NULL;
    fm.GoForward( 10 + 3 ); // Signature
    uint frames_count = fm.GetLEUInt();

    // Create animation
    AnyFrames* anim = AnyFrames::Create( frames_count, 1000 / 10 * frames_count );

    for( uint frm = 0; frm < frames_count; frm++ )
    {
        // Read header
        char head[ 6 ];
        if( !fm.CopyMem( head, 6 ) || head[ 5 ] || strcmp( head, "<zar>" ) )
            return NULL;
        uchar type = fm.GetUChar();
        fm.GoForward( 1 );       // \0
        uint  w = fm.GetLEUInt();
        uint  h = fm.GetLEUInt();
        uchar palette_present = fm.GetUChar();

        // Read palette
        uint  palette[ 256 ] = { 0 };
        uchar def_color = 0;
        if( palette_present )
        {
            uint palette_count = fm.GetLEUInt();
            if( palette_count > 256 )
                return NULL;
            fm.CopyMem( palette, sizeof( uint ) * palette_count );
            if( type == 0x34 )
                def_color = fm.GetUChar();
        }

        // Read image
        uint   rle_size = fm.GetLEUInt();
        uchar* rle_buf = fm.GetCurBuf();
        fm.GoForward( rle_size );

        // Allocate data
        uchar* img_data = new uchar[ w * h * 4 ];
        uint*  ptr = (uint*) img_data;

        // Decode
        while( rle_size )
        {
            int control = *rle_buf;
            rle_buf++;
            rle_size--;

            int control_mode = ( control & 3 );
            int control_count = ( control >> 2 );

            for( int i = 0; i < control_count; i++ )
            {
                uint col = 0;
                switch( control_mode )
                {
                case 1:
                    col = palette[ rle_buf[ i ] ];
                    ( (uchar*) &col )[ 3 ] = 0xFF;
                    break;
                case 2:
                    col = palette[ rle_buf[ 2 * i ] ];
                    ( (uchar*) &col )[ 3 ] = rle_buf[ 2 * i + 1 ];
                    break;
                case 3:
                    col = palette[ def_color ];
                    ( (uchar*) &col )[ 3 ] = rle_buf[ i ];
                    break;
                default:
                    break;
                }

                *ptr++ = col;
            }

            if( control_mode )
            {
                rle_size -= control_count;
                rle_buf += control_count;
                if( control_mode == 2 )
                {
                    rle_size -= control_count;
                    rle_buf += control_count;
                }
            }
        }

        // Fill animation
        SpriteInfo* si = new SpriteInfo();
        uint        result = RequestFillAtlas( si, w, h, img_data );
        if( !result )
            return NULL;

        anim->Ind[ frm ] = result;
    }

    return anim;
}

AnyFrames* SpriteManager::LoadAnimationMos( const char* fname, int path_type )
{
    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return NULL;

    // Read signature
    char head[ 8 ];
    if( !fm.CopyMem( head, 8 ) || !Str::CompareCount( head, "MOS", 3 ) )
        return NULL;

    // Packed
    if( head[ 3 ] == 'C' )
    {
        uint   unpacked_len = fm.GetLEUInt();
        uint   data_len = fm.GetFsize() - 12;
        uchar* buf = fm.GetCurBuf();
        *(ushort*) buf = 0x9C78;
        uchar* data = Crypt.Uncompress( buf, data_len, unpacked_len / fm.GetFsize() + 1 );
        if( !data )
            return NULL;

        fm.UnloadFile();
        fm.LoadStream( data, data_len );
        delete[] data;

        if( !fm.CopyMem( head, 8 ) || !Str::CompareCount( head, "MOS", 3 ) )
            return NULL;
    }

    // Read header
    uint w = fm.GetLEUShort();            // Width (pixels)
    uint h = fm.GetLEUShort();            // Height (pixels)
    uint col = fm.GetLEUShort();          // Columns (blocks)
    uint row = fm.GetLEUShort();          // Rows (blocks)
    uint block_size = fm.GetLEUInt();     // Block size (pixels)
    UNUSED_VARIABLE( block_size );
    uint palette_offset = fm.GetLEUInt(); // Offset (from start of file) to palettes
    uint tiles_offset = palette_offset + col * row * 256 * 4;
    uint data_offset = tiles_offset + col * row * 4;

    // Allocate data
    uchar* img_data = new uchar[ w * h * 4 ];
    uint*  ptr = (uint*) img_data;

    // Read image data
    uint palette[ 256 ] = { 0 };
    uint block = 0;
    for( uint y = 0; y < row; y++ )
    {
        for( uint x = 0; x < col; x++ )
        {
            // Get palette for current block
            fm.SetCurPos( palette_offset + block * 256 * 4 );
            fm.CopyMem( palette, 256 * 4 );

            // Set initial position
            fm.SetCurPos( tiles_offset + block * 4 );
            fm.SetCurPos( data_offset + fm.GetLEUInt() );

            // Calculate current block size
            uint block_w = ( ( x == col - 1 ) ? w % 64 : 64 );
            uint block_h = ( ( y == row - 1 ) ? h % 64 : 64 );
            if( !block_w )
                block_w = 64;
            if( !block_h )
                block_h = 64;

            // Read data
            uint pos = y * 64 * w + x * 64;
            for( uint yy = 0; yy < block_h; yy++ )
            {
                for( uint xx = 0; xx < block_w; xx++ )
                {
                    uint color = palette[ fm.GetUChar() ];
                    if( color == 0xFF00 )
                        color = 0;                                 // Green is transparent
                    else
                        color |= 0xFF000000;
                    *( ptr + pos ) = color;
                    pos++;
                }
                pos += w - block_w;
            }

            // Go to next block
            block++;
        }
    }

    // Fill animation
    SpriteInfo* si = new SpriteInfo();
    uint        result = RequestFillAtlas( si, w, h, img_data );
    if( !result )
        return NULL;

    AnyFrames* anim = AnyFrames::Create( 1, 100 );
    anim->Ind[ 0 ] = result;
    return anim;
}

AnyFrames* SpriteManager::LoadAnimationBam( const char* fname, int path_type )
{
    // Parameters
    char file_name[ MAX_FOPATH ];
    Str::Copy( file_name, fname );

    uint  need_cycle = 0;
    int   specific_frame = -1;

    char* delim = Str::Substring( file_name, "$" );
    if( delim )
    {
        // Format: fileName$5-6.spr
        const char* ext = FileManager::GetExtension( file_name ) - 1;
        uint        len = (uint) ( (size_t) ext - (size_t) delim );
        if( len > 1 )
        {
            char params[ MAX_FOPATH ];
            memcpy( params, delim + 1, len - 1 );
            params[ len - 1 ] = 0;

            need_cycle = Str::AtoI( params );
            const char* next_param = Str::Substring( params, "-" );
            if( next_param )
            {
                specific_frame = Str::AtoI( next_param + 1 );
                if( specific_frame < 0 )
                    specific_frame = -1;
            }
        }
        Str::EraseInterval( delim, len );
    }
    if( !file_name[ 0 ] )
        return NULL;

    // Load file
    FileManager fm;
    if( !fm.LoadFile( file_name, path_type ) )
        return NULL;

    // Read signature
    char head[ 8 ];
    if( !fm.CopyMem( head, 8 ) || !Str::CompareCount( head, "BAM", 3 ) )
        return NULL;

    // Packed
    if( head[ 3 ] == 'C' )
    {
        uint   unpacked_len = fm.GetLEUInt();
        uint   data_len = fm.GetFsize() - 12;
        uchar* buf = fm.GetCurBuf();
        *(ushort*) buf = 0x9C78;
        uchar* data = Crypt.Uncompress( buf, data_len, unpacked_len / fm.GetFsize() + 1 );
        if( !data )
            return NULL;

        fm.UnloadFile();
        fm.LoadStream( data, data_len );
        delete[] data;

        if( !fm.CopyMem( head, 8 ) || !Str::CompareCount( head, "BAM", 3 ) )
            return NULL;
    }

    // Read header
    uint  frames_count = fm.GetLEUShort();
    uint  cycles_count = fm.GetUChar();
    uchar compr_color = fm.GetUChar();
    uint  frames_offset = fm.GetLEUInt();
    uint  palette_offset = fm.GetLEUInt();
    uint  lookup_table_offset = fm.GetLEUInt();

    // Count whole frames
    if( need_cycle >= cycles_count )
        need_cycle = 0;
    fm.SetCurPos( frames_offset + frames_count * 12 + need_cycle * 4 );
    uint cycle_frames = fm.GetLEUShort();
    uint table_index = fm.GetLEUShort();
    if( specific_frame != -1 && specific_frame >= (int) cycle_frames )
        specific_frame = 0;

    // Create animation
    AnyFrames* anim = AnyFrames::Create( specific_frame == -1 ? cycle_frames : 1, 0 );

    // Palette
    uint palette[ 256 ] = { 0 };
    fm.SetCurPos( palette_offset );
    fm.CopyMem( palette, 256 * 4 );

    // Find in lookup table
    for( uint i = 0; i < cycle_frames; i++ )
    {
        if( specific_frame != -1 && specific_frame != (int) i )
            continue;

        // Get need index
        fm.SetCurPos( lookup_table_offset + table_index * 2 );
        table_index++;
        uint frame_index = fm.GetLEUShort();

        // Get frame data
        fm.SetCurPos( frames_offset + frame_index * 12 );
        uint w = fm.GetLEUShort();
        uint h = fm.GetLEUShort();
        int  ox = (ushort) fm.GetLEUShort();
        int  oy = (ushort) fm.GetLEUShort();
        uint data_offset = fm.GetLEUInt();
        bool rle = ( data_offset & 0x80000000 ? false : true );
        data_offset &= 0x7FFFFFFF;

        // Allocate data
        uchar* img_data = new uchar[ w * h * 4 ];
        uint*  ptr = (uint*) img_data;

        // Fill it
        fm.SetCurPos( data_offset );
        for( uint k = 0, l = w * h; k < l;)
        {
            uchar index = fm.GetUChar();
            uint  color = palette[ index ];
            if( color == 0xFF00 )
                color = 0;
            else
                color |= 0xFF000000;

            if( rle && index == compr_color )
            {
                uint copies = fm.GetUChar();
                for( uint m = 0; m <= copies; m++, k++ )
                    *ptr++ = color;
            }
            else
            {
                *ptr++ = color;
                k++;
            }
        }

        // Set in animation sequence
        SpriteInfo* si = new SpriteInfo();
        uint        result = RequestFillAtlas( si, w, h, img_data );
        if( !result )
            return NULL;
        si->OffsX = -ox + w / 2;
        si->OffsY = -oy + h;

        if( specific_frame != -1 )
        {
            anim->Ind[ 0 ] = result;
            break;
        }
        anim->Ind[ i ] = result;
    }

    return anim;
}

AnyFrames* SpriteManager::LoadAnimationOther( const char* fname, int path_type, uchar* ( *loader )( const uchar *, uint, uint &, uint & ) )
{
    // Load file
    FileManager fm;
    AnyFrames*  fast_anim;
    if( TryLoadAnimationInFastFormat( fname, path_type, fm, fast_anim ) )
        return fast_anim;

    // Load
    uint   w, h;
    uchar* data = loader( fm.GetBuf(), fm.GetFsize(), w, h );
    if( !data )
        return NULL;

    // Fill data
    SpriteInfo* si = new SpriteInfo();
    uint        result = RequestFillAtlas( si, w, h, data );
    if( !result )
        return NULL;

    // Create animation
    AnyFrames* anim = AnyFrames::Create( 1, 100 );
    anim->Ind[ 0 ] = result;
    return anim;
}

bool SpriteManager::Render3d( Animation3d* anim3d )
{
    // Find place for render
    if( !anim3d->SprId )
        RefreshPure3dAnimationSprite( anim3d );

    // Set render target
    SpriteInfo* si = sprData[ anim3d->SprId ];
    RenderTarget* rt = Get3dRenderTarget( si->Width, si->Height );
    PushRenderTarget( rt );
    ClearCurrentRenderTarget( 0 );
    ClearCurrentRenderTargetDS( true, true );
    Animation3d::SetScreenSize( rt->TargetTexture->Width, rt->TargetTexture->Height );

    // Draw model
    GL( glDisable( GL_BLEND ) );
    anim3d->Draw( 0, 0 );
    GL( glEnable( GL_BLEND ) );

    // Restore render target
    PopRenderTarget();

    // Copy render
    Rect region_to(
        (int) ( si->SprRect.L * (float) si->Atlas->Width + 0.5f ),
        (int) ( ( 1.0f - si->SprRect.T ) * (float) si->Atlas->Height + 0.5f ),
        (int) ( si->SprRect.R * (float) si->Atlas->Width + 0.5f ),
        (int) ( ( 1.0f - si->SprRect.B ) * (float) si->Atlas->Height + 0.5f ) );
    PushRenderTarget( si->Atlas->RT );
    DrawRenderTarget( rt, false, NULL, &region_to );
    PopRenderTarget();

    return true;
}

bool SpriteManager::Draw3d( int x, int y, Animation3d* anim3d, uint color )
{
    if( GameOpt.Disable3dRendering )
        return false;

    anim3d->StartMeshGeneration();
    Render3d( anim3d );

    SpriteInfo* si = sprData[ anim3d->SprId ];
    DrawSprite( anim3d->SprId, x - si->Width / 2 + si->OffsX, y - si->Height + si->OffsY, color );

    return true;
}

Animation3d* SpriteManager::LoadPure3dAnimation( const char* fname, int path_type, bool auto_redraw )
{
    if( GameOpt.Disable3dRendering )
        return NULL;

    // Fill data
    Animation3d* anim3d = Animation3d::GetAnimation( fname, path_type, false );
    if( !anim3d )
        return NULL;

    // Create render sprite
    anim3d->SprId = 0;
    anim3d->SprAtlasType = atlasTypeStack.back();
    if( auto_redraw )
    {
        RefreshPure3dAnimationSprite( anim3d );
        autoRedrawAnim3d.push_back( anim3d );
    }
    return anim3d;
}

void SpriteManager::RefreshPure3dAnimationSprite( Animation3d* anim3d )
{
    // Free old place
    if( anim3d->SprId )
    {
        sprData[ anim3d->SprId ]->Anim3d = NULL;
        anim3d->SprId = 0;
    }

    // Render size
    uint draw_width, draw_height;
    anim3d->GetDrawSize( draw_width, draw_height );

    // Find already created place for rendering
    uint index = 0;
    for( size_t i = 0, j = sprData.size(); i < j; i++ )
    {
        SpriteInfo* si = sprData[ i ];
        if( si && si->UsedForAnim3d && !si->Anim3d && si->Width == draw_width && si->Height == draw_height && si->Atlas->Type == anim3d->SprAtlasType )
        {
            index = (uint) i;
            break;
        }
    }

    // Create new place for rendering
    if( !index )
    {
        PushAtlasType( anim3d->SprAtlasType );
        index = RequestFillAtlas( NULL, draw_width, draw_height, NULL );
        PopAtlasType();
        SpriteInfo* si = sprData[ index ];
        si->OffsY = draw_height / 4;
        si->UsedForAnim3d = true;
    }

    // Cross links
    anim3d->SprId = index;
    sprData[ index ]->Anim3d = anim3d;
}

void SpriteManager::FreePure3dAnimation( Animation3d* anim3d )
{
    if( anim3d )
    {
        auto it = std::find( autoRedrawAnim3d.begin(), autoRedrawAnim3d.end(), anim3d );
        if( it != autoRedrawAnim3d.end() )
            autoRedrawAnim3d.erase( it );

        if( anim3d->SprId )
            sprData[ anim3d->SprId ]->Anim3d = NULL;

        SAFEDEL( anim3d );
    }
}

bool SpriteManager::SaveAnimationInFastFormat( AnyFrames* anim, const char* fname, int path_type )
{
    FileManager fm;
    fm.SetBEUInt( FAST_FORMAT_SIGNATURE );
    fm.SetBEUShort( FAST_FORMAT_VERSION );
    fm.SetBEUShort( anim->CntFrm );
    fm.SetBEUShort( anim->Ticks );
    fm.SetBEUShort( anim->DirCount() );
    for( int dir = 0; dir < anim->DirCount(); dir++ )
    {
        AnyFrames* dir_anim = anim->GetDir( dir );
        for( ushort i = 0; i < dir_anim->CntFrm; i++ )
        {
            SpriteInfo* si = GetSpriteInfo( dir_anim->Ind[ i ] );
            fm.SetBEUShort( si->Width );
            fm.SetBEUShort( si->Height );
            fm.SetBEShort( si->OffsX );
            fm.SetBEShort( si->OffsY );
            fm.SetBEShort( dir_anim->NextX[ i ] );
            fm.SetBEShort( dir_anim->NextY[ i ] );
            fm.SetData( si->Data, si->Width * si->Height * 4 );
        }
    }
    return fm.SaveOutBufToFile( fname, path_type );
}

bool SpriteManager::TryLoadAnimationInFastFormat( const char* fname, int path_type, FileManager& fm, AnyFrames*& anim )
{
    // Null result
    anim = NULL;

    // Load file
    if( !fm.LoadFile( fname, path_type ) )
        return true;

    // Check for fonline cached format
    if( fm.GetFsize() >= 16 && fm.GetBEUInt() == FAST_FORMAT_SIGNATURE )
    {
        ushort version = fm.GetBEUShort();
        if( version == 1 )
        {
            ushort frames_count = fm.GetBEUShort();
            uint   ticks = fm.GetBEUInt();
            int    dirs = fm.GetBEUInt();
            if( dirs != 1 && dirs != DIRS_COUNT )
            {
                WriteLogF( _FUNC_, " - Incorrect dirs count. \n" );
                return true;
            }

            anim = AnyFrames::Create( frames_count, ticks );
            if( dirs > 1 )
                anim->CreateDirAnims();
            for( int dir = 0; dir < dirs; dir++ )
            {
                AnyFrames* dir_anim = anim;
                for( ushort i = 0; i < frames_count; i++ )
                {
                    SpriteInfo* si = new SpriteInfo();
                    ushort      w = fm.GetBEUShort();
                    ushort      h = fm.GetBEUShort();
                    si->OffsX = fm.GetBEShort();
                    si->OffsY = fm.GetBEShort();
                    dir_anim->NextX[ i ] = fm.GetBEShort();
                    dir_anim->NextY[ i ] = fm.GetBEShort();
                    uchar* data = fm.GetCurBuf();
                    dir_anim->Ind[ i ] = RequestFillAtlas( si, w, h, data );
                    fm.GoForward( w * h * 4 );
                }
            }
        }
        else
        {
            WriteLogF( _FUNC_, " - Unknown fast format version<%d>.\n", version );
        }
        return true;
    }

    // Load as real format
    fm.SetCurPos( 0 );
    return false;
}

bool SpriteManager::Flush()
{
    if( !curDrawQuad )
        return true;

    EnableVertexArray( quadsVertexArray, 4 * curDrawQuad );

    uint pos = 0;
    for( auto it = dipQueue.begin(), end = dipQueue.end(); it != end; ++it )
    {
        DipData& dip = *it;
        Effect*  effect = dip.SourceEffect;

        GL( glUseProgram( effect->Program ) );

        if( effect->ZoomFactor != -1 )
            GL( glUniform1f( effect->ZoomFactor, GameOpt.SpritesZoom ) );
        if( effect->ProjectionMatrix != -1 )
            GL( glUniformMatrix4fv( effect->ProjectionMatrix, 1, GL_FALSE, projectionMatrixCM[ 0 ] ) );
        if( effect->ColorMap != -1 && dip.SourceTexture )
        {
            if( dip.SourceTexture->Samples == 0.0f )
            {
                GL( glBindTexture( GL_TEXTURE_2D, dip.SourceTexture->Id ) );
            }
            else
            {
                GL( glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, dip.SourceTexture->Id ) );
                if( effect->ColorMapSamples != -1 )
                    GL( glUniform1f( effect->ColorMapSamples, dip.SourceTexture->Samples ) );
            }
            GL( glUniform1i( effect->ColorMap, 0 ) );
            if( effect->ColorMapSize != -1 )
                GL( glUniform4fv( effect->ColorMapSize, 1, dip.SourceTexture->SizeData ) );
        }
        if( effect->EggMap != -1 && sprEgg )
        {
            GL( glActiveTexture( GL_TEXTURE1 ) );
            GL( glBindTexture( GL_TEXTURE_2D, sprEgg->Atlas->TextureOwner->Id ) );
            GL( glActiveTexture( GL_TEXTURE0 ) );
            GL( glUniform1i( effect->EggMap, 1 ) );
            if( effect->EggMapSize != -1 )
                GL( glUniform4fv( effect->EggMapSize, 1, sprEgg->Atlas->TextureOwner->SizeData ) );
        }
        if( effect->SpriteBorder != -1 )
            GL( glUniform4f( effect->SpriteBorder, dip.SpriteBorder.L, dip.SpriteBorder.T, dip.SpriteBorder.R, dip.SpriteBorder.B ) );

        GLsizei count = 6 * dip.SpritesCount;
        if( effect->IsNeedProcess )
            GraphicLoader::EffectProcessVariables( effect, -1 );
        for( uint pass = 0; pass < effect->Passes; pass++ )
        {
            if( effect->IsNeedProcess )
                GraphicLoader::EffectProcessVariables( effect, pass );
            GL( glDrawElements( GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (void*) (size_t) ( pos * 2 ) ) );
        }

        pos += 6 * dip.SpritesCount;
    }
    dipQueue.clear();
    curDrawQuad = 0;

    GL( glUseProgram( 0 ) );
    DisableVertexArray( quadsVertexArray );

    return true;
}

bool SpriteManager::DrawSprite( uint id, int x, int y, uint color /* = 0 */ )
{
    if( !id )
        return false;

    SpriteInfo* si = sprData[ id ];
    if( !si )
        return false;

    Effect* effect = ( si->DrawEffect ? si->DrawEffect : Effect::Iface );
    if( dipQueue.empty() || dipQueue.back().SourceTexture != si->Atlas->TextureOwner || dipQueue.back().SourceEffect->Id != effect->Id )
        dipQueue.push_back( DipData( si->Atlas->TextureOwner, effect ) );
    else
        dipQueue.back().SpritesCount++;

    int pos = curDrawQuad * 4;

    if( !color )
        color = COLOR_IFACE;
    color = COLOR_SWAP_RB( color );

    vBuffer[ pos ].X = (float) x;
    vBuffer[ pos ].Y = (float) y + si->Height;
    vBuffer[ pos ].TU = si->SprRect.L;
    vBuffer[ pos ].TV = si->SprRect.B;
    vBuffer[ pos++ ].Diffuse = color;

    vBuffer[ pos ].X = (float) x;
    vBuffer[ pos ].Y = (float) y;
    vBuffer[ pos ].TU = si->SprRect.L;
    vBuffer[ pos ].TV = si->SprRect.T;
    vBuffer[ pos++ ].Diffuse = color;

    vBuffer[ pos ].X = (float) x + si->Width;
    vBuffer[ pos ].Y = (float) y;
    vBuffer[ pos ].TU = si->SprRect.R;
    vBuffer[ pos ].TV = si->SprRect.T;
    vBuffer[ pos++ ].Diffuse = color;

    vBuffer[ pos ].X = (float) x + si->Width;
    vBuffer[ pos ].Y = (float) y + si->Height;
    vBuffer[ pos ].TU = si->SprRect.R;
    vBuffer[ pos ].TV = si->SprRect.B;
    vBuffer[ pos ].Diffuse = color;

    if( ++curDrawQuad == drawQuadCount )
        Flush();
    return true;
}

bool SpriteManager::DrawSpritePattern( uint id, int x, int y, int w, int h, int spr_width /* = 0 */, int spr_height /* = 0 */, uint color /* = 0 */ )
{
    if( !id )
        return false;

    if( !w || !h )
        return false;

    SpriteInfo* si = sprData[ id ];
    if( !si )
        return false;

    float width = (float) si->Width;
    float height = (float) si->Height;

    if( spr_width && spr_height )
    {
        width = (float) spr_width;
        height = (float) spr_height;
    }
    else if( spr_width )
    {
        float ratio = (float) spr_width / width;
        width = (float) spr_width;
        height *= ratio;
    }
    else if( spr_height )
    {
        float ratio = (float) spr_height / height;
        height = (float) spr_height;
        width *= ratio;
    }

    if( !color )
        color = COLOR_IFACE;
    color = COLOR_SWAP_RB( color );

    Effect* effect = ( si->DrawEffect ? si->DrawEffect : Effect::Iface );

    float   last_right_offs = ( si->SprRect.R - si->SprRect.L ) / width;
    float   last_bottom_offs = ( si->SprRect.B - si->SprRect.T ) / height;

    for( float yy = (float) y, end_y = (float) y + h; yy < end_y; yy += height )
    {
        bool last_y = yy + height >= end_y;
        for( float xx = (float) x, end_x = (float) x + w; xx < end_x; xx += width )
        {
            bool last_x = xx + width >= end_x;

            if( dipQueue.empty() || dipQueue.back().SourceTexture != si->Atlas->TextureOwner || dipQueue.back().SourceEffect->Id != effect->Id )
                dipQueue.push_back( DipData( si->Atlas->TextureOwner, effect ) );
            else
                dipQueue.back().SpritesCount++;

            int   pos = curDrawQuad * 4;

            float local_width = last_x ? ( end_x - xx ) : width;
            float local_height = last_y ? ( end_y - yy ) : height;
            float local_right = last_x ? si->SprRect.L + last_right_offs * local_width : si->SprRect.R;
            float local_bottom = last_y ? si->SprRect.T + last_bottom_offs * local_height : si->SprRect.B;

            vBuffer[ pos ].X = xx;
            vBuffer[ pos ].Y = yy + local_height;
            vBuffer[ pos ].TU = si->SprRect.L;
            vBuffer[ pos ].TV = local_bottom;
            vBuffer[ pos++ ].Diffuse = color;

            vBuffer[ pos ].X = xx;
            vBuffer[ pos ].Y = yy;
            vBuffer[ pos ].TU = si->SprRect.L;
            vBuffer[ pos ].TV = si->SprRect.T;
            vBuffer[ pos++ ].Diffuse = color;

            vBuffer[ pos ].X = xx + local_width;
            vBuffer[ pos ].Y = yy;
            vBuffer[ pos ].TU = local_right;
            vBuffer[ pos ].TV = si->SprRect.T;
            vBuffer[ pos++ ].Diffuse = color;

            vBuffer[ pos ].X = xx + local_width;
            vBuffer[ pos ].Y = yy + local_height;
            vBuffer[ pos ].TU = local_right;
            vBuffer[ pos ].TV = local_bottom;
            vBuffer[ pos ].Diffuse = color;

            if( ++curDrawQuad == drawQuadCount )
                Flush();
        }
    }
    return true;
}

#pragma MESSAGE("Add 3d auto scaling.")
bool SpriteManager::DrawSpriteSize( uint id, int x, int y, float w, float h, bool stretch_up, bool center, uint color /* = 0 */ )
{
    if( !id )
        return false;

    SpriteInfo* si = sprData[ id ];
    if( !si )
        return false;

    float w_real = (float) si->Width;
    float h_real = (float) si->Height;
//      if(si->Anim3d)
//      {
//              si->Anim3d->SetupBorders();
//              INTRECT fb=si->Anim3d->GetBaseBorders();
//              w_real=fb.W();
//              h_real=fb.H();
//      }

    float wf = w_real;
    float hf = h_real;
    float k = min( w / w_real, h / h_real );

    if( k < 1.0f || ( k > 1.0f && stretch_up ) )
    {
        wf *= k;
        hf *= k;
    }

    if( center )
    {
        x += (int) ( ( w - wf ) / 2.0f );
        y += (int) ( ( h - hf ) / 2.0f );
    }

    Effect* effect = ( si->DrawEffect ? si->DrawEffect : Effect::Iface );
    if( dipQueue.empty() || dipQueue.back().SourceTexture != si->Atlas->TextureOwner || dipQueue.back().SourceEffect->Id != effect->Id )
        dipQueue.push_back( DipData( si->Atlas->TextureOwner, effect ) );
    else
        dipQueue.back().SpritesCount++;

    int pos = curDrawQuad * 4;

    if( !color )
        color = COLOR_IFACE;
    color = COLOR_SWAP_RB( color );

    vBuffer[ pos ].X = (float) x;
    vBuffer[ pos ].Y = (float) y + hf;
    vBuffer[ pos ].TU = si->SprRect.L;
    vBuffer[ pos ].TV = si->SprRect.B;
    vBuffer[ pos++ ].Diffuse = color;

    vBuffer[ pos ].X = (float) x;
    vBuffer[ pos ].Y = (float) y;
    vBuffer[ pos ].TU = si->SprRect.L;
    vBuffer[ pos ].TV = si->SprRect.T;
    vBuffer[ pos++ ].Diffuse = color;

    vBuffer[ pos ].X = (float) x + wf;
    vBuffer[ pos ].Y = (float) y;
    vBuffer[ pos ].TU = si->SprRect.R;
    vBuffer[ pos ].TV = si->SprRect.T;
    vBuffer[ pos++ ].Diffuse = color;

    vBuffer[ pos ].X = (float) x + wf;
    vBuffer[ pos ].Y = (float) y + hf;
    vBuffer[ pos ].TU = si->SprRect.R;
    vBuffer[ pos ].TV = si->SprRect.B;
    vBuffer[ pos ].Diffuse = color;

    if( ++curDrawQuad == drawQuadCount )
        Flush();
    return true;
}

void SpriteManager::PrepareSquare( PointVec& points, Rect r, uint color )
{
    points.push_back( PrepPoint( r.L, r.B, color, NULL, NULL ) );
    points.push_back( PrepPoint( r.L, r.T, color, NULL, NULL ) );
    points.push_back( PrepPoint( r.R, r.B, color, NULL, NULL ) );
    points.push_back( PrepPoint( r.L, r.T, color, NULL, NULL ) );
    points.push_back( PrepPoint( r.R, r.T, color, NULL, NULL ) );
    points.push_back( PrepPoint( r.R, r.B, color, NULL, NULL ) );
}

void SpriteManager::PrepareSquare( PointVec& points, Point lt, Point rt, Point lb, Point rb, uint color )
{
    points.push_back( PrepPoint( lb.X, lb.Y, color, NULL, NULL ) );
    points.push_back( PrepPoint( lt.X, lt.Y, color, NULL, NULL ) );
    points.push_back( PrepPoint( rb.X, rb.Y, color, NULL, NULL ) );
    points.push_back( PrepPoint( lt.X, lt.Y, color, NULL, NULL ) );
    points.push_back( PrepPoint( rt.X, rt.Y, color, NULL, NULL ) );
    points.push_back( PrepPoint( rb.X, rb.Y, color, NULL, NULL ) );
}

uint SpriteManager::PackColor( int r, int g, int b )
{
    r = CLAMP( r, 0, 255 );
    g = CLAMP( g, 0, 255 );
    b = CLAMP( b, 0, 255 );
    return COLOR_RGB( r, g, b );
}

void SpriteManager::GetDrawRect( Sprite* prep, Rect& rect )
{
    uint id = ( prep->PSprId ? *prep->PSprId : prep->SprId );
    if( id >= sprData.size() )
        return;
    SpriteInfo* si = sprData[ id ];
    if( !si )
        return;

    int x = prep->ScrX - si->Width / 2 + si->OffsX;
    int y = prep->ScrY - si->Height + si->OffsY;
    if( prep->OffsX )
        x += *prep->OffsX;
    if( prep->OffsY )
        y += *prep->OffsY;
    rect.L = x;
    rect.T = y;
    rect.R = x + si->Width;
    rect.B = y + si->Height;
}

void SpriteManager::InitializeEgg( const char* egg_name )
{
    #ifndef DISABLE_EGG
    eggValid = false;
    eggHx = eggHy = eggX = eggY = 0;
    AnyFrames* egg_frames = LoadAnimation( egg_name, PT_ART_MISC );
    if( egg_frames )
    {
        sprEgg = GetSpriteInfo( egg_frames->Ind[ 0 ] );
        AnyFrames::Destroy( egg_frames );
        eggSprWidth = sprEgg->Width;
        eggSprHeight = sprEgg->Height;
        eggAtlasWidth = (float) atlasWidth;
        eggAtlasHeight = (float) atlasHeight;
    }
    else
    {
        WriteLogF( _FUNC_, " - Load sprite<%s> fail. Egg disabled.\n", egg_name );
    }
    #endif
}

bool SpriteManager::CompareHexEgg( ushort hx, ushort hy, int egg_type )
{
    if( egg_type == EGG_ALWAYS )
        return true;
    if( eggHy == hy && hx & 1 && !( eggHx & 1 ) )
        hy--;
    switch( egg_type )
    {
    case EGG_X:
        if( hx >= eggHx )
            return true;
        break;
    case EGG_Y:
        if( hy >= eggHy )
            return true;
        break;
    case EGG_X_AND_Y:
        if( hx >= eggHx || hy >= eggHy )
            return true;
        break;
    case EGG_X_OR_Y:
        if( hx >= eggHx && hy >= eggHy )
            return true;
        break;
    default:
        break;
    }
    return false;
}

void SpriteManager::SetEgg( ushort hx, ushort hy, Sprite* spr )
{
    if( !sprEgg )
        return;

    uint        id = ( spr->PSprId ? *spr->PSprId : spr->SprId );
    SpriteInfo* si = sprData[ id ];
    if( !si )
        return;

    eggX = spr->ScrX + si->OffsX - sprEgg->Width / 2 + *spr->OffsX;
    eggY = spr->ScrY - si->Height / 2 + si->OffsY - sprEgg->Height / 2 + *spr->OffsY;
    eggHx = hx;
    eggHy = hy;
    eggValid = true;
}

bool SpriteManager::DrawSprites( Sprites& dtree, bool collect_contours, bool use_egg, int draw_oder_from, int draw_oder_to )
{
    PointVec borders;

    if( !eggValid )
        use_egg = false;
    int  ex = eggX + GameOpt.ScrOx;
    int  ey = eggY + GameOpt.ScrOy;
    uint cur_tick = Timer::FastTick();

    for( auto it = dtree.Begin(), end = dtree.End(); it != end; ++it )
    {
        // Data
        Sprite* spr = *it;
        if( !spr->Valid )
            continue;

        if( spr->DrawOrderType < draw_oder_from )
            continue;
        if( spr->DrawOrderType > draw_oder_to )
            break;

        uint        id = ( spr->PSprId ? *spr->PSprId : spr->SprId );
        SpriteInfo* si = sprData[ id ];
        if( !si )
            continue;

        // Coords
        int x = spr->ScrX - si->Width / 2 + si->OffsX;
        int y = spr->ScrY - si->Height + si->OffsY;
        x += GameOpt.ScrOx;
        y += GameOpt.ScrOy;
        if( spr->OffsX )
            x += *spr->OffsX;
        if( spr->OffsY )
            y += *spr->OffsY;
        float zoom = GameOpt.SpritesZoom;

        // Base color
        uint cur_color;
        if( spr->Color )
            cur_color = ( spr->Color | 0xFF000000 );
        else
            cur_color = baseColor;

        // Light
        if( spr->Light )
        {
            int    lr = *spr->Light;
            int    lg = *( spr->Light + 1 );
            int    lb = *( spr->Light + 2 );
            uchar& r = ( (uchar*) &cur_color )[ 2 ];
            uchar& g = ( (uchar*) &cur_color )[ 1 ];
            uchar& b = ( (uchar*) &cur_color )[ 0 ];
            int    ir = (int) r + lr;
            int    ig = (int) g + lg;
            int    ib = (int) b + lb;
            if( ir > 0xFF )
                ir = 0xFF;
            if( ig > 0xFF )
                ig = 0xFF;
            if( ib > 0xFF )
                ib = 0xFF;
            r = ir;
            g = ig;
            b = ib;
        }

        // Alpha
        if( spr->Alpha )
        {
            ( (uchar*) &cur_color )[ 3 ] = *spr->Alpha;
        }

        // Process flashing
        if( spr->FlashMask )
        {
            static int  cnt = 0;
            static uint tick = cur_tick + 100;
            static bool add = true;
            if( cur_tick >= tick )
            {
                cnt += ( add ? 10 : -10 );
                if( cnt > 40 )
                {
                    cnt = 40;
                    add = false;
                }
                else if( cnt < -40 )
                {
                    cnt = -40;
                    add = true;
                }
                tick = cur_tick + 100;
            }
            int r = ( ( cur_color >> 16 ) & 0xFF ) + cnt;
            int g = ( ( cur_color >> 8 ) & 0xFF ) + cnt;
            int b = ( cur_color & 0xFF ) + cnt;
            r = CLAMP( r, 0, 0xFF );
            g = CLAMP( g, 0, 0xFF );
            b = CLAMP( b, 0, 0xFF );
            ( (uchar*) &cur_color )[ 2 ] = r;
            ( (uchar*) &cur_color )[ 1 ] = g;
            ( (uchar*) &cur_color )[ 0 ] = b;
            cur_color &= spr->FlashMask;
        }

        // Fix color
        cur_color = COLOR_SWAP_RB( cur_color );

        // Check borders
        if( x / zoom > modeWidth || ( x + si->Width ) / zoom < 0 || y / zoom > modeHeight || ( y + si->Height ) / zoom < 0 )
            continue;

        // 2d sprite
        // Egg process
        #ifndef DISABLE_EGG
        bool egg_added = false;
        if( use_egg && spr->EggType && CompareHexEgg( spr->HexX, spr->HexY, spr->EggType ) )
        {
            int x1 = x - ex;
            int y1 = y - ey;
            int x2 = x1 + si->Width;
            int y2 = y1 + si->Height;

            if( spr->CutType )
            {
                x1 += (int) spr->CutX;
                x2 = x1 + (int) spr->CutW;
            }

            if( !( x1 >= eggSprWidth || y1 >= eggSprHeight || x2 < 0 || y2 < 0 ) )
            {
                x1 = max( x1, 0 );
                y1 = max( y1, 0 );
                x2 = min( x2, eggSprWidth );
                y2 = min( y2, eggSprHeight );

                float x1f = (float) ( x1 + ATLAS_SPRITES_PADDING );
                float x2f = (float) ( x2 + ATLAS_SPRITES_PADDING );
                float y1f = (float) ( y1 + ATLAS_SPRITES_PADDING );
                float y2f = (float) ( y2 + ATLAS_SPRITES_PADDING );

                int   pos = curDrawQuad * 4;
                vBuffer[ pos + 0 ].TUEgg = x1f / eggAtlasWidth;
                vBuffer[ pos + 0 ].TVEgg = y2f / eggAtlasHeight;
                vBuffer[ pos + 1 ].TUEgg = x1f / eggAtlasWidth;
                vBuffer[ pos + 1 ].TVEgg = y1f / eggAtlasHeight;
                vBuffer[ pos + 2 ].TUEgg = x2f / eggAtlasWidth;
                vBuffer[ pos + 2 ].TVEgg = y1f / eggAtlasHeight;
                vBuffer[ pos + 3 ].TUEgg = x2f / eggAtlasWidth;
                vBuffer[ pos + 3 ].TVEgg = y2f / eggAtlasHeight;

                egg_added = true;
            }
        }
        #endif

        // Choose effect
        Effect* effect = ( spr->DrawEffect ? *spr->DrawEffect : NULL );
        if( !effect )
            effect = ( si->DrawEffect ? si->DrawEffect : Effect::Generic );

        // Choose atlas
        if( dipQueue.empty() || dipQueue.back().SourceTexture != si->Atlas->TextureOwner || dipQueue.back().SourceEffect->Id != effect->Id )
            dipQueue.push_back( DipData( si->Atlas->TextureOwner, effect ) );
        else
            dipQueue.back().SpritesCount++;

        // Casts
        float xf = (float) x / zoom;
        float yf = (float) y / zoom;
        float wf = (float) si->Width / zoom;
        float hf = (float) si->Height / zoom;

        // Fill buffer
        int pos = curDrawQuad * 4;

        vBuffer[ pos ].X = xf;
        vBuffer[ pos ].Y = yf + hf;
        vBuffer[ pos ].TU = si->SprRect.L;
        vBuffer[ pos ].TV = si->SprRect.B;
        vBuffer[ pos++ ].Diffuse = cur_color;

        vBuffer[ pos ].X = xf;
        vBuffer[ pos ].Y = yf;
        vBuffer[ pos ].TU = si->SprRect.L;
        vBuffer[ pos ].TV = si->SprRect.T;
        vBuffer[ pos++ ].Diffuse = cur_color;

        vBuffer[ pos ].X = xf + wf;
        vBuffer[ pos ].Y = yf;
        vBuffer[ pos ].TU = si->SprRect.R;
        vBuffer[ pos ].TV = si->SprRect.T;
        vBuffer[ pos++ ].Diffuse = cur_color;

        vBuffer[ pos ].X = xf + wf;
        vBuffer[ pos ].Y = yf + hf;
        vBuffer[ pos ].TU = si->SprRect.R;
        vBuffer[ pos ].TV = si->SprRect.B;
        vBuffer[ pos++ ].Diffuse = cur_color;

        // Cutted sprite
        if( spr->CutType )
        {
            xf = (float) ( x + spr->CutX ) / zoom;
            wf = spr->CutW / zoom;
            vBuffer[ pos - 4 ].X = xf;
            vBuffer[ pos - 4 ].TU = spr->CutTexL;
            vBuffer[ pos - 3 ].X = xf;
            vBuffer[ pos - 3 ].TU = spr->CutTexL;
            vBuffer[ pos - 2 ].X = xf + wf;
            vBuffer[ pos - 2 ].TU = spr->CutTexR;
            vBuffer[ pos - 1 ].X = xf + wf;
            vBuffer[ pos - 1 ].TU = spr->CutTexR;
        }

        // Set default texture coordinates for egg texture
        #ifndef DISABLE_EGG
        if( !egg_added && vBuffer[ pos - 1 ].TUEgg != -1.0f )
        {
            vBuffer[ pos - 1 ].TUEgg = -1.0f;
            vBuffer[ pos - 2 ].TUEgg = -1.0f;
            vBuffer[ pos - 3 ].TUEgg = -1.0f;
            vBuffer[ pos - 4 ].TUEgg = -1.0f;
        }
        #endif

        // Draw
        if( ++curDrawQuad == drawQuadCount )
            Flush();

        #ifdef FONLINE_MAPPER
        // Corners indication
        if( GameOpt.ShowCorners && spr->EggType )
        {
            PointVec corner;
            float    cx = wf / 2.0f;

            switch( spr->EggType )
            {
            case EGG_ALWAYS:
                PrepareSquare( corner, RectF( xf + cx - 2.0f, yf + hf - 50.0f, xf + cx + 2.0f, yf + hf ), 0x5FFFFF00 );
                break;
            case EGG_X:
                PrepareSquare( corner, PointF( xf + cx - 5.0f, yf + hf - 55.0f ), PointF( xf + cx + 5.0f, yf + hf - 45.0f ), PointF( xf + cx - 5.0f, yf + hf - 5.0f ), PointF( xf + cx + 5.0f, yf + hf + 5.0f ), 0x5F00AF00 );
                break;
            case EGG_Y:
                PrepareSquare( corner, PointF( xf + cx - 5.0f, yf + hf - 49.0f ), PointF( xf + cx + 5.0f, yf + hf - 52.0f ), PointF( xf + cx - 5.0f, yf + hf + 1.0f ), PointF( xf + cx + 5.0f, yf + hf - 2.0f ), 0x5F00FF00 );
                break;
            case EGG_X_AND_Y:
                PrepareSquare( corner, PointF( xf + cx - 10.0f, yf + hf - 49.0f ), PointF( xf + cx, yf + hf - 52.0f ), PointF( xf + cx - 10.0f, yf + hf + 1.0f ), PointF( xf + cx, yf + hf - 2.0f ), 0x5FFF0000 );
                PrepareSquare( corner, PointF( xf + cx, yf + hf - 55.0f ), PointF( xf + cx + 10.0f, yf + hf - 45.0f ), PointF( xf + cx, yf + hf - 5.0f ), PointF( xf + cx + 10.0f, yf + hf + 5.0f ), 0x5FFF0000 );
                break;
            case EGG_X_OR_Y:
                PrepareSquare( corner, PointF( xf + cx, yf + hf - 49.0f ), PointF( xf + cx + 10.0f, yf + hf - 52.0f ), PointF( xf + cx, yf + hf + 1.0f ), PointF( xf + cx + 10.0f, yf + hf - 2.0f ), 0x5FAF0000 );
                PrepareSquare( corner, PointF( xf + cx - 10.0f, yf + hf - 55.0f ), PointF( xf + cx, yf + hf - 45.0f ), PointF( xf + cx - 10.0f, yf + hf - 5.0f ), PointF( xf + cx, yf + hf + 5.0f ), 0x5FAF0000 );
            default:
                break;
            }

            DrawPoints( corner, PRIMITIVE_TRIANGLELIST );
        }

        // Cuts
        if( GameOpt.ShowSpriteCuts && spr->CutType )
        {
            PointVec cut;
            float    z = zoom;
            float    oy = ( spr->CutType == SPRITE_CUT_HORIZONTAL ? 3.0f : -5.2f ) / z;
            float    x1 = (float) ( spr->ScrX - si->Width / 2 + spr->CutX + GameOpt.ScrOx + 1.0f ) / z;
            float    y1 = (float) ( spr->ScrY + spr->CutOyL + GameOpt.ScrOy ) / z;
            float    x2 = (float) ( spr->ScrX - si->Width / 2 + spr->CutX + spr->CutW + GameOpt.ScrOx - 1.0f ) / z;
            float    y2 = (float) ( spr->ScrY + spr->CutOyR + GameOpt.ScrOy ) / z;
            PrepareSquare( cut, PointF( x1, y1 - 80.0f / z + oy ), PointF( x2, y2 - 80.0f / z - oy ), PointF( x1, y1 + oy ), PointF( x2, y2 - oy ), 0x4FFFFF00 );
            PrepareSquare( cut, RectF( xf, yf, xf + 1.0f, yf + hf ), 0x4F000000 );
            PrepareSquare( cut, RectF( xf + wf, yf, xf + wf + 1.0f, yf + hf ), 0x4F000000 );
            DrawPoints( cut, PRIMITIVE_TRIANGLELIST );
        }

        // Draw order
        if( GameOpt.ShowDrawOrder )
        {
            float z = zoom;
            int   x1, y1;

            if( !spr->CutType )
            {
                x1 = (int) ( ( spr->ScrX + GameOpt.ScrOx ) / z );
                y1 = (int) ( ( spr->ScrY + GameOpt.ScrOy ) / z );
            }
            else
            {

                x1 = (int) ( ( spr->ScrX - si->Width / 2 + spr->CutX + GameOpt.ScrOx + 1.0f ) / z );
                y1 = (int) ( ( spr->ScrY + spr->CutOyL + GameOpt.ScrOy ) / z );
            }

            if( spr->DrawOrderType >= DRAW_ORDER_FLAT && spr->DrawOrderType < DRAW_ORDER )
                y1 -= (int) ( 40.0f / z );

            char str[ 32 ];
            Str::Format( str, "%u", spr->TreeIndex );
            DrawStr( Rect( x1, y1, x1 + 100, y1 + 100 ), str, 0 );
        }
        #endif

        // Process contour effect
        if( collect_contours && spr->ContourType )
            CollectContour( x, y, si, spr );
    }

    Flush();

    if( GameOpt.DebugInfo )
        DrawPoints( borders, PRIMITIVE_TRIANGLELIST );
    return true;
}

bool SpriteManager::IsPixNoTransp( uint spr_id, int offs_x, int offs_y, bool with_zoom )
{
    uint color = GetPixColor( spr_id, offs_x, offs_y, with_zoom );
    return ( color & 0xFF000000 ) != 0;
}

uint SpriteManager::GetPixColor( uint spr_id, int offs_x, int offs_y, bool with_zoom )
{
    if( offs_x < 0 || offs_y < 0 )
        return 0;
    SpriteInfo* si = GetSpriteInfo( spr_id );
    if( !si )
        return 0;

    // 2d animation
    if( with_zoom && ( offs_x > si->Width / GameOpt.SpritesZoom || offs_y > si->Height / GameOpt.SpritesZoom ) )
        return 0;
    if( !with_zoom && ( offs_x > si->Width || offs_y > si->Height ) )
        return 0;

    if( with_zoom )
    {
        offs_x = (int) ( offs_x * GameOpt.SpritesZoom );
        offs_y = (int) ( offs_y * GameOpt.SpritesZoom );
    }

    offs_x += (int) ( si->Atlas->TextureOwner->SizeData[ 0 ] * si->SprRect.L );
    offs_y += (int) ( si->Atlas->TextureOwner->SizeData[ 1 ] * si->SprRect.T );
    return GetRenderTargetPixel( si->Atlas->RT, offs_x, offs_y );
}

bool SpriteManager::IsEggTransp( int pix_x, int pix_y )
{
    if( !eggValid )
        return false;

    int ex = eggX + GameOpt.ScrOx;
    int ey = eggY + GameOpt.ScrOy;
    int ox = pix_x - (int) ( ex / GameOpt.SpritesZoom );
    int oy = pix_y - (int) ( ey / GameOpt.SpritesZoom );

    if( ox < 0 || oy < 0 || ox >= int(eggAtlasWidth / GameOpt.SpritesZoom) || oy >= int(eggAtlasHeight / GameOpt.SpritesZoom) )
        return false;

    ox = (int) ( ox * GameOpt.SpritesZoom );
    oy = (int) ( oy * GameOpt.SpritesZoom );

    return GetRenderTargetPixel( sprEgg->Atlas->RT, ox, oy ) < 170;
}

bool SpriteManager::DrawPoints( PointVec& points, int prim, float* zoom /* = NULL */, RectF* stencil /* = NULL */, PointF* offset /* = NULL */, Effect* effect /* = NULL */ )
{
    if( points.empty() )
        return true;
    Flush();

    if( !effect )
        effect = Effect::Primitive;

    // Check primitives
    uint   count = (uint) points.size();
    int    prim_count = (int) count;
    GLenum prim_type;
    switch( prim )
    {
    case PRIMITIVE_POINTLIST:
        prim_type = GL_POINTS;
        break;
    case PRIMITIVE_LINELIST:
        prim_type = GL_LINES;
        prim_count /= 2;
        break;
    case PRIMITIVE_LINESTRIP:
        prim_type = GL_LINE_STRIP;
        prim_count -= 1;
        break;
    case PRIMITIVE_TRIANGLELIST:
        prim_type = GL_TRIANGLES;
        prim_count /= 3;
        break;
    case PRIMITIVE_TRIANGLESTRIP:
        prim_type = GL_TRIANGLE_STRIP;
        prim_count -= 2;
        break;
    case PRIMITIVE_TRIANGLEFAN:
        prim_type = GL_TRIANGLE_FAN;
        prim_count -= 2;
        break;
    default:
        return false;
    }
    if( prim_count <= 0 )
        return false;

    // Draw stencil
    if( stencil )
        EnableStencil( *stencil );

    // Resize buffers
    if( vBuffer.size() < count )
        vBuffer.resize( count );
    if( pointsVertexArray->VCount < (uint) vBuffer.size() )
        InitVertexArray( pointsVertexArray, false, (uint) vBuffer.size() );

    // Collect data
    for( uint i = 0; i < count; i++ )
    {
        PrepPoint& point = points[ i ];
        float      x = (float) point.PointX;
        float      y = (float) point.PointY;
        if( point.PointOffsX )
            x += (float) *point.PointOffsX;
        if( point.PointOffsY )
            y += (float) *point.PointOffsY;
        if( zoom )
            x /= *zoom, y /= *zoom;
        if( offset )
            x += offset->X, y += offset->Y;

        memzero( &vBuffer[ i ], sizeof( Vertex2D ) );
        vBuffer[ i ].X = x;
        vBuffer[ i ].Y = y;

        uint color = point.PointColor;
        color = COLOR_SWAP_RB( color );
        vBuffer[ i ].Diffuse = color;
    }

    // Draw
    EnableVertexArray( pointsVertexArray, count );
    GL( glUseProgram( effect->Program ) );

    if( effect->ProjectionMatrix != -1 )
        GL( glUniformMatrix4fv( effect->ProjectionMatrix, 1, GL_FALSE, projectionMatrixCM[ 0 ] ) );

    if( effect->IsNeedProcess )
        GraphicLoader::EffectProcessVariables( effect, -1 );
    for( uint pass = 0; pass < effect->Passes; pass++ )
    {
        if( effect->IsNeedProcess )
            GraphicLoader::EffectProcessVariables( effect, pass );
        GL( glDrawElements( prim_type, count, GL_UNSIGNED_SHORT, (void*) 0 ) );
    }

    GL( glUseProgram( 0 ) );
    DisableVertexArray( pointsVertexArray );

    // Finish
    if( stencil )
        DisableStencil( true );
    return true;
}

bool SpriteManager::DrawContours()
{
    if( contoursAdded && rtContours && rtContoursMid )
    {
        // Draw collected contours
        DrawRenderTarget( rtContours, true );

        // Clean render targets
        PushRenderTarget( rtContours );
        ClearCurrentRenderTarget( 0 );
        PopRenderTarget();
        PushRenderTarget( rtContoursMid );
        ClearCurrentRenderTarget( 0 );
        PopRenderTarget();
        contoursAdded = false;
    }
    return true;
}

bool SpriteManager::CollectContour( int x, int y, SpriteInfo* si, Sprite* spr )
{
    if( !rtContours || !rtContoursMid )
        return true;

    Rect     borders = Rect( x - 1, y - 1, x + si->Width + 1, y + si->Height + 1 );
    Texture* texture = si->Atlas->TextureOwner;
    RectF    textureuv, sprite_border;

    if( borders.L >= modeWidth * GameOpt.SpritesZoom || borders.R < 0 || borders.T >= modeHeight * GameOpt.SpritesZoom || borders.B < 0 )
        return true;

    if( GameOpt.SpritesZoom == 1.0f )
    {
        RectF& sr = si->SprRect;
        float  txw = texture->SizeData[ 2 ];
        float  txh = texture->SizeData[ 3 ];
        textureuv( sr.L - txw, sr.T - txh, sr.R + txw, sr.B + txh );
        sprite_border = textureuv;
    }
    else
    {
        RectF& sr = si->SprRect;
        borders( (int) ( x / GameOpt.SpritesZoom ), (int) ( y / GameOpt.SpritesZoom ),
                 (int) ( ( x + si->Width ) / GameOpt.SpritesZoom ), (int) ( ( y + si->Height ) / GameOpt.SpritesZoom ) );
        RectF bordersf( (float) borders.L, (float) borders.T, (float) borders.R, (float) borders.B );
        float mid_height = rtContoursMid->TargetTexture->SizeData[ 1 ];

        PushRenderTarget( rtContoursMid );

        uint pos = 0;
        vBuffer[ pos ].X = bordersf.L;
        vBuffer[ pos ].Y = mid_height - bordersf.B;
        vBuffer[ pos ].TU = sr.L;
        vBuffer[ pos++ ].TV = sr.B;
        vBuffer[ pos ].X = bordersf.L;
        vBuffer[ pos ].Y = mid_height - bordersf.T;
        vBuffer[ pos ].TU = sr.L;
        vBuffer[ pos++ ].TV = sr.T;
        vBuffer[ pos ].X = bordersf.R;
        vBuffer[ pos ].Y = mid_height - bordersf.T;
        vBuffer[ pos ].TU = sr.R;
        vBuffer[ pos++ ].TV = sr.T;
        vBuffer[ pos ].X = bordersf.R;
        vBuffer[ pos ].Y = mid_height - bordersf.B;
        vBuffer[ pos ].TU = sr.R;
        vBuffer[ pos++ ].TV = sr.B;

        curDrawQuad = 1;
        dipQueue.push_back( DipData( texture, Effect::FlushRenderTarget ) );
        Flush();

        PopRenderTarget();

        texture = rtContoursMid->TargetTexture;
        float tw = texture->SizeData[ 0 ];
        float th = texture->SizeData[ 1 ];
        borders.L--, borders.T--, borders.R++, borders.B++;
        textureuv( (float) borders.L / tw, (float) borders.T / th, (float) borders.R / tw, (float) borders.B / th );
        sprite_border = textureuv;
    }

    uint contour_color = 0;
    if( spr->ContourType == CONTOUR_RED )
        contour_color = 0xFFAF0000;
    else if( spr->ContourType == CONTOUR_YELLOW )
        contour_color = 0x00AFAF00;  // Disable flashing by passing alpha == 0.0
    else if( spr->ContourType == CONTOUR_CUSTOM )
        contour_color = spr->ContourColor;
    else
        contour_color = 0xFFAFAFAF;
    contour_color = COLOR_SWAP_RB( contour_color );

    RectF borders_pos( (float) borders.L, (float) borders.T, (float) borders.R, (float) borders.B );

    PushRenderTarget( rtContours );

    uint pos = 0;
    vBuffer[ pos ].X = borders_pos.L;
    vBuffer[ pos ].Y = borders_pos.B;
    vBuffer[ pos ].TU = textureuv.L;
    vBuffer[ pos ].TV = textureuv.B;
    vBuffer[ pos++ ].Diffuse = contour_color;
    vBuffer[ pos ].X = borders_pos.L;
    vBuffer[ pos ].Y = borders_pos.T;
    vBuffer[ pos ].TU = textureuv.L;
    vBuffer[ pos ].TV = textureuv.T;
    vBuffer[ pos++ ].Diffuse = contour_color;
    vBuffer[ pos ].X = borders_pos.R;
    vBuffer[ pos ].Y = borders_pos.T;
    vBuffer[ pos ].TU = textureuv.R;
    vBuffer[ pos ].TV = textureuv.T;
    vBuffer[ pos++ ].Diffuse = contour_color;
    vBuffer[ pos ].X = borders_pos.R;
    vBuffer[ pos ].Y = borders_pos.B;
    vBuffer[ pos ].TU = textureuv.R;
    vBuffer[ pos ].TV = textureuv.B;
    vBuffer[ pos++ ].Diffuse = contour_color;

    curDrawQuad = 1;
    dipQueue.push_back( DipData( texture, Effect::Contour ) );
    dipQueue.back().SpriteBorder = sprite_border;
    Flush();

    PopRenderTarget();
    contoursAdded = true;
    return true;
}
