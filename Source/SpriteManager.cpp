#include "StdAfx.h"
#include "SpriteManager.h"
#include "IniParser.h"
#include "Crypt.h"
#include "F2Palette.h"

SpriteManager SprMngr;
AnyFrames*    SpriteManager::DummyAnimation = NULL;

#define TEX_FRMT             D3DFMT_A8R8G8B8
#define SPR_BUFFER_COUNT     ( 10000 )
#define SURF_SPRITES_OFFS    ( 2 )

#ifdef FO_D3D
# define SURF_POINT( lr, x, y )    ( *( (uint*) ( (uchar*) lr.pBits + lr.Pitch * ( y ) + ( x ) * 4 ) ) )
#endif

SpriteManager::SpriteManager(): isInit( 0 ), flushSprCnt( 0 ), curSprCnt( 0 ), SurfType( 0 ), SurfFilterNearest( false ),
#ifdef FO_D3D
                                spr3dRT( NULL ), spr3dRTEx( NULL ), spr3dDS( NULL ), spr3dRTData( NULL ), spr3dSurfWidth( 256 ), spr3dSurfHeight( 256 ),
#endif
                                sceneBeginned( false ), d3dDevice( NULL ), vbMain( NULL ), ibMain( NULL ), PreRestore( NULL ), PostRestore( NULL ), baseTextureSize( 0 ),
                                eggValid( false ), eggHx( 0 ), eggHy( 0 ), eggX( 0 ), eggY( 0 ), eggOX( NULL ), eggOY( NULL ), sprEgg( NULL ), eggSurfWidth( 1.0f ), eggSurfHeight( 1.0f ), eggSprWidth( 1 ), eggSprHeight( 1 ),
                                contoursTexture( NULL ), contoursTextureSurf( NULL ), contoursMidTexture( NULL ), contoursMidTextureSurf( NULL ), contours3dRT( NULL ),
                                contoursPS( NULL ), contoursCT( NULL ), contoursAdded( false ),
                                modeWidth( 0 ), modeHeight( 0 )
{
    memzero( &presentParams, sizeof( presentParams ) );
    memzero( &mngrParams, sizeof( mngrParams ) );
    memzero( &deviceCaps, sizeof( deviceCaps ) );
    baseColor = COLOR_ARGB( 255, 128, 128, 128 );
    surfList.reserve( 100 );
    dipQueue.reserve( 1000 );
    contoursConstWidthStep = NULL;
    contoursConstHeightStep = NULL;
    contoursConstSpriteBorders = NULL;
    contoursConstSpriteBordersHeight = NULL;
    contoursConstContourColor = NULL;
    contoursConstContourColorOffs = NULL;
    memzero( sprDefaultEffect, sizeof( sprDefaultEffect ) );
    curDefaultEffect = NULL;

    #ifdef FO_D3D
    direct3D = NULL;
    vbPoints = NULL;
    vbPointsSize = 0;
    #else
    vaMain = 0;
    vbMain = 0;
    ibMain = 0;
    ibDirect = 0;
    memzero( projectionMatrix, sizeof( projectionMatrix ) );
    #endif
}

bool SpriteManager::Init( SpriteMngrParams& params )
{
    if( isInit )
        return true;

    WriteLog( "Sprite manager initialization...\n" );

    mngrParams = params;
    PreRestore = params.PreRestoreFunc;
    PostRestore = params.PostRestoreFunc;
    flushSprCnt = GameOpt.FlushVal;
    baseTextureSize = GameOpt.BaseTexture;
    modeWidth = GameOpt.ScreenWidth;
    modeHeight = GameOpt.ScreenHeight;
    curSprCnt = 0;

    #ifdef FO_D3D
    direct3D = Direct3DCreate9( D3D_SDK_VERSION );
    if( !direct3D )
    {
        WriteLog( "Create Direct3D fail.\n" );
        return false;
    }

    memzero( &deviceCaps, sizeof( deviceCaps ) );
    D3D_HR( direct3D->GetDeviceCaps( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &deviceCaps ) );

    memzero( &presentParams, sizeof( presentParams ) );
    presentParams.BackBufferCount = 1;
    presentParams.Windowed = ( GameOpt.FullScreen ? FALSE : TRUE );
    presentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentParams.EnableAutoDepthStencil = TRUE;
    presentParams.AutoDepthStencilFormat = D3DFMT_D24S8;
    presentParams.hDeviceWindow = fl_xid( MainWindow );
    presentParams.BackBufferWidth = modeWidth;
    presentParams.BackBufferHeight = modeHeight;
    presentParams.BackBufferFormat = D3DFMT_X8R8G8B8;
    if( !GameOpt.VSync )
        presentParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    presentParams.MultiSampleType = (D3DMULTISAMPLE_TYPE) GameOpt.MultiSampling;
    if( GameOpt.MultiSampling < 0 )
    {
        presentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        for( int i = 4; i >= 1; i-- )
        {
            if( SUCCEEDED( direct3D->CheckDeviceMultiSampleType( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                                                 presentParams.BackBufferFormat, !GameOpt.FullScreen, (D3DMULTISAMPLE_TYPE) i, NULL ) ) )
            {
                presentParams.MultiSampleType = (D3DMULTISAMPLE_TYPE) i;
                break;
            }
        }
    }
    if( presentParams.MultiSampleType != D3DMULTISAMPLE_NONE )
    {
        HRESULT hr = direct3D->CheckDeviceMultiSampleType( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, presentParams.BackBufferFormat, !GameOpt.FullScreen,
                                                           presentParams.MultiSampleType, &presentParams.MultiSampleQuality );
        if( FAILED( hr ) )
        {
            WriteLog( "Multisampling %dx not supported. Disabled.\n", (int) presentParams.MultiSampleType );
            presentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
            presentParams.MultiSampleQuality = 0;
        }
        if( presentParams.MultiSampleQuality )
            presentParams.MultiSampleQuality--;
    }

    int vproc = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    if( !GameOpt.SoftwareSkinning && deviceCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT &&
        deviceCaps.VertexShaderVersion >= D3DPS_VERSION( 2, 0 ) && deviceCaps.MaxVertexBlendMatrices >= 2 )
        vproc = D3DCREATE_HARDWARE_VERTEXPROCESSING;

    D3D_HR( direct3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, fl_xid( MainWindow ), vproc, &presentParams, &d3dDevice ) );
    #else
    # ifdef FO_WINDOWS
    PIXELFORMATDESCRIPTOR pfd;
    memzero( &pfd, sizeof( PIXELFORMATDESCRIPTOR ) );
    pfd.nSize = sizeof( PIXELFORMATDESCRIPTOR );
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 16;
    dcScreen = GetDC( fl_xid( MainWindow ) );
    GLuint pixel_format = ChoosePixelFormat( dcScreen, &pfd );
    SetPixelFormat( dcScreen, pixel_format, &pfd );
    HGLRC  rc = wglCreateContext( dcScreen );
    wglMakeCurrent( dcScreen, rc );
    # endif

    // OpenGL extensions
    glewInit();
    # define CHECK_EXTENSION( ext )                                       \
        if( !GLEW_ARB_ ## ext )                                           \
        {                                                                 \
            WriteLog( " OpenGL extension '" # ext "' not supported.\n" ); \
            return false;                                                 \
        }
    CHECK_EXTENSION( vertex_buffer_object );
    CHECK_EXTENSION( vertex_array_object );
    CHECK_EXTENSION( framebuffer_object );
    # undef CHECK_EXTENSION

    // States
    GL( glViewport( 0, 0, modeWidth, modeHeight ) );
    GL( glMatrixMode( GL_PROJECTION ) );
    GL( glLoadIdentity() );
    GL( gluOrtho2D( 0, modeWidth, modeHeight, 0 ) );
    GL( glGetFloatv( GL_PROJECTION_MATRIX, projectionMatrix ) );
    GL( glClearColor( 0.0f, 0.0f, 0.0f, 1.0f ) );
    if( !GameOpt.VSync )
        wglSwapIntervalEXT( 0 );
    #endif

    #ifdef FO_D3D
    // Contours
    if( deviceCaps.PixelShaderVersion >= D3DPS_VERSION( 2, 0 ) )
    {
        // Contours shader
        ID3DXBuffer* shader = NULL, * errors = NULL, * errors31 = NULL;
        HRESULT      hr = 0, hr31 = 0;
        hr = D3DXCompileShaderFromResource( NULL, MAKEINTRESOURCE( IDR_PS_CONTOUR ), NULL, NULL, "Main", "ps_2_0", D3DXSHADER_SKIPVALIDATION, &shader, &errors, &contoursCT );
        if( FAILED( hr ) )
            hr31 = D3DXCompileShaderFromResource( NULL, MAKEINTRESOURCE( IDR_PS_CONTOUR ), NULL, NULL, "Main", "ps_2_0", D3DXSHADER_SKIPVALIDATION | D3DXSHADER_USE_LEGACY_D3DX9_31_DLL, &shader, &errors31, &contoursCT );
        if( SUCCEEDED( hr ) || SUCCEEDED( hr31 ) )
        {
            hr = d3dDevice->CreatePixelShader( (DWORD*) shader->GetBufferPointer(), &contoursPS );
            shader->Release();
            if( FAILED( hr ) )
            {
                WriteLogF( _FUNC_, " - Can't create contours pixel shader, error<%s>. Used old style contours.\n", DXGetErrorString( hr ) );
                contoursPS = NULL;
            }
        }
        else
        {
            WriteLogF( _FUNC_, " - Shader 2d contours compilation fail, errors<%s\n%s>, legacy compiler errors<%s\n%s>. Used old style contours.\n",
                       DXGetErrorString( hr ), errors ? errors->GetBufferPointer() : "", DXGetErrorString( hr31 ), errors31 ? errors31->GetBufferPointer() : "" );
        }
        SAFEREL( errors );
        SAFEREL( errors31 );

        if( contoursPS )
        {
            contoursConstWidthStep = contoursCT->GetConstantByName( NULL, "WidthStep" );
            contoursConstHeightStep = contoursCT->GetConstantByName( NULL, "HeightStep" );
            contoursConstSpriteBorders = contoursCT->GetConstantByName( NULL, "SpriteBorders" );
            contoursConstSpriteBordersHeight = contoursCT->GetConstantByName( NULL, "SpriteBordersHeight" );
            contoursConstContourColor = contoursCT->GetConstantByName( NULL, "ContourColor" );
            contoursConstContourColorOffs = contoursCT->GetConstantByName( NULL, "ContourColorOffs" );
        }
    }
    #endif

    if( !Animation3d::StartUp( d3dDevice, GameOpt.SoftwareSkinning ) )
        return false;
    if( !InitRenderStates() )
        return false;
    if( !InitBuffers() )
        return false;

    // Sprites buffer
    sprData.resize( SPR_BUFFER_COUNT );
    for( auto it = sprData.begin(), end = sprData.end(); it != end; ++it )
        ( *it ) = NULL;

    // Transparent egg
    isInit = true;
    eggValid = false;
    sprEgg = NULL;
    eggHx = eggHy = eggX = eggY = 0;

    AnyFrames* egg_spr = LoadAnimation( "egg.png", PT_ART_MISC );
    if( egg_spr )
    {
        sprEgg = GetSpriteInfo( egg_spr->Ind[ 0 ] );
        delete egg_spr;
        eggSurfWidth = (float) surfList[ 0 ]->Width;    // First added surface
        eggSurfHeight = (float) surfList[ 0 ]->Height;
        eggSprWidth = sprEgg->Width;
        eggSprHeight = sprEgg->Height;
    }
    else
    {
        WriteLog( "Load sprite 'egg.png' fail. Egg disabled.\n" );
    }

    // Default effects
    curDefaultEffect = GraphicLoader::LoadEffect( d3dDevice, "2D_Default.fx", true );
    sprDefaultEffect[ DEFAULT_EFFECT_GENERIC ] = curDefaultEffect;
    sprDefaultEffect[ DEFAULT_EFFECT_TILE ] = curDefaultEffect;
    sprDefaultEffect[ DEFAULT_EFFECT_ROOF ] = curDefaultEffect;
    sprDefaultEffect[ DEFAULT_EFFECT_IFACE ] = GraphicLoader::LoadEffect( d3dDevice, "Interface_Default.fx", true );
    sprDefaultEffect[ DEFAULT_EFFECT_POINT ] = GraphicLoader::LoadEffect( d3dDevice, "Primitive_Default.fx", true );
    #ifndef FO_D3D
    sprDefaultEffect[ DEFAULT_EFFECT_CONTOUR ] = GraphicLoader::LoadEffect( d3dDevice, "Contour_Default.fx", true );
    sprDefaultEffect[ DEFAULT_EFFECT_TILE ] = GraphicLoader::LoadEffect( d3dDevice, "Tile_Default.fx", true );
    sprDefaultEffect[ DEFAULT_EFFECT_ROOF ] = GraphicLoader::LoadEffect( d3dDevice, "Tile_Default.fx", true );
    sprDefaultEffect[ DEFAULT_EFFECT_FLUSH_TEXTURE ] = GraphicLoader::LoadEffect( d3dDevice, "Flush_Texture.fx", true );
    sprDefaultEffect[ DEFAULT_EFFECT_FLUSH_TEXTURE_MS ] = NULL;
    sprDefaultEffect[ DEFAULT_EFFECT_FLUSH_COLOR ] = GraphicLoader::LoadEffect( d3dDevice, "Flush_Color.fx", true );
    sprDefaultEffect[ DEFAULT_EFFECT_FLUSH_FINAL ] = GraphicLoader::LoadEffect( d3dDevice, "Flush_Final.fx", true );
    for( uint i = 0; i < DEFAULT_EFFECT_COUNT; i++ )
    {
        if( !sprDefaultEffect[ i ] && i != DEFAULT_EFFECT_FLUSH_TEXTURE_MS )
        {
            WriteLog( "Default effects not loaded.\n" );
            return false;
        }
    }

    // Render targets
    if( !CreateRenderTarget( rtMain, true ) ||
        !CreateRenderTarget( rtContours, false ) ||
        !CreateRenderTarget( rtContoursMid, false ) ||
        !CreateRenderTarget( rt3D, true ) )
    {
        WriteLog( "Can't create render targets.\n" );
        return false;
    }
    CreateRenderTarget( rt3DMS, true, true );
    rtMain.DrawEffect = sprDefaultEffect[ DEFAULT_EFFECT_FLUSH_FINAL ];
    PushRenderTarget( rtMain );
    #endif

    // Clear scene
    #ifdef FO_D3D
    D3D_HR( d3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, COLOR_XRGB( 0, 0, 0 ), 1.0f, 0 ) );
    #else
    GL( glClear( GL_COLOR_BUFFER_BIT ) );
    SwapBuffers( dcScreen );
    #endif

    // Generate dummy animation
    if( !DummyAnimation )
    {
        DummyAnimation = CreateAnimation( 1, 100 );
        if( !DummyAnimation )
        {
            WriteLog( "Can't create dummy animation.\n" );
            return false;
        }
    }

    WriteLog( "Sprite manager initialization complete.\n" );
    return true;
}

bool SpriteManager::InitBuffers()
{
    #ifdef FO_D3D
    SAFEREL( vbMain );
    SAFEREL( ibMain );

    // Vertex buffer
    D3D_HR( d3dDevice->CreateVertexBuffer( flushSprCnt * 4 * sizeof( Vertex ), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, D3DFVF_MYVERTEX, D3DPOOL_DEFAULT, &vbMain, NULL ) );

    // Index buffer
    D3D_HR( d3dDevice->CreateIndexBuffer( flushSprCnt * 6 * sizeof( ushort ), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ibMain, NULL ) );

    ushort* ind = new ushort[ 6 * flushSprCnt ];
    if( !ind )
        return false;
    for( int i = 0; i < flushSprCnt; i++ )
    {
        ind[ 6 * i + 0 ] = 4 * i + 0;
        ind[ 6 * i + 1 ] = 4 * i + 1;
        ind[ 6 * i + 2 ] = 4 * i + 3;
        ind[ 6 * i + 3 ] = 4 * i + 1;
        ind[ 6 * i + 4 ] = 4 * i + 2;
        ind[ 6 * i + 5 ] = 4 * i + 3;
    }

    void* buf;
    D3D_HR( ibMain->Lock( 0, 0, (void**) &buf, 0 ) );
    memcpy( buf, ind, flushSprCnt * 6 * sizeof( ushort ) );
    D3D_HR( ibMain->Unlock() );
    delete[] ind;

    vBuffer.resize( flushSprCnt * 4 );
    #else
    if( vaMain )
        GL( glDeleteVertexArrays( 1, &vaMain ) );
    vaMain = 0;
    if( vbMain )
        GL( glDeleteBuffers( 1, &vbMain ) );
    vbMain = 0;
    if( ibMain )
        GL( glDeleteBuffers( 1, &ibMain ) );
    ibMain = 0;
    if( ibDirect )
        GL( glDeleteBuffers( 1, &ibDirect ) );
    ibDirect = 0;

    // Vertex buffer
    vBuffer.resize( flushSprCnt * 4 );
    GL( glGenBuffers( 1, &vbMain ) );
    GL( glBindBuffer( GL_ARRAY_BUFFER, vbMain ) );
    GL( glBufferData( GL_ARRAY_BUFFER, flushSprCnt * 4 * sizeof( Vertex ), NULL, GL_DYNAMIC_DRAW ) );

    // Index buffers
    ushort* ind = new ushort[ 6 * flushSprCnt ];
    for( int i = 0; i < flushSprCnt; i++ )
    {
        ind[ 6 * i + 0 ] = 4 * i + 0;
        ind[ 6 * i + 1 ] = 4 * i + 1;
        ind[ 6 * i + 2 ] = 4 * i + 3;
        ind[ 6 * i + 3 ] = 4 * i + 1;
        ind[ 6 * i + 4 ] = 4 * i + 2;
        ind[ 6 * i + 5 ] = 4 * i + 3;
    }
    GL( glGenBuffers( 1, &ibMain ) );
    GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibMain ) );
    GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, flushSprCnt * 6 * sizeof( ushort ), ind, GL_DYNAMIC_DRAW ) );
    for( int i = 0; i < flushSprCnt * 4; i++ )
        ind[ i ] = i;
    GL( glGenBuffers( 1, &ibDirect ) );
    GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibDirect ) );
    GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, flushSprCnt * 4 * sizeof( ushort ), ind, GL_DYNAMIC_DRAW ) );
    delete[] ind;

    // Vertex array
    GL( glGenVertexArrays( 1, &vaMain ) );
    GL( glBindVertexArray( vaMain ) );
    GL( glBindBuffer( GL_ARRAY_BUFFER, vbMain ) );
    GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibMain ) );
    GL( glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (void*) OFFSETOF( Vertex, x ) ) );
    GL( glVertexAttribPointer( 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( Vertex ), (void*) OFFSETOF( Vertex, diffuse ) ) );
    GL( glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (void*) OFFSETOF( Vertex, tu ) ) );
    GL( glVertexAttribPointer( 3, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (void*) OFFSETOF( Vertex, tu2 ) ) );
    GL( glEnableVertexAttribArray( 0 ) );
    GL( glEnableVertexAttribArray( 1 ) );
    GL( glEnableVertexAttribArray( 2 ) );
    GL( glEnableVertexAttribArray( 3 ) );
    #endif

    #ifdef FO_D3D
    SAFEREL( spr3dRT );
    SAFEREL( spr3dRTEx );
    SAFEREL( spr3dDS );
    SAFEREL( spr3dRTData );
    SAFEDEL( contoursTexture );
    SAFEREL( contoursTextureSurf );
    SAFEDEL( contoursMidTexture );
    SAFEREL( contoursMidTextureSurf );
    SAFEREL( contours3dRT );

    // Contours
    if( contoursPS )
    {
        // Contours render target
        D3D_HR( direct3D->CheckDepthStencilMatch( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_D24S8 ) );
        contoursTexture = new Texture();
        D3D_HR( D3DXCreateTexture( d3dDevice, modeWidth, modeHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &contoursTexture->Instance ) );
        contoursMidTexture = new Texture();
        D3D_HR( contoursTexture->Instance->GetSurfaceLevel( 0, &contoursTextureSurf ) );
        D3D_HR( D3DXCreateTexture( d3dDevice, modeWidth, modeHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &contoursMidTexture->Instance ) );
        D3D_HR( contoursMidTexture->Instance->GetSurfaceLevel( 0, &contoursMidTextureSurf ) );
        D3D_HR( d3dDevice->CreateRenderTarget( modeWidth, modeHeight, D3DFMT_A8R8G8B8, presentParams.MultiSampleType, presentParams.MultiSampleQuality, FALSE, &contours3dRT, NULL ) );
    }

    // 3d models prerendering
    D3D_HR( d3dDevice->CreateRenderTarget( spr3dSurfWidth, spr3dSurfHeight, D3DFMT_A8R8G8B8, presentParams.MultiSampleType, presentParams.MultiSampleQuality, FALSE, &spr3dRT, NULL ) );
    if( presentParams.MultiSampleType != D3DMULTISAMPLE_NONE )
        D3D_HR( d3dDevice->CreateRenderTarget( spr3dSurfWidth, spr3dSurfHeight, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &spr3dRTEx, NULL ) );
    D3D_HR( d3dDevice->CreateDepthStencilSurface( spr3dSurfWidth, spr3dSurfHeight, D3DFMT_D24S8, presentParams.MultiSampleType, presentParams.MultiSampleQuality, TRUE, &spr3dDS, NULL ) );
    D3D_HR( d3dDevice->CreateOffscreenPlainSurface( spr3dSurfWidth, spr3dSurfHeight, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &spr3dRTData, NULL ) );
    #endif

    return true;
}

bool SpriteManager::InitRenderStates()
{
    #ifdef FO_D3D
    D3D_HR( d3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE ) );
    D3D_HR( d3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE ) );
    D3D_HR( d3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESS ) );
    D3D_HR( d3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE ) );
    D3D_HR( d3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE ) );
    D3D_HR( d3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE ) );
    D3D_HR( d3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA ) );
    D3D_HR( d3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA ) );

    if( deviceCaps.AlphaCmpCaps & D3DPCMPCAPS_GREATEREQUAL )
    {
        D3D_HR( d3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE ) );
        D3D_HR( d3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL ) );
        D3D_HR( d3dDevice->SetRenderState( D3DRS_ALPHAREF, 1 ) );
    }

    D3D_HR( d3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE ) );
    D3D_HR( d3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR ) ); // Zoom Out
    D3D_HR( d3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR ) ); // Zoom In

    D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,  D3DTOP_MODULATE2X ) );
    D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE ) );
    D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE ) );
    D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE ) );
    D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE ) );
    D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE ) );
    D3D_HR( d3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE ) );
    D3D_HR( d3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE ) );
    D3D_HR( d3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_CURRENT ) );
    D3D_HR( d3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_MODULATE ) );
    D3D_HR( d3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE ) );
    D3D_HR( d3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT ) );

    D3D_HR( d3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE ) );
    D3D_HR( d3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE ) );
    D3D_HR( d3dDevice->SetRenderState( D3DRS_SPECULARENABLE, FALSE ) );
    // D3D_HR(d3dDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CCW));
    D3D_HR( d3dDevice->SetRenderState( D3DRS_AMBIENT, COLOR_XRGB( 80, 80, 80 ) ) );
    D3D_HR( d3dDevice->SetRenderState( D3DRS_NORMALIZENORMALS, TRUE ) );
    #else
    GL( glEnable( GL_TEXTURE_2D ) );
    GL( glEnable( GL_BLEND ) );
    GL( glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ) );
    GL( glEnable( GL_ALPHA_TEST ) );
    GL( glAlphaFunc( GL_GEQUAL, 1.0f / 255.0f ) );

    GL( glShadeModel( GL_SMOOTH ) );
    GL( glEnable( GL_POINT_SMOOTH ) );
    GL( glEnable( GL_LINE_SMOOTH ) );
    // GL( glCullFace( GL_FRONT ) );
    // GL( glFrontFace( GL_CW ) );
    GL( glDisable( GL_CULL_FACE ) );
    // GL( glPolygonMode( GL_FRONT_AND_BACK, GL_FILL ) );
    // GL( glReadBuffer( GL_BACK ) );
    // GL( glDrawBuffer( GL_BACK ) );
    // glEnable(GL_DEPTH_TEST);
    // glDepthFunc(GL_LEQUAL);
    // glDepthMask(TRUE);
    // glDisable(GL_STENCIL_TEST);
    // glStencilMask(0xFFFFFFFF);
    // glStencilFunc(GL_EQUAL, 0x00000000, 0x00000001);
    // glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    // glFrontFace(GL_CCW);

    // glClearColor(1.0, 0.0, 0.0, 0.0);
    // glClearDepth(1.0);
    // glClearStencil(0);
    // glDisable(GL_BLEND);
    // glDisable(GL_ALPHA_TEST);
    // glDisable(GL_DITHER);
    // glActiveTexture( GL_TEXTURE0 );
    #endif

    return true;
}

void SpriteManager::Finish()
{
    WriteLog( "Sprite manager finish...\n" );

    for( auto it = surfList.begin(), end = surfList.end(); it != end; ++it )
        SAFEDEL( *it );
    surfList.clear();
    for( auto it = sprData.begin(), end = sprData.end(); it != end; ++it )
        SAFEDEL( *it );
    sprData.clear();
    dipQueue.clear();

    Animation3d::Finish();

    #ifdef FO_D3D
    SAFEREL( spr3dRT );
    SAFEREL( spr3dRTEx );
    SAFEREL( spr3dDS );
    SAFEREL( spr3dRTData );
    SAFEREL( vbMain );
    SAFEREL( ibMain );
    SAFEREL( vbPoints );
    SAFEREL( d3dDevice );
    SAFEREL( contoursTextureSurf );
    SAFEDEL( contoursTexture );
    SAFEREL( contoursMidTextureSurf );
    SAFEDEL( contoursMidTexture );
    SAFEREL( contours3dRT );
    SAFEREL( contoursCT );
    SAFEREL( contoursPS );
    SAFEREL( direct3D );
    #endif

    isInit = false;
    WriteLog( "Sprite manager finish complete.\n" );
}

bool SpriteManager::BeginScene( uint clear_color )
{
    #ifdef FO_D3D
    HRESULT hr = d3dDevice->TestCooperativeLevel();
    if( hr != D3D_OK && ( hr != D3DERR_DEVICENOTRESET || !Restore() ) )
        return false;

    if( clear_color )
        D3D_HR( d3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, clear_color, 1.0f, 0 ) );
    D3D_HR( d3dDevice->BeginScene() );
    #else
    if( clear_color )
        ClearRenderTarget( rtMain, clear_color );
    #endif

    Animation3d::BeginScene();
    sceneBeginned = true;
    return true;
}

void SpriteManager::EndScene()
{
    Flush();
    #ifdef FO_D3D
    d3dDevice->EndScene();
    d3dDevice->Present( NULL, NULL, NULL, NULL );
    #else
    GL( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
    DrawRenderTarget( rtMain );
    GL( glBindFramebuffer( GL_FRAMEBUFFER, rtMain.FBO ) );
    SwapBuffers( dcScreen );
    #endif
    sceneBeginned = false;
}

bool SpriteManager::Restore()
{
    if( !isInit )
        return false;

    // Release resources
    #ifdef FO_D3D
    SAFEREL( spr3dRT );
    SAFEREL( spr3dRTEx );
    SAFEREL( spr3dDS );
    SAFEREL( spr3dRTData );
    SAFEREL( vbMain );
    SAFEREL( ibMain );
    SAFEREL( vbPoints );
    SAFEDEL( contoursTexture );
    SAFEREL( contoursTextureSurf );
    SAFEDEL( contoursMidTexture );
    SAFEREL( contoursMidTextureSurf );
    SAFEREL( contours3dRT );
    #endif
    Animation3d::PreRestore();
    if( PreRestore )
        ( *PreRestore )( );
    GraphicLoader::EffectsPreRestore();

    // Reset device
    #ifdef FO_D3D
    D3D_HR( d3dDevice->Reset( &presentParams ) );
    D3D_HR( d3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, COLOR_XRGB( 0, 0, 0 ), 1.0f, 0 ) );
    #endif

    // Create resources
    GraphicLoader::EffectsPostRestore();
    if( !InitRenderStates() )
        return false;
    if( !InitBuffers() )
        return false;
    if( PostRestore )
        ( *PostRestore )( );
    if( !Animation3d::StartUp( d3dDevice, GameOpt.SoftwareSkinning ) )
        return false;

    return true;
}

#ifndef FO_D3D
bool SpriteManager::CreateRenderTarget( RenderTarget& rt, bool depth_stencil, bool multisampling /* = false */, uint width /* = 0 */, uint height /* = 0 */ )
{
    // Zero data
    memzero( &rt, sizeof( rt ) );
    width = ( width ? width : modeWidth );
    height = ( height ? height : modeHeight );

    // Multisampling
    static int samples = -1;
    if( multisampling && samples == -1 )
    {
        if( GLEW_ARB_texture_multisample && GameOpt.MultiSampling != 0 )
        {
            // Samples count
            GLint max_samples = 0;
            GL( glGetIntegerv( GL_MAX_SAMPLES, &max_samples ) );
            if( GameOpt.MultiSampling < 0 )
                GameOpt.MultiSampling = 4;
            samples = min( GameOpt.MultiSampling, max_samples );

            // Flush effect
            sprDefaultEffect[ DEFAULT_EFFECT_FLUSH_TEXTURE_MS ] = GraphicLoader::LoadEffect( d3dDevice, "Flush_TextureMS.fx", true );
            if( !sprDefaultEffect[ DEFAULT_EFFECT_FLUSH_TEXTURE_MS ] )
                samples = 0;
        }
        else
        {
            samples = 0;
        }
    }
    if( multisampling && samples <= 1 )
        return false;

    // Framebuffer
    GL( glGenFramebuffers( 1, &rt.FBO ) );
    GL( glBindFramebuffer( GL_FRAMEBUFFER, rt.FBO ) );

    Texture* tex = new Texture();
    tex->Data = NULL;
    tex->Size = width * height * 4;
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
        GL( glTexImage2D( GL_TEXTURE_2D, 0, 4, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, tex->Data ) );
        GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST ) );
        GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST ) );
        GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP ) );
        GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP ) );
        GL( glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex->Id, 0 ) );
    }
    else
    {
        tex->Samples = (float) samples;
        GL( glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, tex->Id ) );
        GL( glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, samples, 4, width, height, TRUE ) );
        GL( glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, tex->Id, 0 ) );
    }
    rt.TargetTexture = tex;

    if( depth_stencil )
    {
        GL( glGenRenderbuffers( 1, &rt.DepthStencilBuffer ) );
        GL( glBindRenderbuffer( GL_RENDERBUFFER, rt.DepthStencilBuffer ) );
        if( !multisampling )
        {
            GL( glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height ) );
        }
        else
        {
            GL( glRenderbufferStorageMultisample( GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height ) );
        }
        GL( glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt.DepthStencilBuffer ) );
    }

    GLenum status;
    GL( status = glCheckFramebufferStatus( GL_FRAMEBUFFER ) );
    if( status != GL_FRAMEBUFFER_COMPLETE )
        WriteLogF( _FUNC_, "Framebuffer not created.\n" );

    if( !multisampling )
        rt.DrawEffect = sprDefaultEffect[ DEFAULT_EFFECT_FLUSH_TEXTURE ];
    else
        rt.DrawEffect = sprDefaultEffect[ DEFAULT_EFFECT_FLUSH_TEXTURE_MS ];

    GL( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );
    ClearRenderTarget( rt, 0 );
    if( depth_stencil )
        ClearRenderTargetDS( rt, true, true );
    return true;
}

void SpriteManager::DeleteRenderTarget( RenderTarget& rt )
{
    if( rt.DepthStencilBuffer )
        GL( glDeleteRenderbuffers( 1, &rt.DepthStencilBuffer ) );
    GL( glDeleteFramebuffers( 1, &rt.FBO ) );
    SAFEDEL( rt.TargetTexture );
    memzero( &rt, sizeof( rt ) );
}

void SpriteManager::ClearRenderTarget( RenderTarget& rt, uint color )
{
    GL( glBindFramebuffer( GL_FRAMEBUFFER, rt.FBO ) );
    float a = (float) ( ( color >> 24 ) & 0xFF ) / 255.0f;
    float r = (float) ( ( color >> 16 ) & 0xFF ) / 255.0f;
    float g = (float) ( ( color >>  8 ) & 0xFF ) / 255.0f;
    float b = (float) ( ( color >>  0 ) & 0xFF ) / 255.0f;
    GL( glClearColor( r, g, b, a ) );
    GL( glClear( GL_COLOR_BUFFER_BIT ) );
    GL( glBindFramebuffer( GL_FRAMEBUFFER, rtStack.empty() ? 0 : rtStack.back()->FBO ) );
}

void SpriteManager::ClearRenderTargetDS( RenderTarget& rt, bool depth, bool stencil )
{
    GL( glBindFramebuffer( GL_FRAMEBUFFER, rt.FBO ) );
    int depth_stencil_bit = 0;
    depth_stencil_bit |= ( depth ? GL_DEPTH_BUFFER_BIT : 0 );
    depth_stencil_bit |= ( stencil ? GL_STENCIL_BUFFER_BIT : 0 );
    if( depth )
    {
        depth_stencil_bit |= GL_DEPTH_BUFFER_BIT;
        GL( glClearDepth( 1.0 ) );
    }
    if( stencil )
    {
        depth_stencil_bit |= GL_STENCIL_BUFFER_BIT;
        GL( glClearStencil( 0 ) );
    }
    GL( glClear( depth_stencil_bit ) );
    GL( glBindFramebuffer( GL_FRAMEBUFFER, rtStack.empty() ? 0 : rtStack.back()->FBO ) );
}

void SpriteManager::PushRenderTarget( RenderTarget& rt )
{
    Flush();
    rtStack.push_back( &rt );
    GL( glBindFramebuffer( GL_FRAMEBUFFER, rt.FBO ) );
}

void SpriteManager::PopRenderTarget()
{
    Flush();
    rtStack.pop_back();
    GL( glBindFramebuffer( GL_FRAMEBUFFER, rtStack.empty() ? 0 : rtStack.back()->FBO ) );
}

void SpriteManager::DrawRenderTarget( RenderTarget& rt, Rect* region /* = NULL */ )
{
    Flush();

    if( !region )
    {
        uint  mulpos = 0;
        float w = (float) rt.TargetTexture->Width;
        float h = (float) rt.TargetTexture->Height;

        vBuffer[ mulpos ].x = 0.0f;
        vBuffer[ mulpos ].y = h;
        vBuffer[ mulpos ].tu = 0.0f;
        vBuffer[ mulpos++ ].tv = 0.0f;
        vBuffer[ mulpos ].x = 0.0f;
        vBuffer[ mulpos ].y = 0.0f;
        vBuffer[ mulpos ].tu = 0.0f;
        vBuffer[ mulpos++ ].tv = 1.0f;
        vBuffer[ mulpos ].x = w;
        vBuffer[ mulpos ].y = 0.0f;
        vBuffer[ mulpos ].tu = 1.0f;
        vBuffer[ mulpos++ ].tv = 1.0f;
        vBuffer[ mulpos ].x = w;
        vBuffer[ mulpos ].y = h;
        vBuffer[ mulpos ].tu = 1.0f;
        vBuffer[ mulpos++ ].tv = 0.0f;
    }
    else
    {
        uint  mulpos = 0;
        RectF regionf;
        regionf = *region;
        float w = (float) rt.TargetTexture->Width;
        float h = (float) rt.TargetTexture->Height;

        vBuffer[ mulpos ].x = regionf.L;
        vBuffer[ mulpos ].y = regionf.B;
        vBuffer[ mulpos ].tu = regionf.L / w;
        vBuffer[ mulpos++ ].tv = 1.0f - regionf.B / h;
        vBuffer[ mulpos ].x = regionf.L;
        vBuffer[ mulpos ].y = regionf.T;
        vBuffer[ mulpos ].tu = regionf.L / w;
        vBuffer[ mulpos++ ].tv = 1.0f - regionf.T / h;
        vBuffer[ mulpos ].x = regionf.R;
        vBuffer[ mulpos ].y = regionf.T;
        vBuffer[ mulpos ].tu = regionf.R / w;
        vBuffer[ mulpos++ ].tv = 1.0f - regionf.T / h;
        vBuffer[ mulpos ].x = regionf.R;
        vBuffer[ mulpos ].y = regionf.B;
        vBuffer[ mulpos ].tu = regionf.R / w;
        vBuffer[ mulpos++ ].tv = 1.0f - regionf.B / h;
    }

    curSprCnt = 1;
    dipQueue.push_back( DipData( rt.TargetTexture, rt.DrawEffect ) );
    Flush();
}
#endif

Surface* SpriteManager::CreateNewSurface( int w, int h )
{
    if( !isInit )
        return NULL;

    // Check power of two
    int ww = w + SURF_SPRITES_OFFS * 2;
    w = baseTextureSize;
    int hh = h + SURF_SPRITES_OFFS * 2;
    h = baseTextureSize;
    while( w < ww )
        w *= 2;
    while( h < hh )
        h *= 2;

    Texture* tex = new Texture();
    #ifdef FO_D3D
    D3D_HR( d3dDevice->CreateTexture( w, h, 1, 0, TEX_FRMT, D3DPOOL_MANAGED, &tex->Instance, NULL ) );
    #else
    tex->Data = new uchar[ w * h * 4 ];
    tex->Size = w * h * 4;
    tex->Width = w;
    tex->Height = h;
    tex->SizeData[ 0 ] = (float) w;
    tex->SizeData[ 1 ] = (float) h;
    tex->SizeData[ 2 ] = 1.0f / tex->SizeData[ 0 ];
    tex->SizeData[ 3 ] = 1.0f / tex->SizeData[ 1 ];
    GL( glGenTextures( 1, &tex->Id ) );
    GL( glBindTexture( GL_TEXTURE_2D, tex->Id ) );
    GL( glPixelStorei( GL_UNPACK_ALIGNMENT, 1 ) );
    GL( glTexImage2D( GL_TEXTURE_2D, 0, 4, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, tex->Data ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, SurfFilterNearest ? GL_NEAREST : GL_LINEAR ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, SurfFilterNearest ? GL_NEAREST : GL_LINEAR ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP ) );
    GL( glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP ) );
    #endif

    Surface* surf = new Surface();
    surf->Type = SurfType;
    surf->TextureOwner = tex;
    surf->Width = w;
    surf->Height = h;
    surf->BusyH = SURF_SPRITES_OFFS;
    surf->FreeX = SURF_SPRITES_OFFS;
    surf->FreeY = SURF_SPRITES_OFFS;
    surfList.push_back( surf );
    return surf;
}

Surface* SpriteManager::FindSurfacePlace( SpriteInfo* si, int& x, int& y )
{
    // Find place in already created surface
    uint w = si->Width + SURF_SPRITES_OFFS * 2;
    uint h = si->Height + SURF_SPRITES_OFFS * 2;
    for( auto it = surfList.begin(), end = surfList.end(); it != end; ++it )
    {
        Surface* surf = *it;
        if( surf->Type == SurfType )
        {
            if( surf->Width - surf->FreeX >= w && surf->Height - surf->FreeY >= h )
            {
                x = surf->FreeX + SURF_SPRITES_OFFS;
                y = surf->FreeY;
                return surf;
            }
            else if( surf->Width >= w && surf->Height - surf->BusyH >= h )
            {
                x = SURF_SPRITES_OFFS;
                y = surf->BusyH + SURF_SPRITES_OFFS;
                return surf;
            }
        }
    }

    // Create new
    Surface* surf = CreateNewSurface( si->Width, si->Height );
    if( !surf )
        return NULL;
    x = surf->FreeX;
    y = surf->FreeY;
    return surf;
}

void SpriteManager::FreeSurfaces( int surf_type )
{
    for( auto it = surfList.begin(); it != surfList.end();)
    {
        Surface* surf = *it;
        if( surf->Type == surf_type )
        {
            for( auto it_ = sprData.begin(), end_ = sprData.end(); it_ != end_; ++it_ )
            {
                SpriteInfo* si = *it_;
                if( si && si->Surf == surf )
                {
                    delete si;
                    ( *it_ ) = NULL;
                }
            }

            delete surf;
            it = surfList.erase( it );
        }
        else
            ++it;
    }
}

void SpriteManager::SaveSufaces()
{
    uint surf_size = 0;
    for( auto it = surfList.begin(), end = surfList.end(); it != end; ++it )
    {
        Surface* surf = *it;
        surf_size += surf->Width * surf->Height * 4;
    }

    char path[ MAX_FOPATH ];
    Str::Format( path, DIR_SLASH_SD "%u_%u.%03umb" DIR_SLASH_S, (uint) time( NULL ), surf_size / 1000000, surf_size % 1000000 / 1000 );
    FileManager::CreateDirectoryTree( path );

    #ifdef FO_D3D
    int  cnt = 0;
    char name[ MAX_FOPATH ];
    for( auto it = surfList.begin(), end = surfList.end(); it != end; ++it )
    {
        Surface*           surf = *it;
        LPDIRECT3DSURFACE9 s;
        surf->TextureOwner->Instance->GetSurfaceLevel( 0, &s );
        Str::Format( name, "%s%d_%d_%ux%u.png", path, surf->Type, cnt, surf->Width, surf->Height );
        D3DXSaveSurfaceToFile( name, D3DXIFF_PNG, s, NULL, NULL );
        s->Release();
        cnt++;
    }
    #else
    int  cnt = 0;
    char name[ MAX_FOPATH ];
    for( auto it = surfList.begin(), end = surfList.end(); it != end; ++it )
    {
        Surface* surf = *it;
        Texture* tex = surf->TextureOwner;
        ILuint   img = 0;
        ilGenImages( 1, &img );
        ilBindImage( img );
        ilTexImage( tex->Width, tex->Height, 1, 4, IL_BGRA, IL_UNSIGNED_BYTE, tex->Data );
        uint   size = ilSaveL( IL_PNG, NULL, 0 );
        uchar* buf = new uchar[ size ];
        ilSaveL( IL_PNG, buf, size );
        ilDeleteImages( 1, &img );
        Str::Format( name, "%s%d_%d_%ux%u.png", path, surf->Type, cnt, surf->Width, surf->Height );
        void* f = FileOpen( name, true );
        FileWrite( f, buf, size );
        FileClose( f );
        delete[] buf;
        cnt++;
    }
    #endif
}

uint SpriteManager::FillSurfaceFromMemory( SpriteInfo* si, void* data, uint size )
{
    // Parameters
    uint w, h;
    if( !si )
        si = new (nothrow) SpriteInfo();

    // Get width, height
    w = *( (uint*) data + 1 );
    h = *( (uint*) data + 2 );

    // Find place on surface
    si->Width = w;
    si->Height = h;
    int      x, y;
    Surface* surf = FindSurfacePlace( si, x, y );
    if( !surf )
    {
        delete[] data;
        return 0;
    }

    #ifdef FO_D3D
    LPDIRECT3DSURFACE9 dst_surf;
    LockRect_          rdst;
    D3D_HR( surf->TextureOwner->Instance->GetSurfaceLevel( 0, &dst_surf ) );

    // Copy
    RECT   r = { x - 1, y - 1, x + w + 1, y + h + 1 };
    D3D_HR( dst_surf->LockRect( &rdst, &r, 0 ) );
    uchar* ptr = (uchar*) ( (uint*) data + 3 );
    for( uint i = 0; i < h; i++ )
        memcpy( (uchar*) rdst.pBits + rdst.Pitch * ( i + 1 ) + 4, ptr + w * 4 * i, w * 4 );

    if( GameOpt.DebugSprites )
    {
        uint rnd_color = COLOR_XRGB( Random( 0, 255 ), Random( 0, 255 ), Random( 0, 255 ) );
        for( uint yy = 1; yy < h + 1; yy++ )
        {
            for( uint xx = 1; xx < w + 1; xx++ )
            {
                uint& p = SURF_POINT( rdst, xx, yy );
                if( p && ( !SURF_POINT( rdst, xx - 1, yy - 1 ) || !SURF_POINT( rdst, xx, yy - 1 ) || !SURF_POINT( rdst, xx + 1, yy - 1 ) ||
                           !SURF_POINT( rdst, xx - 1, yy ) || !SURF_POINT( rdst, xx + 1, yy ) || !SURF_POINT( rdst, xx - 1, yy + 1 ) ||
                           !SURF_POINT( rdst, xx, yy + 1 ) || !SURF_POINT( rdst, xx + 1, yy + 1 ) ) )
                    p = rnd_color;
            }
        }
    }

    for( uint i = 0; i < h + 2; i++ )
        SURF_POINT( rdst, 0, i ) = SURF_POINT( rdst, 1, i );               // Left
    for( uint i = 0; i < h + 2; i++ )
        SURF_POINT( rdst, w + 1, i ) = SURF_POINT( rdst, w, i );           // Right
    for( uint i = 0; i < w + 2; i++ )
        SURF_POINT( rdst, i, 0 ) = SURF_POINT( rdst, i, 1 );               // Top
    for( uint i = 0; i < w + 2; i++ )
        SURF_POINT( rdst, i, h + 1 ) = SURF_POINT( rdst, i, h );           // Bottom

    D3D_HR( dst_surf->UnlockRect() );
    dst_surf->Release();
    #else
    Texture* tex = surf->TextureOwner;

    uint*    ptr_from = (uint*) data + 3;
    uint*    ptr_to = (uint*) tex->Data + ( y - 1 ) * tex->Width + x - 1;
    for( uint i = 0; i < h; i++ )
        memcpy( ptr_to + tex->Width * ( i + 1 ) + 1, ptr_from + w * i, w * 4 );

    if( GameOpt.DebugSprites )
    {
        uint rnd_color = COLOR_XRGB( Random( 0, 255 ), Random( 0, 255 ), Random( 0, 255 ) );
        for( uint yy = 1; yy < h + 1; yy++ )
        {
            for( uint xx = 1; xx < w + 1; xx++ )
            {
                uint  xxx = x + xx - 1;
                uint  yyy = y + yy - 1;
                uint& p = tex->Pixel( xxx, yyy );
                if( p && ( !tex->Pixel( xxx - 1, yyy - 1 ) || !tex->Pixel( xxx, yyy - 1 ) || !tex->Pixel( xxx + 1, yyy - 1 ) ||
                           !tex->Pixel( xxx - 1, yyy ) || !tex->Pixel( xxx + 1, yyy ) || !tex->Pixel( xxx - 1, yyy + 1 ) ||
                           !tex->Pixel( xxx, yyy + 1 ) || !tex->Pixel( xxx + 1, yyy + 1 ) ) )
                    p = rnd_color;
            }
        }
    }

    // 1px border for correct linear interpolation
    for( uint i = 0; i < h + 2; i++ )
        tex->Pixel( x + 0 - 1, y + i - 1 ) = tex->Pixel( x + 1 - 1, y + i - 1 );               // Left
    for( uint i = 0; i < h + 2; i++ )
        tex->Pixel( x + w + 1 - 1, y + i - 1 ) = tex->Pixel( x + w - 1, y + i - 1 );           // Right
    for( uint i = 0; i < w + 2; i++ )
        tex->Pixel( x + i - 1, y + 0 - 1 ) = tex->Pixel( x + i - 1, y + 1 - 1 );               // Top
    for( uint i = 0; i < w + 2; i++ )
        tex->Pixel( x + i - 1, y + h + 1 - 1 ) = tex->Pixel( x + i - 1, y + h - 1 );           // Bottom

    // Refresh texture
    tex->Update( Rect( x - 1, y - 1, x + w + 1, y + h + 1 ) );
    #endif

    // Delete data
    delete[] data;

    // Set parameters
    si->Surf = surf;
    surf->FreeX = x + w;
    surf->FreeY = y;
    if( y + h > surf->BusyH )
        surf->BusyH = y + h;
    si->SprRect.L = float(x) / float(surf->Width);
    si->SprRect.T = float(y) / float(surf->Height);
    si->SprRect.R = float(x + w) / float(surf->Width);
    si->SprRect.B = float(y + h) / float(surf->Height);

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

AnyFrames* SpriteManager::LoadAnimation( const char* fname, int path_type, int flags )
{
    AnyFrames* dummy = ( FLAG( flags, ANIM_USE_DUMMY ) ? DummyAnimation : NULL );

    if( !isInit || !fname || !fname[ 0 ] )
        return dummy;

    const char* ext = FileManager::GetExtension( fname );
    if( !ext )
    {
        WriteLogF( _FUNC_, " - Extension not found, file<%s>.\n", fname );
        return dummy;
    }

    int        dir = ( flags & 0xFF );

    AnyFrames* result = NULL;
    if( Str::CompareCaseCount( ext, "fr", 2 ) )
        result = LoadAnimationFrm( fname, path_type, dir, FLAG( flags, ANIM_FRM_ANIM_PIX ) );
    else if( Str::CompareCase( ext, "rix" ) )
        result = LoadAnimationRix( fname, path_type );
    else if( Str::CompareCase( ext, "fofrm" ) )
        result = LoadAnimationFofrm( fname, path_type, dir );
    else if( Str::CompareCase( ext, "art" ) )
        result = LoadAnimationArt( fname, path_type, dir );
    else if( Str::CompareCase( ext, "spr" ) )
        result = LoadAnimationSpr( fname, path_type, dir );
    else if( Str::CompareCase( ext, "zar" ) )
        result = LoadAnimationZar( fname, path_type );
    else if( Str::CompareCase( ext, "til" ) )
        result = LoadAnimationTil( fname, path_type );
    else if( Str::CompareCase( ext, "mos" ) )
        result = LoadAnimationMos( fname, path_type );
    else if( Str::CompareCase( ext, "bam" ) )
        result = LoadAnimationBam( fname, path_type );
    else if( GraphicLoader::IsExtensionSupported( ext ) )
        result = LoadAnimation3d( fname, path_type, dir );
    else
        result = LoadAnimationOther( fname, path_type );

    return result ? result : dummy;
}

AnyFrames* SpriteManager::ReloadAnimation( AnyFrames* anim, const char* fname, int path_type )
{
    if( !isInit )
        return anim;
    if( !fname || !fname[ 0 ] )
        return anim;

    // Release old images
    if( anim )
    {
        for( uint i = 0; i < anim->CntFrm; i++ )
        {
            SpriteInfo* si = GetSpriteInfo( anim->Ind[ i ] );
            if( !si )
                continue;

            for( auto it = surfList.begin(), end = surfList.end(); it != end; ++it )
            {
                Surface* surf = *it;
                if( si->Surf == surf )
                {
                    delete surf;
                    surfList.erase( it );
                    break;
                }
            }
            SAFEDEL( sprData[ anim->Ind[ i ] ] );
        }

        delete anim;
    }

    // Load fresh
    return LoadAnimation( fname, path_type );
}

AnyFrames* SpriteManager::CreateAnimation( uint frames, uint ticks )
{
    if( !frames || frames > 10000 )
        return NULL;

    AnyFrames* anim = new (nothrow) AnyFrames();
    if( !anim )
        return NULL;

    anim->Ind = new (nothrow) uint[ frames ];
    if( !anim->Ind )
        return NULL;
    memzero( anim->Ind, sizeof( uint ) * frames );
    anim->NextX = new ( nothrow ) short[ frames ];
    if( !anim->NextX )
        return NULL;
    memzero( anim->NextX, sizeof( short ) * frames );
    anim->NextY = new ( nothrow ) short[ frames ];
    if( !anim->NextY )
        return NULL;
    memzero( anim->NextY, sizeof( short ) * frames );

    anim->CntFrm = frames;
    anim->Ticks = ( ticks ? ticks : frames * 100 );
    return anim;
}

AnyFrames* SpriteManager::LoadAnimationFrm( const char* fname, int path_type, int dir, bool anim_pix )
{
    if( dir < 0 || dir >= DIRS_COUNT )
        return NULL;

    if( !GameOpt.MapHexagonal )
    {
        switch( dir )
        {
        case 0:
            dir = 0;
            break;
        case 1:
            dir = 1;
            break;
        case 2:
            dir = 2;
            break;
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
            dir = 5;
            break;
        case 7:
            dir = 5;
            break;
        default:
            dir = 0;
            break;
        }
    }

    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return NULL;

    fm.SetCurPos( 0x4 );
    ushort frm_fps = fm.GetBEUShort();
    if( !frm_fps )
        frm_fps = 10;
    fm.SetCurPos( 0x8 );
    ushort frm_num = fm.GetBEUShort();
    fm.SetCurPos( 0xA + dir * 2 );
    short  offs_x = fm.GetBEUShort();
    fm.SetCurPos( 0x16 + dir * 2 );
    short  offs_y = fm.GetBEUShort();

    fm.SetCurPos( 0x22 + dir * 4 );
    uint offset = 0x3E + fm.GetBEUInt();
    if( offset == 0x3E && dir )
        return NULL;

    AnyFrames* anim = CreateAnimation( frm_num, 1000 / frm_fps * frm_num );
    if( !anim )
        return NULL;

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
            palette_entry[ i ] = COLOR_XRGB( r, g, b );
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
        SpriteInfo* si = new (nothrow) SpriteInfo();      // TODO: Memory leak
        if( !si )
            return NULL;
        fm.SetCurPos( offset );
        ushort w = fm.GetBEUShort();
        ushort h = fm.GetBEUShort();

        fm.GoForward( 4 );       // Frame size

        si->OffsX = offs_x;
        si->OffsY = offs_y;

        anim->NextX[ frm ] = fm.GetBEUShort();
        anim->NextY[ frm ] = fm.GetBEUShort();

        // Data for FillSurfaceFromMemory
        uint   size = 12 + h * w * 4;
        uchar* data = new (nothrow) uchar[ size ];
        if( !data )
        {
            delete anim;
            return NULL;
        }
        *( (uint*) data + 1 ) = w;
        *( (uint*) data + 2 ) = h;
        uint* ptr = (uint*) data + 3;
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
                        *( ptr + i ) = COLOR_XRGB( blinking_red_vals[ frm % 10 ], 0, 0 );
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
                short nx = anim->NextX[ 0 ];
                short ny = anim->NextY[ 0 ];
                SAFEDELA( anim->Ind );
                SAFEDELA( anim->NextX );
                SAFEDELA( anim->NextY );
                anim->Ind = new (nothrow) uint[ frm_num ];
                if( !anim->Ind )
                    return NULL;
                anim->NextX = new ( nothrow ) short[ frm_num ];
                if( !anim->NextX )
                    return NULL;
                anim->NextY = new ( nothrow ) short[ frm_num ];
                if( !anim->NextY )
                    return NULL;
                anim->NextX[ 0 ] = nx;
                anim->NextY[ 0 ] = ny;
            }
        }

        if( !anim_pix_type )
            offset += w * h + 12;

        uint result = FillSurfaceFromMemory( si, data, size );
        if( !result )
        {
            delete anim;
            return NULL;
        }
        anim->Ind[ frm ] = result;
    }

    return anim;
}

AnyFrames* SpriteManager::LoadAnimationRix( const char* fname, int path_type )
{
    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return NULL;

    SpriteInfo* si = new (nothrow) SpriteInfo();
    fm.SetCurPos( 0x4 );
    ushort      w;
    fm.CopyMem( &w, 2 );
    ushort      h;
    fm.CopyMem( &h, 2 );

    // Data for FillSurfaceFromMemory
    uint   size = 12 + h * w * 4;
    uchar* data = new (nothrow) uchar[ size ];
    *( (uint*) data + 1 ) = w;
    *( (uint*) data + 2 ) = h;
    uint*  ptr = (uint*) data + 3;
    fm.SetCurPos( 0xA );
    uchar* palette = fm.GetCurBuf();
    fm.SetCurPos( 0xA + 256 * 3 );
    for( int i = 0, j = w * h; i < j; i++ )
    {
        uint index = fm.GetUChar();
        uint r = *( palette + index * 3 + 0 ) * 4;
        uint g = *( palette + index * 3 + 1 ) * 4;
        uint b = *( palette + index * 3 + 2 ) * 4;
        *( ptr + i ) = COLOR_XRGB( r, g, b );
    }

    uint result = FillSurfaceFromMemory( si, data, size );
    if( !result )
        return NULL;

    AnyFrames* anim = CreateAnimation( 1, 100 );
    if( !anim )
        return NULL;
    anim->Ind[ 0 ] = result;
    return anim;
}

AnyFrames* SpriteManager::LoadAnimationFofrm( const char* fname, int path_type, int dir )
{
    IniParser fofrm;
    if( !fofrm.LoadFile( fname, path_type ) )
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
            effect = GraphicLoader::LoadEffect( d3dDevice, effect_name, true );
    }

    char dir_str[ 16 ];
    sprintf( dir_str, "dir_%d", dir );
    bool no_app = ( dir == 0 && !fofrm.IsApp( "dir_0" ) );

    if( !no_app )
    {
        ox = fofrm.GetInt( dir_str, "offs_x", ox );
        oy = fofrm.GetInt( dir_str, "offs_y", oy );
    }

    char    frm_fname[ MAX_FOPATH ];
    FileManager::ExtractPath( fname, frm_fname );
    char*   frm_name = frm_fname + strlen( frm_fname );

    AnimVec anims;
    IntVec  anims_offs;
    anims.reserve( 50 );
    anims_offs.reserve( 100 );
    uint frames = 0;
    for( int frm = 0; frm < frm_num; frm++ )
    {
        anims_offs.push_back( fofrm.GetInt( no_app ? NULL : dir_str, Str::FormatBuf( "next_x_%d", frm ), 0 ) );
        anims_offs.push_back( fofrm.GetInt( no_app ? NULL : dir_str, Str::FormatBuf( "next_y_%d", frm ), 0 ) );

        if( !fofrm.GetStr( no_app ? NULL : dir_str, Str::FormatBuf( "frm_%d", frm ), "", frm_name ) &&
            ( frm != 0 || !fofrm.GetStr( no_app ? NULL : dir_str, Str::FormatBuf( "frm", frm ), "", frm_name ) ) )
            goto label_Fail;

        AnyFrames* anim = LoadAnimation( frm_fname, path_type, ANIM_DIR( dir ) );
        if( !anim )
            goto label_Fail;

        frames += anim->CntFrm;
        anims.push_back( anim );
    }

    // No frames found or error
    if( anims.empty() )
    {
label_Fail:
        for( uint i = 0, j = (uint) anims.size(); i < j; i++ )
            delete anims[ i ];
        return NULL;
    }

    // One animation
    if( anims.size() == 1 )
    {
        AnyFrames* anim = anims[ 0 ];
        anim->Ticks = 1000 / frm_fps * frm_num;

        SpriteInfo* si = GetSpriteInfo( anim->Ind[ 0 ] );
        si->OffsX += ox;
        si->OffsY += oy;
        si->DrawEffect = effect;

        return anim;
    }

    // Merge many animations in one
    AnyFrames* anim = CreateAnimation( frames, 1000 / frm_fps * frm_num );
    if( !anim )
        goto label_Fail;

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

        delete part;
    }

    return anim;
}

AnyFrames* SpriteManager::LoadAnimation3d( const char* fname, int path_type, int dir )
{
    Animation3d* anim3d = Animation3d::GetAnimation( fname, path_type, false );
    if( !anim3d )
        return NULL;

    // Get animation data
    float period;
    int   proc_from, proc_to;
    anim3d->GetRenderFramesData( period, proc_from, proc_to );

    // If not animations available than render just one
    if( period == 0.0f || proc_from == proc_to )
        goto label_LoadOneSpr;

    // Calculate need information
    float frame_time = 1.0f / (float) ( GameOpt.Animation3dFPS ? GameOpt.Animation3dFPS : 10 ); // 1 second / fps
    float period_from = period * (float) proc_from / 100.0f;
    float period_to = period * (float) proc_to / 100.0f;
    float period_len = abs( period_to - period_from );
    float proc_step = (float) ( proc_to - proc_from ) / ( period_len / frame_time );
    int   frames_count = (int) ceil( period_len / frame_time );

    if( frames_count <= 1 )
    {
label_LoadOneSpr:
        uint spr_id = Render3dSprite( anim3d, dir, proc_from * 10 );
        if( !spr_id )
            return NULL;
        AnyFrames* anim = CreateAnimation( 1, 100 );
        if( !anim )
            return NULL;
        anim->Ind[ 0 ] = spr_id;
        return anim;
    }

    AnyFrames* anim = CreateAnimation( frames_count, (uint) ( period_len * 1000.0f ) );
    if( !anim )
        return NULL;

    float cur_proc = (float) proc_from;
    int   prev_cur_proci = -1;
    for( int i = 0; i < frames_count; i++ )
    {
        int cur_proci = ( proc_to > proc_from ? (int) ( 10.0f * cur_proc + 0.5 ) : (int) ( 10.0f * cur_proc ) );

        if( cur_proci != prev_cur_proci ) // Previous frame is different
            anim->Ind[ i ] = Render3dSprite( anim3d, dir, cur_proci );
        else                              // Previous frame is same
            anim->Ind[ i ] = anim->Ind[ i - 1 ];

        if( !anim->Ind[ i ] )
            return NULL;

        cur_proc += proc_step;
        prev_cur_proci = cur_proci;
    }

    return anim;
}

AnyFrames* SpriteManager::LoadAnimationArt( const char* fname, int path_type, int dir )
{
    if( dir < 0 || dir >= DIRS_COUNT )
        return NULL;

    if( GameOpt.MapHexagonal )
    {
        switch( dir )
        {
        case 0:
            dir = 1;
            break;
        case 1:
            dir = 2;
            break;
        case 2:
            dir = 3;
            break;
        case 3:
            dir = 5;
            break;
        case 4:
            dir = 6;
            break;
        case 5:
            dir = 7;
            break;
        default:
            dir = 0;
            break;
        }
    }
    else
    {
        dir = ( dir + 1 ) % 8;
    }

    char file_name[ MAX_FOPATH ];
    int  palette_index = 0;  // 0..3
    bool transparent = false;
    bool mirror_hor = false;
    bool mirror_ver = false;
    uint frm_from = 0;
    uint frm_to = 100000;
    Str::Copy( file_name, fname );

    char* delim = strstr( file_name, "$" );
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
    if( header.rotationCount != 8 )
        dir = dir * ( header.rotationCount * 100 / 8 ) / 100;
    if( (uint) dir >= header.rotationCount )
        dir = 0;

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
    uint       frm_count_anim = max( frm_from, frm_to ) - min( frm_from, frm_to ) + 1;

    AnyFrames* anim = CreateAnimation( frm_count_anim, 1000 / frm_fps * frm_count_anim );
    if( !anim )
        return NULL;

    // Calculate data offset
    uint data_offset = sizeof( ArtHeader ) + sizeof( ArtPalette ) * palette_count + sizeof( ArtFrameInfo ) * dir_count * frm_count;
    for( uint i = 0; i < (uint) dir; i++ )
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
                          sizeof( ArtFrameInfo ) * dir * frm_count + sizeof( ArtFrameInfo ) * i + sizeof( uint ) * 2 );
            uint frame_size = fm.GetLEUInt();
            data_offset_cur += frame_size;
        }

        fm.SetCurPos( sizeof( ArtHeader ) + sizeof( ArtPalette ) * palette_count +
                      sizeof( ArtFrameInfo ) * dir * frm_count + sizeof( ArtFrameInfo ) * frm );

        SpriteInfo* si = new (nothrow) SpriteInfo();
        if( !si )
        {
            delete anim;
            return NULL;
        }

        if( !fm.CopyMem( &frame_info, sizeof( frame_info ) ) )
        {
            delete si;
            delete anim;
            return NULL;
        }

        si->OffsX = ( -frame_info.offsetX + frame_info.frameWidth / 2 ) * ( mirror_hor ? -1 : 1 );
        si->OffsY = ( -frame_info.offsetY + frame_info.frameHeight ) * ( mirror_ver ? -1 : 1 );
        anim->NextX[ frm_cur ] = 0;    // frame_info.deltaX;
        anim->NextY[ frm_cur ] = 0;    // frame_info.deltaY;

        // Data for FillSurfaceFromMemory
        uint   w = frame_info.frameWidth;
        uint   h = frame_info.frameHeight;
        uint   size = 12 + h * w * 4;
        uchar* data = new (nothrow) uchar[ size ];
        if( !data )
        {
            delete si;
            delete anim;
            return NULL;
        }
        *( (uint*) data + 1 ) = w;
        *( (uint*) data + 2 ) = h;
        uint* ptr = (uint*) data + 3;

        fm.SetCurPos( data_offset_cur );

        // Decode
// =======================================================================
        #define ART_GET_COLOR                                                                              \
            uchar index = fm.GetUChar();                                                                   \
            uint color = palette[ palette_index ][ index ];                                                \
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
        uint result = FillSurfaceFromMemory( si, data, size );
        if( !result )
        {
            delete anim;
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

    return anim;
}

#define SPR_CACHED_COUNT    ( 10 )
AnyFrames* SpriteManager::LoadAnimationSpr( const char* fname, int path_type, int dir )
{
    if( GameOpt.MapHexagonal )
    {
        switch( dir )
        {
        case 0:
            dir = 2;
            break;
        case 1:
            dir = 3;
            break;
        case 2:
            dir = 4;
            break;
        case 3:
            dir = 6;
            break;
        case 4:
            dir = 7;
            break;
        case 5:
            dir = 0;
            break;
        default:
            dir = 0;
            break;
        }
    }
    else
    {
        dir = ( dir + 2 ) % 8;
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

    char* delim = strstr( file_name, "$" );
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
            char* rgb_beg = strstr( seq_name, "[" );
            while( rgb_beg )
            {
                // Find end of offsets data
                char* rgb_end = strstr( rgb_beg + 1, "]" );
                if( !rgb_end )
                    break;

                // Parse numbers
                int rgb[ 4 ];
                if( sscanf( rgb_beg + 1, "%d,%d,%d,%d", &rgb[ 0 ], &rgb[ 1 ], &rgb[ 2 ], &rgb[ 3 ] ) != 4 )
                    break;

                // To one part
                if( rgb[ 0 ] >= 0 && rgb[ 0 ] <= 3 )
                {
                    rgb_offs[ rgb[ 0 ] ][ 0 ] = rgb[ 1 ];           // R
                    rgb_offs[ rgb[ 0 ] ][ 1 ] = rgb[ 2 ];           // G
                    rgb_offs[ rgb[ 0 ] ][ 2 ] = rgb[ 3 ];           // B
                }
                // To all parts
                else
                {
                    for( int i = 0; i < 4; i++ )
                    {
                        rgb_offs[ i ][ 0 ] = rgb[ 1 ];                 // R
                        rgb_offs[ i ][ 1 ] = rgb[ 2 ];                 // G
                        rgb_offs[ i ][ 2 ] = rgb[ 3 ];                 // B
                    }
                }

                // Erase from name and find again
                Str::EraseInterval( rgb_beg, (uint) ( rgb_end - rgb_beg + 1 ) );
                rgb_beg = strstr( seq_name, "[" );
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
    } static* cached[ SPR_CACHED_COUNT + 1 ] = { 0 }; // Last index for last loaded

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

        if( fm.GetFsize() > 1000000 )     // 1mb
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

            cached[ index ] = new (nothrow) SprCache();
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
                cached[ index ] = new (nothrow) SprCache();
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
    float dimension_right = (float) fm.GetUChar() * 6.7f;
    int   center_x = fm.GetLEUInt();
    int   center_y = fm.GetLEUInt();
    fm.GoForward( 2 );                 // ushort unknown1  sometimes it is 0, and sometimes it is 3
    fm.GoForward( 1 );                 // CHAR unknown2  0x64, other values were not observed

    float   ta = RAD( 127.0f / 2.0f ); // Tactics grid angle
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
        fm.GoForward( sizeof( uint ) * item_cnt );   // uint  unknown3[item_cnt]

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
        return false;

    // Find animation
    fm.SetCurPos( 0 );
    for( uint i = 0; i <= anim_index; i++ )
    {
        if( !fm.FindFragment( (uchar*) "<spranim>", 9, fm.GetCurPos() ) )
            return NULL;
        fm.GoForward( 12 );
    }

    uint    file_offset = fm.GetLEUInt();
    fm.GoForward( fm.GetLEUInt() );   // Collection name
    uint    frame_cnt = fm.GetLEUInt();
    uint    dir_cnt = fm.GetLEUInt();
    UIntVec bboxes;
    bboxes.resize( frame_cnt * dir_cnt * 4 );
    fm.CopyMem( &bboxes[ 0 ], sizeof( uint ) * frame_cnt * dir_cnt * 4 );

    // Fix dir
    if( dir_cnt != 8 )
        dir = dir * ( dir_cnt * 100 / 8 ) / 100;
    if( (uint) dir >= dir_cnt )
        dir = 0;

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
    fm.GoForward( 14 );  // <spranim_img>\0
    uchar type = fm.GetUChar();
    fm.GoForward( 1 );   // \0

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

    AnyFrames* anim = CreateAnimation( (uint) frames.size(), 1000 / 10 * (uint) frames.size() );
    if( !anim )
        return NULL;

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
            uint frm_index = ( type == 0x32 ? frame_cnt * dir_cnt * part + dir * frame_cnt + frm : ( ( frm * dir_cnt + dir ) << 2 ) + part );
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

        // Data for FillSurfaceFromMemory
        uint   img_size = 12 + whole_h * whole_w * 4;
        uchar* img_data = new (nothrow) uchar[ img_size ];
        if( !img_data )
            return NULL;
        *( (uint*) img_data + 1 ) = whole_w;
        *( (uint*) img_data + 2 ) = whole_h;

        for( uint part = 0; part < 4; part++ )
        {
            uint frm_index = ( type == 0x32 ? frame_cnt * dir_cnt * part + dir * frame_cnt + frm : ( ( frm * dir_cnt + dir ) << 2 ) + part );
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
            uchar  palette_present = fm_images.GetUChar();
            uint   rle_size = fm_images.GetLEUInt();
            uchar* rle_buf = fm_images.GetCurBuf();
            fm_images.GoForward( rle_size );
            uchar  def_color = 0;

            if( zar[ 5 ] || strcmp( zar, "<zar>" ) || ( subtype != 0x34 && subtype != 0x35 ) || palette_present == 1 )
            {
                delete anim;
                return NULL;
            }

            uint* ptr = (uint*) img_data + 3 + ( posy * whole_w ) + posx;
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

                    if( !part )
                        *ptr = col;
                    else if( ( col >> 24 ) >= 128 )
                        *ptr = col;
                    ptr++;

                    if( ++x >= w + posx )
                    {
                        x = posx;
                        y++;
                        ptr = (uint*) img_data + 3 + ( y * whole_w ) + x;
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

        SpriteInfo* si = new (nothrow) SpriteInfo();
        si->OffsX = bboxes[ frm * dir_cnt * 4 + dir * 4 + 0 ] - center_x + center_x_ex + whole_w / 2;
        si->OffsY = bboxes[ frm * dir_cnt * 4 + dir * 4 + 1 ] - center_y + center_y_ex + whole_h;
        uint result = FillSurfaceFromMemory( si, img_data, img_size );
        if( !result )
        {
            delete anim;
            return NULL;
        }
        anim->Ind[ f ] = result;
    }

    return anim;
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

    // Data for FillSurfaceFromMemory
    uint   img_size = 12 + h * w * 4;
    uchar* img_data = new (nothrow) uchar[ img_size ];
    if( !img_data )
        return NULL;
    *( (uint*) img_data + 1 ) = w;
    *( (uint*) img_data + 2 ) = h;
    uint* ptr = (uint*) img_data + 3;

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
    SpriteInfo* si = new (nothrow) SpriteInfo();
    uint        result = FillSurfaceFromMemory( si, img_data, img_size );
    if( !result )
        return NULL;

    AnyFrames* anim = CreateAnimation( 1, 100 );
    if( !anim )
        return NULL;
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
    uint h = fm.GetLEUInt();

    if( !fm.FindFragment( (uchar*) "<tiledata>", 10, fm.GetCurPos() ) )
        return NULL;
    fm.GoForward( 10 + 3 ); // Signature
    uint frames_count = fm.GetLEUInt();

    // Create animation
    AnyFrames* anim = CreateAnimation( frames_count, 1000 / 10 * frames_count );
    if( !anim )
        return NULL;

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

        // Data for FillSurfaceFromMemory
        uint   img_size = 12 + h * w * 4;
        uchar* img_data = new (nothrow) uchar[ img_size ];
        if( !img_data )
            return NULL;
        *( (uint*) img_data + 1 ) = w;
        *( (uint*) img_data + 2 ) = h;
        uint* ptr = (uint*) img_data + 3;

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
        SpriteInfo* si = new (nothrow) SpriteInfo();
        uint        result = FillSurfaceFromMemory( si, img_data, img_size );
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
    uint palette_offset = fm.GetLEUInt(); // Offset (from start of file) to palettes
    uint tiles_offset = palette_offset + col * row * 256 * 4;
    uint data_offset = tiles_offset + col * row * 4;

    // Data for FillSurfaceFromMemory
    uint   img_size = 12 + h * w * 4;
    uchar* img_data = new (nothrow) uchar[ img_size ];
    if( !img_data )
        return NULL;
    *( (uint*) img_data + 1 ) = w;
    *( (uint*) img_data + 2 ) = h;
    uint* ptr = (uint*) img_data + 3;

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
    SpriteInfo* si = new (nothrow) SpriteInfo();
    uint        result = FillSurfaceFromMemory( si, img_data, img_size );
    if( !result )
        return NULL;

    AnyFrames* anim = CreateAnimation( 1, 100 );
    if( !anim )
        return NULL;
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

    char* delim = strstr( file_name, "$" );
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
            const char* next_param = strstr( params, "-" );
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
    AnyFrames* anim = CreateAnimation( specific_frame == -1 ? cycle_frames : 1, 0 );
    if( !anim )
        return NULL;

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

        // Data for FillSurfaceFromMemory
        uint   img_size = 12 + h * w * 4;
        uchar* img_data = new (nothrow) uchar[ img_size ];
        if( !img_data )
            return NULL;
        *( (uint*) img_data + 1 ) = w;
        *( (uint*) img_data + 2 ) = h;
        uint* ptr = (uint*) img_data + 3;

        // Fill it
        fm.SetCurPos( data_offset );
        for( uint k = 0, l = w * h; k < l; k++ )
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
            }
        }

        // Set in animation sequence
        SpriteInfo* si = new (nothrow) SpriteInfo();
        uint        result = FillSurfaceFromMemory( si, img_data, img_size );
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


AnyFrames* SpriteManager::LoadAnimationOther( const char* fname, int path_type )
{
    // Load file
    FileManager fm;
    if( !fm.LoadFile( fname, path_type ) )
        return NULL;

    // Detect type
    ILenum file_type = ilTypeFromExt( fname );
    if( file_type == IL_TYPE_UNKNOWN )
        return NULL;

    // Load image from memory
    ILuint img = 0;
    ilGenImages( 1, &img );
    ilBindImage( img );
    if( !ilLoadL( file_type, fm.GetBuf(), fm.GetFsize() ) )
    {
        ilDeleteImage( img );
        return NULL;
    }

    // Get image data
    uint w = ilGetInteger( IL_IMAGE_WIDTH );
    uint h = ilGetInteger( IL_IMAGE_HEIGHT );
    int  format = ilGetInteger( IL_IMAGE_FORMAT );
    int  type = ilGetInteger( IL_IMAGE_TYPE );

    // Convert data
    if( format != IL_BGRA || type != IL_UNSIGNED_BYTE )
    {
        if( !ilConvertImage( IL_BGRA, IL_UNSIGNED_BYTE ) )
        {
            ilDeleteImage( img );
            return NULL;
        }
    }

    // Data for FillSurfaceFromMemory
    uint   size = 12 + h * w * 4;
    uchar* data = new uchar[ size ];
    *( (uint*) data + 1 ) = w;
    *( (uint*) data + 2 ) = h;

    // Copy data, also swap blue color to transparent
    uint* from = (uint*) ilGetData();
    uint* to = (uint*) data + 3;
    for( uint i = 0, j = w * h; i < j; i++ )
    {
        if( *from == 0xFF0000FF )
            *from = 0;
        *to = *from;
        ++from;
        ++to;
    }

    // Delete image
    ilDeleteImage( img );

    // Fill data
    SpriteInfo* si = new (nothrow) SpriteInfo();
    uint        result = FillSurfaceFromMemory( si, data, size );
    if( !result )
        return NULL;

    // Create animation
    AnyFrames* anim = CreateAnimation( 1, 100 );
    if( !anim )
        return NULL;
    anim->Ind[ 0 ] = result;
    return anim;
}

uint SpriteManager::Render3dSprite( Animation3d* anim3d, int dir, int time_proc )
{
    #ifdef FO_D3D
    // Render
    if( !sceneBeginned )
        D3D_HR( d3dDevice->BeginScene() );
    LPDIRECT3DSURFACE9 old_rt = NULL, old_ds = NULL;
    D3D_HR( d3dDevice->GetRenderTarget( 0, &old_rt ) );
    D3D_HR( d3dDevice->GetDepthStencilSurface( &old_ds ) );
    D3D_HR( d3dDevice->SetDepthStencilSurface( spr3dDS ) );
    D3D_HR( d3dDevice->SetRenderTarget( 0, spr3dRT ) );
    D3D_HR( d3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0 ) );

    Animation3d::SetScreenSize( spr3dSurfWidth, spr3dSurfHeight );
    anim3d->EnableSetupBorders( false );
    if( dir < 0 || dir >= DIRS_COUNT )
        anim3d->SetDirAngle( dir );
    else
        anim3d->SetDir( dir );
    anim3d->SetAnimation( 0, time_proc, NULL, ANIMATION_ONE_TIME | ANIMATION_STAY );
    Render3d( spr3dSurfWidth / 2, spr3dSurfHeight - spr3dSurfHeight / 4, 1.0f, anim3d, NULL, 0xFFFFFFFF );
    anim3d->EnableSetupBorders( true );
    anim3d->SetupBorders();
    Animation3d::SetScreenSize( modeWidth, modeHeight );

    D3D_HR( d3dDevice->SetRenderTarget( 0, old_rt ) );
    D3D_HR( d3dDevice->SetDepthStencilSurface( old_ds ) );
    old_rt->Release();
    old_ds->Release();
    if( !sceneBeginned )
        D3D_HR( d3dDevice->EndScene() );

    // Calculate sprite borders
    Rect fb = anim3d->GetFullBorders();
    RECT r_ = { fb.L, fb.T, fb.R + 1, fb.B + 1 };

    // Grow surfaces while sprite not fitted in it
    if( fb.L < 0 || fb.R >= spr3dSurfWidth || fb.T < 0 || fb.B >= spr3dSurfHeight )
    {
        // Grow x2
        if( fb.L < 0 || fb.R >= spr3dSurfWidth )
            spr3dSurfWidth *= 2;
        if( fb.T < 0 || fb.B >= spr3dSurfHeight )
            spr3dSurfHeight *= 2;

        // Recreate
        SAFEREL( spr3dRT );
        SAFEREL( spr3dRTEx );
        SAFEREL( spr3dDS );
        SAFEREL( spr3dRTData );
        D3D_HR( d3dDevice->CreateRenderTarget( spr3dSurfWidth, spr3dSurfHeight, D3DFMT_A8R8G8B8, presentParams.MultiSampleType, presentParams.MultiSampleQuality, FALSE, &spr3dRT, NULL ) );
        if( presentParams.MultiSampleType != D3DMULTISAMPLE_NONE )
            D3D_HR( d3dDevice->CreateRenderTarget( spr3dSurfWidth, spr3dSurfHeight, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &spr3dRTEx, NULL ) );
        D3D_HR( d3dDevice->CreateDepthStencilSurface( spr3dSurfWidth, spr3dSurfHeight, D3DFMT_D24S8, presentParams.MultiSampleType, presentParams.MultiSampleQuality, TRUE, &spr3dDS, NULL ) );
        D3D_HR( d3dDevice->CreateOffscreenPlainSurface( spr3dSurfWidth, spr3dSurfHeight, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &spr3dRTData, NULL ) );

        // Try load again
        return Render3dSprite( anim3d, dir, time_proc );
    }

    // Get render target data
    if( presentParams.MultiSampleType != D3DMULTISAMPLE_NONE )
    {
        D3D_HR( d3dDevice->StretchRect( spr3dRT, &r_, spr3dRTEx, &r_, D3DTEXF_NONE ) );
        D3D_HR( d3dDevice->GetRenderTargetData( spr3dRTEx, spr3dRTData ) );
    }
    else
    {
        D3D_HR( d3dDevice->GetRenderTargetData( spr3dRT, spr3dRTData ) );
    }

    // Copy to system memory
    LockRect_ lr;
    D3D_HR( spr3dRTData->LockRect( &lr, &r_, D3DLOCK_READONLY ) );
    uint      w = fb.W();
    uint      h = fb.H();
    uint      size = 12 + h * w * 4;
    uchar*    data = new (nothrow) uchar[ size ];
    *( (uint*) data + 1 ) = w;
    *( (uint*) data + 2 ) = h;
    for( uint i = 0; i < h; i++ )
        memcpy( data + 12 + w * 4 * i, (uchar*) lr.pBits + lr.Pitch * i, w * 4 );
    D3D_HR( spr3dRTData->UnlockRect() );

    // Fill from memory
    SpriteInfo* si = new (nothrow) SpriteInfo();
    Point       p;
    anim3d->GetFullBorders( &p );
    si->OffsX = fb.W() / 2 - p.X;
    si->OffsY = fb.H() - p.Y;
    return FillSurfaceFromMemory( si, data, size );
    #else
    // Create render targets
    if( !rt3DSprite.FBO )
    {
        static const uint init_size = 512;
        if( !CreateRenderTarget( rt3DSprite, true, false, 512, 512 ) )
            return 0;
        CreateRenderTarget( rt3DMSSprite, true, true, 512, 512 );
    }

    // Prepare and render
    int rt_width = rt3DSprite.TargetTexture->Width;
    int rt_height = rt3DSprite.TargetTexture->Height;
    Animation3d::SetScreenSize( rt_width, rt_height );
    anim3d->EnableSetupBorders( false );
    if( dir < 0 || dir >= DIRS_COUNT )
        anim3d->SetDirAngle( dir );
    else
        anim3d->SetDir( dir );
    anim3d->SetAnimation( 0, time_proc, NULL, ANIMATION_ONE_TIME | ANIMATION_STAY );

    PushRenderTarget( rt3DSprite );
    ClearRenderTarget( rt3DSprite, 0 );
    ClearRenderTargetDS( rt3DSprite, true, false );
    GL( glDisable( GL_BLEND ) );
    anim3d->Draw( rt_width / 2, rt_height - rt_height / 4, 1.0f, NULL, 0 );
    GL( glEnable( GL_BLEND ) );
    PopRenderTarget();

    anim3d->EnableSetupBorders( true );
    anim3d->SetupBorders();
    Animation3d::SetScreenSize( modeWidth, modeHeight );
    Rect fb = anim3d->GetFullBorders();

    // Copy from multisampled to default rt
    if( rt3DMSSprite.FBO )
    {
        GL( glDisable( GL_BLEND ) );
        GL( glMatrixMode( GL_PROJECTION ) );
        GL( glLoadIdentity() );
        GL( gluOrtho2D( 0, rt_width, rt_height, 0 ) );
        GL( glGetFloatv( GL_PROJECTION_MATRIX, projectionMatrix ) );
        GL( glViewport( 0, 0, rt_width, rt_height ) );

        PushRenderTarget( rt3DMS );
        DrawRenderTarget( rt3DMS, &fb );
        PopRenderTarget();

        GL( glEnable( GL_BLEND ) );
        GL( glMatrixMode( GL_PROJECTION ) );
        GL( glLoadIdentity() );
        GL( gluOrtho2D( 0, modeWidth, modeHeight, 0 ) );
        GL( glGetFloatv( GL_PROJECTION_MATRIX, projectionMatrix ) );
        GL( glViewport( 0, 0, modeWidth, modeHeight ) );
    }

    // Grow surfaces while sprite not fitted in it
    if( fb.L < 0 || fb.R >= rt_width || fb.T < 0 || fb.B >= rt_height )
    {
        // Grow x2
        if( fb.L < 0 || fb.R >= rt_width )
            rt_width *= 2;
        if( fb.T < 0 || fb.B >= rt_height )
            rt_height *= 2;

        // Recreate
        RenderTarget old_rt = rt3DSprite;
        RenderTarget old_rtms = rt3DMSSprite;
        bool         result;
        if( rt3DMSSprite.FBO )
            result = CreateRenderTarget( rt3DSprite, true, false, rt_width, rt_height ) &&
                     CreateRenderTarget( rt3DMSSprite, true, true, rt_width, rt_height );
        else
            result = CreateRenderTarget( rt3DSprite, true, false, rt_width, rt_height );
        if( !result )
        {
            WriteLogF( _FUNC_, " - Size of model is too big.\n" );
            rt3DSprite = old_rt;
            rt3DMSSprite = old_rtms;
            return 0;
        }
        DeleteRenderTarget( old_rt );
        if( old_rtms.FBO )
            DeleteRenderTarget( old_rtms );

        // Try load again
        return Render3dSprite( anim3d, dir, time_proc );
    }

    // Copy to system memory
    uint   w = fb.W();
    uint   h = fb.H();
    uint   size = 12 + h * w * 4;
    uchar* data = new (nothrow) uchar[ size ];
    *( (uint*) data + 1 ) = w;
    *( (uint*) data + 2 ) = h;
    PushRenderTarget( rt3DSprite );
    GL( glReadPixels( fb.L, rt_height - fb.B, w, h, GL_BGRA, GL_UNSIGNED_BYTE, data + 12 ) );
    PopRenderTarget();

    // Flip
    uint* data4 = (uint*) ( data + 12 );
    for( uint y = 0; y < h / 2; y++ )
        for( uint x = 0; x < w; x++ )
            std::swap( data4[ y * w + x ], data4[ ( h - y - 1 ) * w + x ] );

    // Fill from memory
    SpriteInfo* si = new (nothrow) SpriteInfo();
    Point       p;
    anim3d->GetFullBorders( &p );
    si->OffsX = fb.W() / 2 - p.X;
    si->OffsY = fb.H() - p.Y;
    return FillSurfaceFromMemory( si, data, size );
    #endif
}

Animation3d* SpriteManager::LoadPure3dAnimation( const char* fname, int path_type )
{
    // Fill data
    Animation3d* anim3d = Animation3d::GetAnimation( fname, path_type, false );
    if( !anim3d )
        return NULL;

    // Add sprite information
    SpriteInfo* si = new (nothrow) SpriteInfo();
    uint        index = 1;
    for( uint j = (uint) sprData.size(); index < j; index++ )
        if( !sprData[ index ] )
            break;
    if( index < (uint) sprData.size() )
        sprData[ index ] = si;
    else
        sprData.push_back( si );

    // Fake surface, filled after rendering
    si->Surf = new Surface();

    // Cross links
    anim3d->SetSprId( index );
    si->Anim3d = anim3d;
    return anim3d;
}

void SpriteManager::FreePure3dAnimation( Animation3d* anim3d )
{
    if( anim3d )
    {
        uint spr_id = anim3d->GetSprId();
        if( spr_id )
        {
            sprData[ spr_id ]->Surf->TextureOwner = NULL;
            SAFEDEL( sprData[ spr_id ]->Surf );
            SAFEDEL( sprData[ spr_id ] );
        }
        SAFEDEL( anim3d );
    }
}

bool SpriteManager::Flush()
{
    if( !isInit )
        return false;
    if( !curSprCnt )
        return true;
    int mulpos = 4 * curSprCnt;

    #ifdef FO_D3D
    for( int i = 0; i < mulpos; i++ )
    {
        Vertex& v = vBuffer[ i ];
        v.x -= 0.5f;
        v.y -= 0.5f;
        v.diffuse = COLOR_FIX( v.diffuse );
    }
    void* ptr;
    D3D_HR( vbMain->Lock( 0, sizeof( Vertex ) * mulpos, (void**) &ptr, D3DLOCK_DISCARD ) );
    memcpy( ptr, &vBuffer[ 0 ], sizeof( Vertex ) * mulpos );
    D3D_HR( vbMain->Unlock() );
    D3D_HR( d3dDevice->SetIndices( ibMain ) );
    D3D_HR( d3dDevice->SetStreamSource( 0, vbMain, 0, sizeof( Vertex ) ) );
    D3D_HR( d3dDevice->SetFVF( D3DFVF_MYVERTEX ) );
    #else
    for( int i = 0; i < mulpos; i++ )
    {
        Vertex& v = vBuffer[ i ];
        v.diffuse = COLOR_FIX( v.diffuse );
    }
    GL( glBindVertexArray( vaMain ) );
    GL( glBindBuffer( GL_ARRAY_BUFFER, vbMain ) );
    GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibMain ) );
    GL( glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof( Vertex ) * mulpos, &vBuffer[ 0 ] ) );
    #endif

    uint rpos = 0;
    for( auto it = dipQueue.begin(), end = dipQueue.end(); it != end; ++it )
    {
        DipData& dip = *it;
        Effect*  effect = ( dip.SourceEffect ? dip.SourceEffect : curDefaultEffect );

        #ifdef FO_D3D
        D3D_HR( d3dDevice->SetTexture( 0, dip.SourceTexture->Instance ) );
        if( effect )
        {
            LPD3DXEFFECT dxeffect = effect->DXInstance;

            if( effect->EffectParams )
                D3D_HR( dxeffect->ApplyParameterBlock( effect->EffectParams ) );
            D3D_HR( dxeffect->SetTechnique( effect->TechniqueSimple ) );
            if( effect->IsNeedProcess )
                GraphicLoader::EffectProcessVariables( effect, -1 );

            UINT passes;
            D3D_HR( dxeffect->Begin( &passes, effect->EffectFlags ) );
            for( UINT pass = 0; pass < passes; pass++ )
            {
                if( effect->IsNeedProcess )
                    GraphicLoader::EffectProcessVariables( effect, pass );

                D3D_HR( dxeffect->BeginPass( pass ) );
                D3D_HR( d3dDevice->DrawIndexedPrimitive( (D3DPRIMITIVETYPE) PRIMITIVE_TRIANGLELIST, 0, 0, mulpos, rpos, 2 * dip.SpritesCount ) );
                D3D_HR( dxeffect->EndPass() );
            }
            D3D_HR( dxeffect->End() );
        }
        else
        {
            D3D_HR( d3dDevice->SetVertexShader( NULL ) );
            D3D_HR( d3dDevice->SetPixelShader( NULL ) );
            D3D_HR( d3dDevice->DrawIndexedPrimitive( (D3DPRIMITIVETYPE) PRIMITIVE_TRIANGLELIST, 0, 0, mulpos, rpos, 2 * dip.SpritesCount ) );
        }
        #else
        GL( glUseProgram( effect->Program ) );

        if( effect->ZoomFactor != -1 )
            GL( glUniform1f( effect->ZoomFactor, GameOpt.SpritesZoom ) );
        if( effect->ProjectionMatrix != -1 )
            GL( glUniformMatrix4fv( effect->ProjectionMatrix, 1, GL_FALSE, projectionMatrix ) );
        if( effect->ColorMap != -1 && dip.SourceTexture )
        {
            GL( glActiveTexture( GL_TEXTURE0 ) );
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
        if( effect->EggMap != -1 )
        {
            GL( glActiveTexture( GL_TEXTURE1 ) );
            GL( glBindTexture( GL_TEXTURE_2D, sprEgg->Surf->TextureOwner->Id ) );
            GL( glUniform1i( effect->EggMap, 1 ) );
            if( effect->EggMapSize != -1 )
                GL( glUniform4fv( effect->EggMapSize, 1, sprEgg->Surf->TextureOwner->SizeData ) );
        }
        if( effect->SpriteBorder != -1 )
            GL( glUniform4f( effect->SpriteBorder, dip.SpriteBorder.L, dip.SpriteBorder.T, dip.SpriteBorder.R, dip.SpriteBorder.B ) );

        GLuint  min_index = rpos * 4;
        GLuint  max_index = min_index + dip.SpritesCount * 4 - 1;
        GLsizei count = 6 * dip.SpritesCount;
        if( effect->IsNeedProcess )
            GraphicLoader::EffectProcessVariables( effect, -1 );
        for( uint pass = 0; pass < effect->Passes; pass++ )
        {
            if( effect->IsNeedProcess )
                GraphicLoader::EffectProcessVariables( effect, pass );
            GL( glDrawRangeElements( GL_TRIANGLES, min_index, max_index, count, GL_UNSIGNED_SHORT, (void*) ( rpos * 2 ) ) );
        }
        #endif

        rpos += 6 * dip.SpritesCount;
    }
    dipQueue.clear();
    curSprCnt = 0;

    #ifndef FO_D3D
    GL( glUseProgram( 0 ) );
    #endif

    return true;
}

bool SpriteManager::DrawSprite( uint id, int x, int y, uint color /* = 0 */ )
{
    if( !id )
        return false;

    SpriteInfo* si = sprData[ id ];
    if( !si )
        return false;

    if( si->Anim3d )
    {
        #ifdef FO_D3D
        Render3d( x, y, 1.0f, si->Anim3d, NULL, color );
        return true;
        #else
        Render3d( x, y, 1.0f, si->Anim3d, NULL, color );
        #endif
    }

    Effect* effect = ( si->DrawEffect ? si->DrawEffect : sprDefaultEffect[ DEFAULT_EFFECT_IFACE ] );
    if( dipQueue.empty() || dipQueue.back().SourceTexture != si->Surf->TextureOwner || dipQueue.back().SourceEffect != effect )
        dipQueue.push_back( DipData( si->Surf->TextureOwner, effect ) );
    else
        dipQueue.back().SpritesCount++;

    int mulpos = curSprCnt * 4;

    if( !color )
        color = COLOR_IFACE;

    vBuffer[ mulpos ].x = (float) x;
    vBuffer[ mulpos ].y = (float) y + si->Height;
    vBuffer[ mulpos ].tu = si->SprRect.L;
    vBuffer[ mulpos ].tv = si->SprRect.B;
    vBuffer[ mulpos++ ].diffuse = color;

    vBuffer[ mulpos ].x = (float) x;
    vBuffer[ mulpos ].y = (float) y;
    vBuffer[ mulpos ].tu = si->SprRect.L;
    vBuffer[ mulpos ].tv = si->SprRect.T;
    vBuffer[ mulpos++ ].diffuse = color;

    vBuffer[ mulpos ].x = (float) x + si->Width;
    vBuffer[ mulpos ].y = (float) y;
    vBuffer[ mulpos ].tu = si->SprRect.R;
    vBuffer[ mulpos ].tv = si->SprRect.T;
    vBuffer[ mulpos++ ].diffuse = color;

    vBuffer[ mulpos ].x = (float) x + si->Width;
    vBuffer[ mulpos ].y = (float) y + si->Height;
    vBuffer[ mulpos ].tu = si->SprRect.R;
    vBuffer[ mulpos ].tv = si->SprRect.B;
    vBuffer[ mulpos ].diffuse = color;

    if( ++curSprCnt == flushSprCnt )
        Flush();
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

    if( si->Anim3d )
    {
        #ifdef FO_D3D
        Render3d( x, y, 1.0f, si->Anim3d, NULL, color );
        return true;
        #else
        Render3d( x, y, 1.0f, si->Anim3d, NULL, color );
        #endif
    }

    Effect* effect = ( si->DrawEffect ? si->DrawEffect : sprDefaultEffect[ DEFAULT_EFFECT_IFACE ] );
    if( dipQueue.empty() || dipQueue.back().SourceTexture != si->Surf->TextureOwner || dipQueue.back().SourceEffect != effect )
        dipQueue.push_back( DipData( si->Surf->TextureOwner, effect ) );
    else
        dipQueue.back().SpritesCount++;

    int mulpos = curSprCnt * 4;

    if( !color )
        color = COLOR_IFACE;

    vBuffer[ mulpos ].x = (float) x;
    vBuffer[ mulpos ].y = (float) y + hf;
    vBuffer[ mulpos ].tu = si->SprRect.L;
    vBuffer[ mulpos ].tv = si->SprRect.B;
    vBuffer[ mulpos++ ].diffuse = color;

    vBuffer[ mulpos ].x = (float) x;
    vBuffer[ mulpos ].y = (float) y;
    vBuffer[ mulpos ].tu = si->SprRect.L;
    vBuffer[ mulpos ].tv = si->SprRect.T;
    vBuffer[ mulpos++ ].diffuse = color;

    vBuffer[ mulpos ].x = (float) x + wf;
    vBuffer[ mulpos ].y = (float) y;
    vBuffer[ mulpos ].tu = si->SprRect.R;
    vBuffer[ mulpos ].tv = si->SprRect.T;
    vBuffer[ mulpos++ ].diffuse = color;

    vBuffer[ mulpos ].x = (float) x + wf;
    vBuffer[ mulpos ].y = (float) y + hf;
    vBuffer[ mulpos ].tu = si->SprRect.R;
    vBuffer[ mulpos ].tv = si->SprRect.B;
    vBuffer[ mulpos ].diffuse = color;

    if( ++curSprCnt == flushSprCnt )
        Flush();
    return true;
}

void SpriteManager::PrepareSquare( PointVec& points, RectF& r, uint color )
{
    points.push_back( PrepPoint( (short) r.L, (short) r.B, color, NULL, NULL ) );
    points.push_back( PrepPoint( (short) r.L, (short) r.T, color, NULL, NULL ) );
    points.push_back( PrepPoint( (short) r.R, (short) r.B, color, NULL, NULL ) );
    points.push_back( PrepPoint( (short) r.L, (short) r.T, color, NULL, NULL ) );
    points.push_back( PrepPoint( (short) r.R, (short) r.T, color, NULL, NULL ) );
    points.push_back( PrepPoint( (short) r.R, (short) r.B, color, NULL, NULL ) );
}

void SpriteManager::PrepareSquare( PointVec& points, PointF& lt, PointF& rt, PointF& lb, PointF& rb, uint color )
{
    points.push_back( PrepPoint( (short) lb.X, (short) lb.Y, color, NULL, NULL ) );
    points.push_back( PrepPoint( (short) lt.X, (short) lt.Y, color, NULL, NULL ) );
    points.push_back( PrepPoint( (short) rb.X, (short) rb.Y, color, NULL, NULL ) );
    points.push_back( PrepPoint( (short) lt.X, (short) lt.Y, color, NULL, NULL ) );
    points.push_back( PrepPoint( (short) rt.X, (short) rt.Y, color, NULL, NULL ) );
    points.push_back( PrepPoint( (short) rb.X, (short) rb.Y, color, NULL, NULL ) );
}

uint SpriteManager::PackColor( int r, int g, int b )
{
    r = CLAMP( r, 0, 255 );
    g = CLAMP( g, 0, 255 );
    b = CLAMP( b, 0, 255 );
    return COLOR_XRGB( r, g, b );
}

void SpriteManager::GetDrawRect( Sprite* prep, Rect& rect )
{
    uint id = ( prep->PSprId ? *prep->PSprId : prep->SprId );
    if( id >= sprData.size() )
        return;
    SpriteInfo* si = sprData[ id ];
    if( !si )
        return;

    if( !si->Anim3d )
    {
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
    else
    {
        rect = si->Anim3d->GetBaseBorders();
        rect.L = (int) ( (float) rect.L * GameOpt.SpritesZoom );
        rect.T = (int) ( (float) rect.T * GameOpt.SpritesZoom );
        rect.R = (int) ( (float) rect.R * GameOpt.SpritesZoom );
        rect.B = (int) ( (float) rect.B * GameOpt.SpritesZoom );
    }
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

    if( !si->Anim3d )
    {
        eggX = spr->ScrX + si->OffsX - sprEgg->Width / 2 + *spr->OffsX;
        eggY = spr->ScrY - si->Height / 2 + si->OffsY - sprEgg->Height / 2 + *spr->OffsY;
    }
    else
    {
        #ifdef FO_D3D
        Rect bb = si->Anim3d->GetBaseBorders();
        int  w = (int) ( (float) bb.W() * GameOpt.SpritesZoom );
        int  h = (int) ( (float) bb.H() * GameOpt.SpritesZoom );
        eggX = spr->ScrX - w / 2 + si->OffsX + w / 2 - sprEgg->Width / 2 + *spr->OffsX;
        eggY = spr->ScrY - h + si->OffsY + h / 2 - sprEgg->Height / 2 + *spr->OffsY;
        #else
        Rect bb = si->Anim3d->GetFullBorders();
        int  w = (int) ( (float) bb.W() * GameOpt.SpritesZoom );
        int  h = (int) ( (float) bb.H() * GameOpt.SpritesZoom );
        eggX = spr->ScrX - sprEgg->Width / 2 + *spr->OffsX;
        eggY = spr->ScrY - h / 2 - sprEgg->Height / 2 + *spr->OffsY;
        #endif
    }

    eggHx = hx;
    eggHy = hy;
    eggValid = true;
}

bool SpriteManager::DrawSprites( Sprites& dtree, bool collect_contours, bool use_egg, int draw_oder_from, int draw_oder_to )
{
    PointVec borders;

    if( !eggValid )
        use_egg = false;
    bool egg_trans = false;
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

        #ifdef FO_D3D
        // Check borders
        if( !si->Anim3d )
        {
            if( x / zoom > modeWidth || ( x + si->Width ) / zoom < 0 || y / zoom > modeHeight || ( y + si->Height ) / zoom < 0 )
                continue;
        }

        // 3d model
        if( si->Anim3d )
        {
            // Draw collected sprites and disable egg
            Flush();
            if( egg_trans )
            {
                # ifdef FO_D3D
                D3D_HR( d3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE ) );
                D3D_HR( d3dDevice->SetTexture( 1, NULL ) );
                # endif
                egg_trans = false;
            }

            // Draw 3d animation
            Render3d( (int) ( x / zoom ), (int) ( y / zoom ), 1.0f / zoom, si->Anim3d, NULL, cur_color );

            // Process contour effect
            if( collect_contours && spr->ContourType )
                CollectContour( x, y, si, spr );

            // Debug borders
            if( GameOpt.DebugInfo )
            {
                Rect eb = si->Anim3d->GetExtraBorders();
                Rect bb = si->Anim3d->GetBaseBorders();
                PrepareSquare( borders, RectF( (float) eb.L, (float) eb.T, (float) eb.R, (float) eb.B ), 0x5f750075 );
                PrepareSquare( borders, RectF( (float) bb.L, (float) bb.T, (float) bb.R, (float) bb.B ), 0x5f757575 );
            }

            continue;
        }
        #else
        // Render 3d
        if( si->Anim3d )
        {
            x += si->Width / 2 - si->OffsX;
            y += si->Height - si->OffsY;
            x = (int) ( (float) x / zoom );
            y = (int) ( (float) y / zoom );
            Render3d( x, y, 1.0f / zoom, si->Anim3d, NULL, 0  );
            x -= si->Width / 2 - si->OffsX;
            y -= si->Height - si->OffsY;
            zoom = 1.0f;
        }

        // Check borders
        if( x / zoom > modeWidth || ( x + si->Width ) / zoom < 0 || y / zoom > modeHeight || ( y + si->Height ) / zoom < 0 )
            continue;
        #endif

        // 2d sprite
        // Egg process
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
                if( !egg_trans )
                {
                    Flush();
                    #ifdef FO_D3D
                    D3D_HR( d3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_SELECTARG2 ) );
                    D3D_HR( d3dDevice->SetTexture( 1, sprEgg->Surf->TextureOwner->Instance ) );
                    #endif
                    egg_trans = true;
                }

                x1 = max( x1, 0 );
                y1 = max( y1, 0 );
                x2 = min( x2, eggSprWidth );
                y2 = min( y2, eggSprHeight );

                float x1f = (float) ( x1 + SURF_SPRITES_OFFS );
                float x2f = (float) ( x2 + SURF_SPRITES_OFFS );
                float y1f = (float) ( y1 + SURF_SPRITES_OFFS );
                float y2f = (float) ( y2 + SURF_SPRITES_OFFS );

                int   mulpos = curSprCnt * 4;
                vBuffer[ mulpos + 0 ].tu2 = x1f / eggSurfWidth;
                vBuffer[ mulpos + 0 ].tv2 = y2f / eggSurfHeight;
                vBuffer[ mulpos + 1 ].tu2 = x1f / eggSurfWidth;
                vBuffer[ mulpos + 1 ].tv2 = y1f / eggSurfHeight;
                vBuffer[ mulpos + 2 ].tu2 = x2f / eggSurfWidth;
                vBuffer[ mulpos + 2 ].tv2 = y1f / eggSurfHeight;
                vBuffer[ mulpos + 3 ].tu2 = x2f / eggSurfWidth;
                vBuffer[ mulpos + 3 ].tv2 = y2f / eggSurfHeight;

                egg_added = true;
            }
        }

        if( !egg_added && egg_trans )
        {
            Flush();
            #ifdef FO_D3D
            D3D_HR( d3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE ) );
            D3D_HR( d3dDevice->SetTexture( 1, NULL ) );
            #endif
            egg_trans = false;
        }

        // Choose surface
        if( dipQueue.empty() || dipQueue.back().SourceTexture != si->Surf->TextureOwner || dipQueue.back().SourceEffect != si->DrawEffect )
            dipQueue.push_back( DipData( si->Surf->TextureOwner, si->DrawEffect ) );
        else
            dipQueue.back().SpritesCount++;

        // Casts
        float xf = (float) x / zoom;
        float yf = (float) y / zoom;
        float wf = (float) si->Width / zoom;
        float hf = (float) si->Height / zoom;

        // Fill buffer
        int mulpos = curSprCnt * 4;

        vBuffer[ mulpos ].x = xf;
        vBuffer[ mulpos ].y = yf + hf;
        vBuffer[ mulpos ].tu = si->SprRect.L;
        vBuffer[ mulpos ].tv = si->SprRect.B;
        vBuffer[ mulpos++ ].diffuse = cur_color;

        vBuffer[ mulpos ].x = xf;
        vBuffer[ mulpos ].y = yf;
        vBuffer[ mulpos ].tu = si->SprRect.L;
        vBuffer[ mulpos ].tv = si->SprRect.T;
        vBuffer[ mulpos++ ].diffuse = cur_color;

        vBuffer[ mulpos ].x = xf + wf;
        vBuffer[ mulpos ].y = yf;
        vBuffer[ mulpos ].tu = si->SprRect.R;
        vBuffer[ mulpos ].tv = si->SprRect.T;
        vBuffer[ mulpos++ ].diffuse = cur_color;

        vBuffer[ mulpos ].x = xf + wf;
        vBuffer[ mulpos ].y = yf + hf;
        vBuffer[ mulpos ].tu = si->SprRect.R;
        vBuffer[ mulpos ].tv = si->SprRect.B;
        vBuffer[ mulpos++ ].diffuse = cur_color;

        // Cutted sprite
        if( spr->CutType )
        {
            xf = (float) ( x + spr->CutX ) / zoom;
            wf = spr->CutW / zoom;
            vBuffer[ mulpos - 4 ].x = xf;
            vBuffer[ mulpos - 4 ].tu = spr->CutTexL;
            vBuffer[ mulpos - 3 ].x = xf;
            vBuffer[ mulpos - 3 ].tu = spr->CutTexL;
            vBuffer[ mulpos - 2 ].x = xf + wf;
            vBuffer[ mulpos - 2 ].tu = spr->CutTexR;
            vBuffer[ mulpos - 1 ].x = xf + wf;
            vBuffer[ mulpos - 1 ].tu = spr->CutTexR;
        }

        // Set default texture coordinates for egg texture
        if( !egg_added )
        {
            vBuffer[ mulpos - 1 ].tu2 = 0.0f;
            vBuffer[ mulpos - 1 ].tv2 = 0.0f;
            vBuffer[ mulpos - 2 ].tu2 = 0.0f;
            vBuffer[ mulpos - 2 ].tv2 = 0.0f;
            vBuffer[ mulpos - 3 ].tu2 = 0.0f;
            vBuffer[ mulpos - 3 ].tv2 = 0.0f;
            vBuffer[ mulpos - 4 ].tu2 = 0.0f;
            vBuffer[ mulpos - 4 ].tv2 = 0.0f;
        }

        // Draw
        if( ++curSprCnt == flushSprCnt || si->Anim3d )
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
            # ifdef FO_D3D
            float    x1 = (float) ( spr->ScrX - si->Width / 2 + spr->CutX + GameOpt.ScrOx + 1.0f ) / z - 0.5f;
            float    y1 = (float) ( spr->ScrY + spr->CutOyL + GameOpt.ScrOy ) / z - 0.5f;
            float    x2 = (float) ( spr->ScrX - si->Width / 2 + spr->CutX + spr->CutW + GameOpt.ScrOx - 1.0f ) / z - 0.5f;
            float    y2 = (float) ( spr->ScrY + spr->CutOyR + GameOpt.ScrOy ) / z - 0.5f;
            # else
            float    x1 = (float) ( spr->ScrX - si->Width / 2 + spr->CutX + GameOpt.ScrOx + 1.0f ) / z;
            float    y1 = (float) ( spr->ScrY + spr->CutOyL + GameOpt.ScrOy ) / z;
            float    x2 = (float) ( spr->ScrX - si->Width / 2 + spr->CutX + spr->CutW + GameOpt.ScrOx - 1.0f ) / z;
            float    y2 = (float) ( spr->ScrY + spr->CutOyR + GameOpt.ScrOy ) / z;
            # endif
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
    if( egg_trans )
    {
        #ifdef FO_D3D
        D3D_HR( d3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE ) );
        D3D_HR( d3dDevice->SetTexture( 1, NULL ) );
        #endif
    }

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

    // 3d animation
    if( si->Anim3d )
    {
        /*if(!si->Anim3d->GetDrawIndex()) return 0;

           IDirect3DSurface9* zstencil;
           D3D_HR(d3dDevice->GetDepthStencilSurface(&zstencil));

           D3DSURFACE_DESC sDesc;
           D3D_HR(zstencil->GetDesc(&sDesc));
           int width=sDesc.Width;
           int height=sDesc.Height;

           D3DLOCKED_RECT desc;
           D3D_HR(zstencil->LockRect(&desc,NULL,D3DLOCK_READONLY));
           uchar* ptr=(uchar*)desc.pBits;
           int pitch=desc.Pitch;

           int stencil_offset=offs_y*pitch+offs_x*4+3;
           WriteLog("===========%d %d====%u\n",offs_x,offs_y,ptr[stencil_offset]);
           if(stencil_offset<pitch*height && ptr[stencil_offset]==si->Anim3d->GetDrawIndex())
           {
                D3D_HR(zstencil->UnlockRect());
                D3D_HR(zstencil->Release());
                return 1;
           }

           D3D_HR(zstencil->UnlockRect());
           D3D_HR(zstencil->Release());*/
        return 0;
    }

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

    #ifdef FO_D3D
    D3DSURFACE_DESC sDesc;
    D3D_HR( si->Surf->TextureOwner->Instance->GetLevelDesc( 0, &sDesc ) );
    int             width = sDesc.Width;
    int             height = sDesc.Height;

    LockRect_       desc;
    D3D_HR( si->Surf->TextureOwner->Instance->LockRect( 0, &desc, NULL, D3DLOCK_READONLY ) );
    uchar*          ptr = (uchar*) desc.pBits;
    int             pitch = desc.Pitch;

    offs_x += (int) ( width * si->SprRect.L );
    offs_y += (int) ( height * si->SprRect.T );
    int offset = offs_y * pitch + offs_x * 4;
    if( offset < pitch * height )
    {
        uint color = *(uint*) ( ptr + offset );
        D3D_HR( si->Surf->TextureOwner->Instance->UnlockRect( 0 ) );
        return color;
    }

    D3D_HR( si->Surf->TextureOwner->Instance->UnlockRect( 0 ) );
    #else
    offs_x += (int) ( si->Surf->TextureOwner->SizeData[ 0 ] * si->SprRect.L );
    offs_y += (int) ( si->Surf->TextureOwner->SizeData[ 1 ] * si->SprRect.T );
    return si->Surf->TextureOwner->Pixel( offs_x, offs_y );
    #endif
    return 0;
}

bool SpriteManager::IsEggTransp( int pix_x, int pix_y )
{
    if( !eggValid )
        return false;

    int ex = eggX + GameOpt.ScrOx;
    int ey = eggY + GameOpt.ScrOy;
    int ox = pix_x - (int) ( ex / GameOpt.SpritesZoom );
    int oy = pix_y - (int) ( ey / GameOpt.SpritesZoom );

    if( ox < 0 || oy < 0 || ox >= int(eggSurfWidth / GameOpt.SpritesZoom) || oy >= int(eggSurfHeight / GameOpt.SpritesZoom) )
        return false;

    ox = (int) ( ox * GameOpt.SpritesZoom );
    oy = (int) ( oy * GameOpt.SpritesZoom );

    #ifdef FO_D3D
    D3DSURFACE_DESC sDesc;
    D3D_HR( sprEgg->Surf->TextureOwner->Instance->GetLevelDesc( 0, &sDesc ) );

    int       sWidth = sDesc.Width;
    int       sHeight = sDesc.Height;

    LockRect_ lrDst;
    D3D_HR( sprEgg->Surf->TextureOwner->Instance->LockRect( 0, &lrDst, NULL, D3DLOCK_READONLY ) );

    uchar* pDst = (uchar*) lrDst.pBits;

    if( pDst[ oy * sHeight * 4 + ox * 4 + 3 ] < 170 )
    {
        D3D_HR( sprEgg->Surf->TextureOwner->Instance->UnlockRect( 0 ) );
        return true;
    }

    D3D_HR( sprEgg->Surf->TextureOwner->Instance->UnlockRect( 0 ) );
    #else
    return sprEgg->Surf->TextureOwner->Pixel( ox, oy ) < 170;
    #endif
    return false;
}

bool SpriteManager::DrawPoints( PointVec& points, int prim, float* zoom /* = NULL */, RectF* stencil /* = NULL */, PointF* offset /* = NULL */ )
{
    if( points.empty() )
        return true;
    Flush();

    #ifdef FO_D3D
    int count = (int) points.size();

    // Draw stencil quad
    if( stencil )
    {
        struct VertexUP
        {
            FLOAT x, y, z, rhw;
            uint  diffuse;
        } vb[ 6 ] =
        {
            # ifdef FO_D3D
            { stencil->L - 0.5f, stencil->B - 0.5f, 1.0f, 1.0f, -1 },
            { stencil->L - 0.5f, stencil->T - 0.5f, 1.0f, 1.0f, -1 },
            { stencil->R - 0.5f, stencil->B - 0.5f, 1.0f, 1.0f, -1 },
            { stencil->L - 0.5f, stencil->T - 0.5f, 1.0f, 1.0f, -1 },
            { stencil->R - 0.5f, stencil->T - 0.5f, 1.0f, 1.0f, -1 },
            { stencil->R - 0.5f, stencil->B - 0.5f, 1.0f, 1.0f, -1 },
            # else
            { stencil->L, stencil->B, 1.0f, 1.0f, -1 },
            { stencil->L, stencil->T, 1.0f, 1.0f, -1 },
            { stencil->R, stencil->B, 1.0f, 1.0f, -1 },
            { stencil->L, stencil->T, 1.0f, 1.0f, -1 },
            { stencil->R, stencil->T, 1.0f, 1.0f, -1 },
            { stencil->R, stencil->B, 1.0f, 1.0f, -1 },
            # endif
        };

        D3D_HR( d3dDevice->SetRenderState( D3DRS_STENCILENABLE, TRUE ) );
        D3D_HR( d3dDevice->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_NEVER ) );
        D3D_HR( d3dDevice->SetRenderState( D3DRS_STENCILFAIL, D3DSTENCILOP_REPLACE ) );
        D3D_HR( d3dDevice->SetRenderState( D3DRS_STENCILREF, 1 ) );
        D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE ) );

        D3D_HR( d3dDevice->SetVertexShader( NULL ) );
        D3D_HR( d3dDevice->SetPixelShader( NULL ) );
        D3D_HR( d3dDevice->SetFVF( D3DFVF_XYZRHW | D3DFVF_DIFFUSE ) );

        D3D_HR( d3dDevice->Clear( 0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0 ) );
        D3D_HR( d3dDevice->DrawPrimitiveUP( (D3DPRIMITIVETYPE) PRIMITIVE_TRIANGLELIST, 2, (void*) vb, sizeof( VertexUP ) ) );

        D3D_HR( d3dDevice->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL ) );
        D3D_HR( d3dDevice->SetRenderState( D3DRS_STENCILREF, 0 ) );
        D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE2X ) );
    }

    // Create or resize vertex buffer
    if( !vbPoints || count > vbPointsSize )
    {
        SAFEREL( vbPoints );
        vbPointsSize = 0;
        D3D_HR( d3dDevice->CreateVertexBuffer( count * sizeof( MYVERTEX_PRIMITIVE ), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_MYVERTEX_PRIMITIVE, D3DPOOL_DEFAULT, &vbPoints, NULL ) );
        vbPointsSize = count;
    }

    // Copy data
    void* vertices;
    D3D_HR( vbPoints->Lock( 0, count * sizeof( MYVERTEX_PRIMITIVE ), (void**) &vertices, D3DLOCK_DISCARD ) );
    for( uint i = 0, j = (uint) points.size(); i < j; i++ )
    {
        PrepPoint&          point = points[ i ];
        MYVERTEX_PRIMITIVE* vertex = (MYVERTEX_PRIMITIVE*) vertices + i;
        vertex->x = (float) point.PointX;
        vertex->y = (float) point.PointY;
        if( point.PointOffsX )
            vertex->x += (float) *point.PointOffsX;
        if( point.PointOffsY )
            vertex->y += (float) *point.PointOffsY;
        if( zoom )
        {
            vertex->x /= *zoom;
            vertex->y /= *zoom;
        }
        if( offset )
        {
            vertex->x += offset->X;
            vertex->y += offset->Y;
        }
        vertex->Diffuse = COLOR_FIX( point.PointColor );
        vertex->z = 0.0f;
        vertex->rhw = 1.0f;
    }
    D3D_HR( vbPoints->Unlock() );

    // Calculate primitive count
    switch( prim )
    {
    case PRIMITIVE_POINTLIST:
        break;
    case PRIMITIVE_LINELIST:
        count /= 2;
        break;
    case PRIMITIVE_LINESTRIP:
        count -= 1;
        break;
    case PRIMITIVE_TRIANGLELIST:
        count /= 3;
        break;
    case PRIMITIVE_TRIANGLESTRIP:
        count -= 2;
        break;
    case PRIMITIVE_TRIANGLEFAN:
        count -= 2;
        break;
    default:
        break;
    }
    if( count <= 0 )
        return false;

    D3D_HR( d3dDevice->SetStreamSource( 0, vbPoints, 0, sizeof( MYVERTEX_PRIMITIVE ) ) );
    D3D_HR( d3dDevice->SetFVF( D3DFVF_MYVERTEX_PRIMITIVE ) );

    if( sprDefaultEffect[ DEFAULT_EFFECT_POINT ] )
    {
        // Draw with effect
        Effect*      effect = sprDefaultEffect[ DEFAULT_EFFECT_POINT ];
        LPD3DXEFFECT dxeffect = effect->DXInstance;

        if( effect->EffectParams )
            D3D_HR( dxeffect->ApplyParameterBlock( effect->EffectParams ) );
        D3D_HR( dxeffect->SetTechnique( effect->TechniqueSimple ) );
        if( effect->IsNeedProcess )
            GraphicLoader::EffectProcessVariables( effect, -1 );

        UINT passes;
        D3D_HR( dxeffect->Begin( &passes, effect->EffectFlags ) );
        for( UINT pass = 0; pass < passes; pass++ )
        {
            if( effect->IsNeedProcess )
                GraphicLoader::EffectProcessVariables( effect, pass );

            D3D_HR( dxeffect->BeginPass( pass ) );
            D3D_HR( d3dDevice->DrawPrimitive( (D3DPRIMITIVETYPE) prim, 0, count ) );
            D3D_HR( dxeffect->EndPass() );
        }
        D3D_HR( dxeffect->End() );
    }
    else
    {
        D3D_HR( d3dDevice->SetVertexShader( NULL ) );
        D3D_HR( d3dDevice->SetPixelShader( NULL ) );
        D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE ) );

        D3D_HR( d3dDevice->DrawPrimitive( (D3DPRIMITIVETYPE) prim, 0, count ) );

        D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE2X ) );
    }

    if( stencil )
        D3D_HR( d3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE ) );

    #else
    // Check primitives
    uint   count = points.size();
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
    {
        uint mulpos = 0;
        vBuffer[ mulpos ].x = stencil->L;
        vBuffer[ mulpos ].y = stencil->B;
        vBuffer[ mulpos++ ].diffuse = 0xFFFFFFFF;
        vBuffer[ mulpos ].x = stencil->L;
        vBuffer[ mulpos ].y = stencil->T;
        vBuffer[ mulpos++ ].diffuse = 0xFFFFFFFF;
        vBuffer[ mulpos ].x = stencil->R;
        vBuffer[ mulpos ].y = stencil->T;
        vBuffer[ mulpos++ ].diffuse = 0xFFFFFFFF;
        vBuffer[ mulpos ].x = stencil->R;
        vBuffer[ mulpos ].y = stencil->B;
        vBuffer[ mulpos++ ].diffuse = 0xFFFFFFFF;

        GL( glEnable( GL_STENCIL_TEST ) );
        GL( glStencilFunc( GL_NEVER, 1, 0xFF ) );
        GL( glStencilOp( GL_REPLACE, GL_KEEP, GL_KEEP ) );

        curSprCnt = 1;
        dipQueue.push_back( DipData( NULL, sprDefaultEffect[ DEFAULT_EFFECT_FLUSH_COLOR ] ) );
        Flush();

        GL( glStencilFunc( GL_NOTEQUAL, 0, 0xFF ) );
        GL( glStencilOp( GL_REPLACE, GL_KEEP, GL_KEEP ) );
    }

    // Resize buffers
    if( vBuffer.size() < count )
    {
        // Vertex buffer
        vBuffer.resize( count );
        GL( glBindBuffer( GL_ARRAY_BUFFER, vbMain ) );
        GL( glBufferData( GL_ARRAY_BUFFER, count * sizeof( Vertex ), NULL, GL_DYNAMIC_DRAW ) );

        // Index buffers
        ushort* ind = new ushort[ count ];
        for( uint i = 0; i < count; i++ )
            ind[ i ] = i;
        GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibDirect ) );
        GL( glBufferData( GL_ELEMENT_ARRAY_BUFFER, count * sizeof( ushort ), ind, GL_DYNAMIC_DRAW ) );
        delete[] ind;
    }

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

        vBuffer[ i ].x = x;
        vBuffer[ i ].y = y;
        vBuffer[ i ].diffuse = COLOR_FIX( point.PointColor );
    }

    // Draw
    GL( glBindBuffer( GL_ARRAY_BUFFER, vbMain ) );
    GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibDirect ) );
    GL( glBufferSubData( GL_ARRAY_BUFFER, 0, count * sizeof( Vertex ), &vBuffer[ 0 ] ) );

    Effect* effect = sprDefaultEffect[ DEFAULT_EFFECT_POINT ];
    GL( glUseProgram( effect->Program ) );

    if( effect->ProjectionMatrix != -1 )
        GL( glUniformMatrix4fv( effect->ProjectionMatrix, 1, GL_FALSE, projectionMatrix ) );

    if( effect->IsNeedProcess )
        GraphicLoader::EffectProcessVariables( effect, -1 );
    for( uint pass = 0; pass < effect->Passes; pass++ )
    {
        if( effect->IsNeedProcess )
            GraphicLoader::EffectProcessVariables( effect, pass );
        GL( glDrawElements( prim_type, count, GL_UNSIGNED_SHORT, (void*) 0 ) );
    }

    GL( glUseProgram( 0 ) );

    GL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibMain ) );

    // Finish
    if( stencil )
    {
        ClearRenderTargetDS( rtMain, false, true );
        GL( glDisable( GL_STENCIL_TEST ) );
    }
    #endif
    return true;
}

bool SpriteManager::Render3d( int x, int y, float scale, Animation3d* anim3d, RectF* stencil, uint color )
{
    // Draw model in another render target
    #ifdef FO_D3D
    Flush();
    anim3d->Draw( x, y, scale, stencil, color );
    #else
    // Set render target
    if( rt3DMS.FBO )
    {
        PushRenderTarget( rt3DMS );
        ClearRenderTarget( rt3DMS, 0 );
        ClearRenderTargetDS( rt3DMS, true, stencil ? true : false );
    }
    else
    {
        PushRenderTarget( rt3D );
        ClearRenderTarget( rt3D, 0 );
        ClearRenderTargetDS( rt3D, true, stencil ? true : false );
    }

    // Draw model
    GL( glDisable( GL_BLEND ) );
    anim3d->Draw( x, y, scale, stencil, rtStack.back()->FBO );
    GL( glEnable( GL_BLEND ) );

    // Restore render target
    PopRenderTarget();

    // Copy from multisampled texture to default
    Point pivot;
    Rect  borders = anim3d->GetExtraBorders( &pivot );
    if( rt3DMS.FBO )
    {
        PushRenderTarget( rt3D );
        ClearRenderTarget( rt3D, 0 );
        GL( glDisable( GL_BLEND ) );
        DrawRenderTarget( rt3DMS, &borders );
        GL( glEnable( GL_BLEND ) );
        PopRenderTarget();
    }

    // Fill sprite info
    SpriteInfo* si = sprData[ anim3d->GetSprId() ];
    si->Surf->TextureOwner = rt3D.TargetTexture;
    si->Surf->Width = rt3D.TargetTexture->Width;
    si->Surf->Height = rt3D.TargetTexture->Height;
    int pivx = ( borders.L < 0 ? -borders.L : 0 );
    int pivy = ( borders.T < 0 ? -borders.T : 0 );
    borders.L = CLAMP( borders.L, 0, modeWidth );
    borders.T = CLAMP( borders.T, 0, modeHeight );
    borders.R = CLAMP( borders.R, 0, modeWidth );
    borders.B = CLAMP( borders.B, 0, modeHeight );
    si->Width = borders.W() - 1;
    si->Height = borders.H() - 1;
    si->SprRect(
        (float) borders.L / rt3D.TargetTexture->SizeData[ 0 ],
        1.0f - (float) borders.T / rt3D.TargetTexture->SizeData[ 1 ],
        (float) borders.R / rt3D.TargetTexture->SizeData[ 0 ],
        1.0f - (float) borders.B / rt3D.TargetTexture->SizeData[ 1 ] );
    si->OffsX = si->Width / 2 - pivot.X + pivx;
    si->OffsY = si->Height - pivot.Y + pivy;
    #endif

    #ifdef FO_D3D
    // Restore 2d stream
    D3D_HR( d3dDevice->SetIndices( ibMain ) );
    D3D_HR( d3dDevice->SetStreamSource( 0, vbMain, 0, sizeof( Vertex ) ) );
    #endif
    return true;
}

bool SpriteManager::Render3dSize( RectF rect, bool stretch_up, bool center, Animation3d* anim3d, RectF* stencil, uint color )
{
    // Data
    Point xy;
    Rect  borders = anim3d->GetBaseBorders( &xy );
    float w_real = (float) borders.W();
    float h_real = (float) borders.H();
    float scale = min( rect.W() / w_real, rect.H() / h_real );
    if( scale > 1.0f && !stretch_up )
        scale = 1.0f;
    if( center )
    {
        xy.X += (int) ( ( rect.W() - w_real * scale ) / 2.0f );
        xy.Y += (int) ( ( rect.H() - h_real * scale ) / 2.0f );
    }

    // Draw model
    #ifdef FO_D3D
    Flush();
    anim3d->Draw( (int) ( rect.L + (float) xy.X * scale ), (int) ( rect.T + (float) xy.Y * scale ), scale, stencil, color );
    #else
    Render3d( (int) ( rect.L + (float) xy.X * scale ), (int) ( rect.T + (float) xy.Y * scale ), scale, anim3d, stencil, 0 );
    #endif

    #ifdef FO_D3D
    // Restore 2d stream
    D3D_HR( d3dDevice->SetIndices( ibMain ) );
    D3D_HR( d3dDevice->SetStreamSource( 0, vbMain, 0, sizeof( Vertex ) ) );
    #endif
    return true;
}

bool SpriteManager::Draw3d( int x, int y, float scale, Animation3d* anim3d, RectF* stencil, uint color )
{
    Render3d( x, y, scale, anim3d, stencil, color );
    #ifndef FO_D3D
    DrawRenderTarget( rt3D );
    #endif
    return true;
}

bool SpriteManager::Draw3dSize( RectF rect, bool stretch_up, bool center, Animation3d* anim3d, RectF* stencil, uint color )
{
    Render3dSize( rect, stretch_up, center, anim3d, stencil, color );
    #ifndef FO_D3D
    DrawRenderTarget( rt3D );
    #endif
    return true;
}

bool SpriteManager::DrawContours()
{
    #ifdef FO_D3D
    if( contoursPS && contoursAdded )
    {
        struct VertexUP
        {
            FLOAT x, y, z, rhw;
            float tu, tv;
        } vb[ 6 ] =
        {
            # ifdef FO_D3D
            { -0.5f, (float) modeHeight - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
            { -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
            { (float) modeWidth - 0.5f, (float) modeHeight - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
            { -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
            { (float) modeWidth - 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
            { (float) modeWidth - 0.5f, (float) modeHeight - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
            # else
            { -0.0f, (float) modeHeight, 0.0f, 1.0f, 0.0f, 1.0f },
            { -0.0f, -0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
            { (float) modeWidth, (float) modeHeight, 0.0f, 1.0f, 1.0f, 1.0f },
            { -0.0f, -0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
            { (float) modeWidth, -0.0f, 0.0f, 1.0f, 1.0f, 0.0f },
            { (float) modeWidth, (float) modeHeight, 0.0f, 1.0f, 1.0f, 1.0f },
            # endif
        };

        D3D_HR( d3dDevice->SetVertexShader( NULL ) );
        D3D_HR( d3dDevice->SetPixelShader( NULL ) );
        D3D_HR( d3dDevice->SetFVF( D3DFVF_XYZRHW | D3DFVF_TEX1 ) );
        D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 ) );
        D3D_HR( d3dDevice->SetTexture( 0, contoursTexture->Instance ) );

        D3D_HR( d3dDevice->DrawPrimitiveUP( (D3DPRIMITIVETYPE) PRIMITIVE_TRIANGLELIST, 2, (void*) vb, sizeof( VertexUP ) ) );

        D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE2X ) );
        contoursAdded = false;
    }
    else if( spriteContours.Size() )
    {
        D3D_HR( d3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT ) );   // Zoom In
        SetCurEffect2D( DEFAULT_EFFECT_GENERIC );
        DrawSprites( spriteContours, false, false, 0, 0 );
        D3D_HR( d3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR ) );  // Zoom In
        spriteContours.Unvalidate();
    }
    #else
    if( contoursAdded )
    {
        DrawRenderTarget( rtContours );
        ClearRenderTarget( rtContours, 0 );
        ClearRenderTarget( rtContoursMid, 0 );
        contoursAdded = false;
    }
    #endif
    return true;
}

bool SpriteManager::CollectContour( int x, int y, SpriteInfo* si, Sprite* spr )
{
    #ifdef FO_D3D
    if( !contoursPS )
    {
        if( !si->Anim3d )
        {
            uint contour_id = GetSpriteContour( si, spr );
            if( contour_id )
            {
                Sprite& contour_spr = spriteContours.AddSprite( 0, 0, 0, 0, spr->ScrX, spr->ScrY, contour_id, NULL, spr->OffsX, spr->OffsY, NULL, NULL );
                if( spr->ContourType == CONTOUR_RED )
                {
                    contour_spr.SetFlash( 0xFFFF0000 );
                    contour_spr.SetColor( 0xFFAFAFAF );
                }
                else if( spr->ContourType == CONTOUR_YELLOW )
                {
                    contour_spr.SetFlash( 0xFFFFFF00 );
                    contour_spr.SetColor( 0xFFAFAFAF );
                }
                else
                {
                    contour_spr.SetFlash( 0xFFFFFFFF );
                    contour_spr.SetColor( spr->ContourColor );
                }
                return true;
            }
        }

        return false;
    }

    // Shader contour
    Animation3d* anim3d = si->Anim3d;
    Rect         borders = ( anim3d ? anim3d->GetExtraBorders() : Rect( x - 1, y - 1, x + si->Width + 1, y + si->Height + 1 ) );
    Texture*     texture = ( anim3d ? contoursMidTexture : si->Surf->TextureOwner );
    float        ws, hs;
    RectF        tuv, tuvh;

    if( !anim3d )
    {
        if( borders.L >= modeWidth * GameOpt.SpritesZoom || borders.R < 0 || borders.T >= modeHeight * GameOpt.SpritesZoom || borders.B < 0 )
            return true;

        if( GameOpt.SpritesZoom == 1.0f )
        {
            ws = 1.0f / (float) si->Surf->Width;
            hs = 1.0f / (float) si->Surf->Height;
            tuv = RectF( si->SprRect.L - ws, si->SprRect.T - hs, si->SprRect.R + ws, si->SprRect.B + hs );
            tuvh = tuv;
        }
        else
        {
            borders( (int) ( x / GameOpt.SpritesZoom ), (int) ( y / GameOpt.SpritesZoom ),
                     (int) ( ( x + si->Width ) / GameOpt.SpritesZoom ), (int) ( ( y + si->Height ) / GameOpt.SpritesZoom ) );
            struct VertexUP
            {
                FLOAT x, y, z, rhw;
                FLOAT tu, tv;
            } vb[ 6 ] =
            {
                { (float) borders.L, (float) borders.B, 1.0f, 1.0f, si->SprRect.L, si->SprRect.B },
                { (float) borders.L, (float) borders.T, 1.0f, 1.0f, si->SprRect.L, si->SprRect.T },
                { (float) borders.R, (float) borders.B, 1.0f, 1.0f, si->SprRect.R, si->SprRect.B },
                { (float) borders.L, (float) borders.T, 1.0f, 1.0f, si->SprRect.L, si->SprRect.T },
                { (float) borders.R, (float) borders.T, 1.0f, 1.0f, si->SprRect.R, si->SprRect.T },
                { (float) borders.R, (float) borders.B, 1.0f, 1.0f, si->SprRect.R, si->SprRect.B },
            };

            borders.L--;
            borders.T--;
            borders.R++;
            borders.B++;

            LPDIRECT3DSURFACE9 old_rt, old_ds;
            D3D_HR( d3dDevice->GetRenderTarget( 0, &old_rt ) );
            D3D_HR( d3dDevice->GetDepthStencilSurface( &old_ds ) );
            D3D_HR( d3dDevice->SetDepthStencilSurface( NULL ) );
            D3D_HR( d3dDevice->SetRenderTarget( 0, contoursMidTextureSurf ) );
            D3D_HR( d3dDevice->SetVertexShader( NULL ) );
            D3D_HR( d3dDevice->SetPixelShader( NULL ) );
            D3D_HR( d3dDevice->SetFVF( D3DFVF_XYZRHW | D3DFVF_TEX1 ) );
            D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 ) );
            D3D_HR( d3dDevice->SetTexture( 0, si->Surf->TextureOwner->Instance ) );

            D3DRECT clear_r = { borders.L - 1, borders.T - 1, borders.R + 1, borders.B + 1 };
            D3D_HR( d3dDevice->Clear( 1, &clear_r, D3DCLEAR_TARGET, 0, 1.0f, 0 ) );
            D3D_HR( d3dDevice->DrawPrimitiveUP( (D3DPRIMITIVETYPE) PRIMITIVE_TRIANGLELIST, 2, (void*) vb, sizeof( VertexUP ) ) );

            D3D_HR( d3dDevice->SetRenderTarget( 0, old_rt ) );
            D3D_HR( d3dDevice->SetDepthStencilSurface( old_ds ) );
            old_rt->Release();
            old_ds->Release();

            float w = (float) modeWidth;
            float h = (float) modeHeight;
            ws = 1.0f / modeWidth;
            hs = 1.0f / modeHeight;
            tuv = RectF( (float) borders.L / w, (float) borders.T / h, (float) borders.R / w, (float) borders.B / h );
            tuvh = tuv;
            texture = contoursMidTexture;
        }
    }
    else
    {
        if( borders.L >= modeWidth || borders.R < 0 || borders.T >= modeHeight || borders.B < 0 )
            return true;

        Rect init_borders = borders;
        if( borders.L <= 0 )
            borders.L = 1;
        if( borders.T <= 0 )
            borders.T = 1;
        if( borders.R >= modeWidth )
            borders.R = modeWidth - 1;
        if( borders.B >= modeHeight )
            borders.B = modeHeight - 1;

        float w = (float) modeWidth;
        float h = (float) modeHeight;
        tuv.L = (float) borders.L / w;
        tuv.T = (float) borders.T / h;
        tuv.R = (float) borders.R / w;
        tuv.B = (float) borders.B / h;
        tuvh.T = (float) init_borders.T / h;
        tuvh.B = (float) init_borders.B / h;

        ws = 0.1f / modeWidth;
        hs = 0.1f / modeHeight;

        // Render to contours texture
        struct VertexUP
        {
            FLOAT x, y, z, rhw;
            uint  diffuse;
        } vb[ 6 ] =
        {
            # ifdef FO_D3D
            { (float) borders.L - 0.5f, (float) borders.B - 0.5f, 0.99999f, 1.0f, 0xFFFF00FF },
            { (float) borders.L - 0.5f, (float) borders.T - 0.5f, 0.99999f, 1.0f, 0xFFFF00FF },
            { (float) borders.R - 0.5f, (float) borders.B - 0.5f, 0.99999f, 1.0f, 0xFFFF00FF },
            { (float) borders.L - 0.5f, (float) borders.T - 0.5f, 0.99999f, 1.0f, 0xFFFF00FF },
            { (float) borders.R - 0.5f, (float) borders.T - 0.5f, 0.99999f, 1.0f, 0xFFFF00FF },
            { (float) borders.R - 0.5f, (float) borders.B - 0.5f, 0.99999f, 1.0f, 0xFFFF00FF },
            # else
            { (float) borders.L, (float) borders.B, 0.99999f, 1.0f, 0xFFFF00FF },
            { (float) borders.L, (float) borders.T, 0.99999f, 1.0f, 0xFFFF00FF },
            { (float) borders.R, (float) borders.B, 0.99999f, 1.0f, 0xFFFF00FF },
            { (float) borders.L, (float) borders.T, 0.99999f, 1.0f, 0xFFFF00FF },
            { (float) borders.R, (float) borders.T, 0.99999f, 1.0f, 0xFFFF00FF },
            { (float) borders.R, (float) borders.B, 0.99999f, 1.0f, 0xFFFF00FF },
            # endif
        };

        LPDIRECT3DSURFACE9 old_rt;
        D3D_HR( d3dDevice->GetRenderTarget( 0, &old_rt ) );
        D3D_HR( d3dDevice->SetRenderTarget( 0, contours3dRT ) );

        D3D_HR( d3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE ) );
        D3D_HR( d3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_NOTEQUAL ) );
        D3D_HR( d3dDevice->SetVertexShader( NULL ) );
        D3D_HR( d3dDevice->SetPixelShader( NULL ) );
        D3D_HR( d3dDevice->SetFVF( D3DFVF_XYZRHW | D3DFVF_DIFFUSE ) );
        D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE ) );

        D3DRECT clear_r = { borders.L - 2, borders.T - 2, borders.R + 2, borders.B + 2 };
        D3D_HR( d3dDevice->Clear( 1, &clear_r, D3DCLEAR_TARGET, 0, 1.0f, 0 ) );
        D3D_HR( d3dDevice->DrawPrimitiveUP( (D3DPRIMITIVETYPE) PRIMITIVE_TRIANGLELIST, 2, (void*) vb, sizeof( VertexUP ) ) );

        D3D_HR( d3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE ) );
        D3D_HR( d3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESS ) );
        D3D_HR( d3dDevice->SetRenderTarget( 0, old_rt ) );
        old_rt->Release();

        // Copy to mid surface
        RECT r = { borders.L - 1, borders.T - 1, borders.R + 1, borders.B + 1 };
        D3D_HR( d3dDevice->StretchRect( contours3dRT, &r, contoursMidTextureSurf, &r, D3DTEXF_NONE ) );
    }

    // Calculate contour color
    uint contour_color = 0;
    if( spr->ContourType == CONTOUR_RED )
        contour_color = 0xFFAF0000;
    else if( spr->ContourType == CONTOUR_YELLOW )
    {
        contour_color = 0xFFAFAF00;
        tuvh.T = -1.0f;       // Disable flashing
        tuvh.B = -1.0f;
    }
    else if( spr->ContourType == CONTOUR_CUSTOM )
        contour_color = spr->ContourColor;
    else
        contour_color = 0xFFAFAFAF;

    static float color_offs = 0.0f;
    static uint  last_tick = 0;
    uint         tick = Timer::FastTick();
    if( tick - last_tick >= 50 )
    {
        color_offs -= 0.05f;
        if( color_offs < -1.0f )
            color_offs = 1.0f;
        last_tick = tick;
    }

    // Draw contour
    struct VertexUP
    {
        FLOAT x, y, z, rhw;
        float tu, tv;
    } vb[ 6 ] =
    {
        # ifdef FO_D3D
        { (float) borders.L - 0.5f, (float) borders.B - 0.5f, 0.0f, 1.0f, tuv.L, tuv.B },
        { (float) borders.L - 0.5f, (float) borders.T - 0.5f, 0.0f, 1.0f, tuv.L, tuv.T },
        { (float) borders.R - 0.5f, (float) borders.B - 0.5f, 0.0f, 1.0f, tuv.R, tuv.B },
        { (float) borders.L - 0.5f, (float) borders.T - 0.5f, 0.0f, 1.0f, tuv.L, tuv.T },
        { (float) borders.R - 0.5f, (float) borders.T - 0.5f, 0.0f, 1.0f, tuv.R, tuv.T },
        { (float) borders.R - 0.5f, (float) borders.B - 0.5f, 0.0f, 1.0f, tuv.R, tuv.B },
        # else
        { (float) borders.L, (float) borders.B, 0.0f, 1.0f, tuv.L, tuv.B },
        { (float) borders.L, (float) borders.T, 0.0f, 1.0f, tuv.L, tuv.T },
        { (float) borders.R, (float) borders.B, 0.0f, 1.0f, tuv.R, tuv.B },
        { (float) borders.L, (float) borders.T, 0.0f, 1.0f, tuv.L, tuv.T },
        { (float) borders.R, (float) borders.T, 0.0f, 1.0f, tuv.R, tuv.T },
        { (float) borders.R, (float) borders.B, 0.0f, 1.0f, tuv.R, tuv.B },
        # endif
    };

    LPDIRECT3DSURFACE9 ds;
    D3D_HR( d3dDevice->GetDepthStencilSurface( &ds ) );
    D3D_HR( d3dDevice->SetDepthStencilSurface( NULL ) );
    LPDIRECT3DSURFACE9 old_rt;
    D3D_HR( d3dDevice->GetRenderTarget( 0, &old_rt ) );
    D3D_HR( d3dDevice->SetRenderTarget( 0, contoursTextureSurf ) );
    D3D_HR( d3dDevice->SetTexture( 0, texture->Instance ) );
    D3D_HR( d3dDevice->SetVertexShader( NULL ) );
    D3D_HR( d3dDevice->SetPixelShader( contoursPS ) );
    D3D_HR( d3dDevice->SetFVF( D3DFVF_XYZRHW | D3DFVF_TEX1 ) );
    D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 ) );

    if( contoursConstWidthStep )
        D3D_HR( contoursCT->SetFloat( d3dDevice, contoursConstWidthStep, ws ) );
    if( contoursConstHeightStep )
        D3D_HR( contoursCT->SetFloat( d3dDevice, contoursConstHeightStep, hs ) );
    float sb[ 4 ] = { tuv.L, tuv.T, tuv.R, tuv.B };
    if( contoursConstSpriteBorders )
        D3D_HR( contoursCT->SetFloatArray( d3dDevice, contoursConstSpriteBorders, sb, 4 ) );
    float sbh[ 3 ] = { tuvh.T, tuvh.B, tuvh.B - tuvh.T };
    if( contoursConstSpriteBordersHeight )
        D3D_HR( contoursCT->SetFloatArray( d3dDevice, contoursConstSpriteBordersHeight, sbh, 3 ) );
    float cc[ 4 ] = { float( ( contour_color >> 16 ) & 0xFF ) / 255.0f, float( ( contour_color >> 8 ) & 0xFF ) / 255.0f, float( ( contour_color ) & 0xFF ) / 255.0f, float( ( contour_color >> 24 ) & 0xFF ) / 255.0f };
    if( contoursConstContourColor )
        D3D_HR( contoursCT->SetFloatArray( d3dDevice, contoursConstContourColor, cc, 4 ) );
    if( contoursConstContourColorOffs )
        D3D_HR( contoursCT->SetFloat( d3dDevice, contoursConstContourColorOffs, color_offs ) );

    if( !contoursAdded )
        D3D_HR( d3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 0.9f, 0 ) );
    D3D_HR( d3dDevice->DrawPrimitiveUP( (D3DPRIMITIVETYPE) PRIMITIVE_TRIANGLELIST, 2, (void*) vb, sizeof( VertexUP ) ) );

    // Restore 2d stream
    D3D_HR( d3dDevice->SetDepthStencilSurface( ds ) );
    ds->Release();
    D3D_HR( d3dDevice->SetRenderTarget( 0, old_rt ) );
    old_rt->Release();
    D3D_HR( d3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE2X ) );
    contoursAdded = true;
    #else
    Rect     borders = Rect( x - 1, y - 1, x + si->Width + 1, y + si->Height + 1 );
    Texture* texture = si->Surf->TextureOwner;
    RectF    textureuv, sprite_border;
    float    zoom = ( si->Anim3d ? 1.0f : GameOpt.SpritesZoom );

    if( borders.L >= modeWidth * zoom || borders.R < 0 || borders.T >= modeHeight * zoom || borders.B < 0 )
        return true;

    if( zoom == 1.0f )
    {
        RectF& sr = si->SprRect;
        float  txw = texture->SizeData[ 2 ];
        float  txh = texture->SizeData[ 3 ];
        textureuv( sr.L - txw, sr.T - txh, sr.R + txw, sr.B + txh );
        if( !si->Anim3d )
        {
            sprite_border = textureuv;
        }
        else
        {
            Rect r = si->Anim3d->GetFullBorders();
            r.L -= 5, r.T -= 5, r.R += 5, r.B += 5;
            r.L = CLAMP( r.L, -1, modeWidth + 1 );
            r.T = CLAMP( r.T, -1, modeHeight + 1 );
            r.R = CLAMP( r.R, -1, modeWidth + 1 );
            r.B = CLAMP( r.B, -1, modeHeight + 1 );
            sprite_border(
                (float) r.L / texture->SizeData[ 0 ],
                1.0f - (float) r.T / texture->SizeData[ 1 ],
                (float) r.R / texture->SizeData[ 0 ],
                1.0f - (float) r.B / texture->SizeData[ 1 ] );
        }
    }
    else
    {
        RectF& sr = si->SprRect;
        borders( (int) ( x / zoom ), (int) ( y / zoom ),
                 (int) ( ( x + si->Width ) / zoom ), (int) ( ( y + si->Height ) / zoom ) );
        RectF bordersf( (float) borders.L, (float) borders.T, (float) borders.R, (float) borders.B );
        float mid_height = rtContoursMid.TargetTexture->SizeData[ 1 ];

        PushRenderTarget( rtContoursMid );

        uint mulpos = 0;
        vBuffer[ mulpos ].x = bordersf.L;
        vBuffer[ mulpos ].y = mid_height - bordersf.B;
        vBuffer[ mulpos ].tu = sr.L;
        vBuffer[ mulpos++ ].tv = sr.B;
        vBuffer[ mulpos ].x = bordersf.L;
        vBuffer[ mulpos ].y = mid_height - bordersf.T;
        vBuffer[ mulpos ].tu = sr.L;
        vBuffer[ mulpos++ ].tv = sr.T;
        vBuffer[ mulpos ].x = bordersf.R;
        vBuffer[ mulpos ].y = mid_height - bordersf.T;
        vBuffer[ mulpos ].tu = sr.R;
        vBuffer[ mulpos++ ].tv = sr.T;
        vBuffer[ mulpos ].x = bordersf.R;
        vBuffer[ mulpos ].y = mid_height - bordersf.B;
        vBuffer[ mulpos ].tu = sr.R;
        vBuffer[ mulpos++ ].tv = sr.B;

        curSprCnt = 1;
        dipQueue.push_back( DipData( texture, sprDefaultEffect[ DEFAULT_EFFECT_FLUSH_TEXTURE ] ) );
        Flush();

        PopRenderTarget();

        texture = rtContoursMid.TargetTexture;
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

    RectF pos( (float) borders.L, (float) borders.T, (float) borders.R, (float) borders.B );

    PushRenderTarget( rtContours );

    uint mulpos = 0;
    vBuffer[ mulpos ].x = pos.L;
    vBuffer[ mulpos ].y = pos.B;
    vBuffer[ mulpos ].tu = textureuv.L;
    vBuffer[ mulpos ].tv = textureuv.B;
    vBuffer[ mulpos++ ].diffuse = contour_color;
    vBuffer[ mulpos ].x = pos.L;
    vBuffer[ mulpos ].y = pos.T;
    vBuffer[ mulpos ].tu = textureuv.L;
    vBuffer[ mulpos ].tv = textureuv.T;
    vBuffer[ mulpos++ ].diffuse = contour_color;
    vBuffer[ mulpos ].x = pos.R;
    vBuffer[ mulpos ].y = pos.T;
    vBuffer[ mulpos ].tu = textureuv.R;
    vBuffer[ mulpos ].tv = textureuv.T;
    vBuffer[ mulpos++ ].diffuse = contour_color;
    vBuffer[ mulpos ].x = pos.R;
    vBuffer[ mulpos ].y = pos.B;
    vBuffer[ mulpos ].tu = textureuv.R;
    vBuffer[ mulpos ].tv = textureuv.B;
    vBuffer[ mulpos++ ].diffuse = contour_color;

    curSprCnt = 1;
    dipQueue.push_back( DipData( texture, sprDefaultEffect[ DEFAULT_EFFECT_CONTOUR ] ) );
    dipQueue.back().SpriteBorder = sprite_border;
    Flush();

    PopRenderTarget();
    contoursAdded = true;
    #endif
    return true;
}

#ifdef FO_D3D
uint SpriteManager::GetSpriteContour( SpriteInfo* si, Sprite* spr )
{
    // Find created
    uint spr_id = ( spr->PSprId ? *spr->PSprId : spr->SprId );
    auto it = createdSpriteContours.find( spr_id );
    if( it != createdSpriteContours.end() )
        return ( *it ).second;

    // Create new
    LPDIRECT3DSURFACE9 surf;
    D3D_HR( si->Surf->TextureOwner->Instance->GetSurfaceLevel( 0, &surf ) );
    D3DSURFACE_DESC    desc;
    D3D_HR( surf->GetDesc( &desc ) );
    RECT               r =
    {
        (uint) ( desc.Width * si->SprRect.L ), (uint) ( desc.Height * si->SprRect.T ),
        (uint) ( desc.Width * si->SprRect.R ), (uint) ( desc.Height * si->SprRect.B )
    };
    LockRect_          lr;
    D3D_HR( surf->LockRect( &lr, &r, D3DLOCK_READONLY ) );

    uint sw = si->Width;
    uint sh = si->Height;
    uint iw = sw + 2;
    uint ih = sh + 2;

    // Data for FillSurfaceFromMemory
    uint   size = 12 + ih * iw * 4;
    uchar* data = new (nothrow) uchar[ size ];
    *( (uint*) data + 1 ) = iw;
    *( (uint*) data + 2 ) = ih;
    uint* ptr = (uint*) data + 3 + iw + 1;

    // Write contour
    WriteContour4( ptr, iw, lr, sw, sh, COLOR_XRGB( 0x7F, 0x7F, 0x7F ) );
    D3D_HR( surf->UnlockRect() );
    surf->Release();

    // End
    SpriteInfo* contour_si = new (nothrow) SpriteInfo();
    contour_si->OffsX = si->OffsX;
    contour_si->OffsY = si->OffsY + 1;
    int  st = SurfType;
    SurfType = si->Surf->Type;
    uint result = FillSurfaceFromMemory( contour_si, data, size );
    SurfType = st;
    createdSpriteContours.insert( PAIR( spr_id, result ) );
    return result;
}

# define SET_IMAGE_POINT( x, y )    *( buf + ( y ) * buf_w + ( x ) ) = color
void SpriteManager::WriteContour4( uint* buf, uint buf_w, LockRect_& r, uint w, uint h, uint color )
{
    for( uint y = 0; y < h; y++ )
    {
        for( uint x = 0; x < w; x++ )
        {
            uint p = SURF_POINT( r, x, y );
            if( !p )
                continue;
            if( !x || !SURF_POINT( r, x - 1, y ) )
                SET_IMAGE_POINT( x - 1, y );
            if( x == w - 1 || !SURF_POINT( r, x + 1, y ) )
                SET_IMAGE_POINT( x + 1, y );
            if( !y || !SURF_POINT( r, x, y - 1 ) )
                SET_IMAGE_POINT( x, y - 1 );
            if( y == h - 1 || !SURF_POINT( r, x, y + 1 ) )
                SET_IMAGE_POINT( x, y + 1 );
        }
    }
}

void SpriteManager::WriteContour8( uint* buf, uint buf_w, LockRect_& r, uint w, uint h, uint color )
{
    for( uint y = 0; y < h; y++ )
    {
        for( uint x = 0; x < w; x++ )
        {
            uint p = SURF_POINT( r, x, y );
            if( !p )
                continue;
            if( !x || !SURF_POINT( r, x - 1, y ) )
                SET_IMAGE_POINT( x - 1, y );
            if( x == w - 1 || !SURF_POINT( r, x + 1, y ) )
                SET_IMAGE_POINT( x + 1, y );
            if( !y || !SURF_POINT( r, x, y - 1 ) )
                SET_IMAGE_POINT( x, y - 1 );
            if( y == h - 1 || !SURF_POINT( r, x, y + 1 ) )
                SET_IMAGE_POINT( x, y + 1 );
            if( ( !x && !y ) || !SURF_POINT( r, x - 1, y - 1 ) )
                SET_IMAGE_POINT( x - 1, y - 1 );
            if( ( x == w - 1 && !y ) || !SURF_POINT( r, x + 1, y - 1 ) )
                SET_IMAGE_POINT( x + 1, y - 1 );
            if( ( x == w - 1 && y == h - 1 ) || !SURF_POINT( r, x + 1, y + 1 ) )
                SET_IMAGE_POINT( x + 1, y + 1 );
            if( ( y == h - 1 && !x ) || !SURF_POINT( r, x - 1, y + 1 ) )
                SET_IMAGE_POINT( x - 1, y + 1 );
        }
    }
}
#endif
