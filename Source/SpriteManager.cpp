#include "StdAfx.h"
#include "SpriteManager.h"
#include "Common.h"
#include "F2Palette.h"

#define TEX_FRMT                  D3DFMT_A8R8G8B8
#define SPR_BUFFER_COUNT          (10000)
#define SPRITES_POOL_GROW_SIZE    (10000)
#define SPRITES_RESIZE_COUNT      (100)
#define SURF_SPRITES_OFFS         (2)
#define SURF_POINT(lr,x,y)        (*((DWORD*)((BYTE*)lr.pBits+lr.Pitch*(y)+(x)*4)))

/************************************************************************/
/* Sprites                                                              */
/************************************************************************/

SpriteVec Sprites::spritesPool;

void Sprite::Unvalidate()
{
	if(Valid)
	{
		if(ValidCallback)
		{
			*ValidCallback=false;
			ValidCallback=NULL;
		}
		Valid=false;
	}
}

void Sprites::GrowPool(size_t size)
{
	spritesPool.reserve(spritesPool.size()+size);
	for(size_t i=0;i<size;i++) spritesPool.push_back(new Sprite());
}

void Sprites::ClearPool()
{
	for(SpriteVecIt it=spritesPool.begin(),end=spritesPool.end();it!=end;++it)
	{
		Sprite* spr=*it;
		spr->Unvalidate();
		delete spr;
	}
	spritesPool.clear();
}

Sprite& Sprites::PutSprite(size_t index, DWORD map_pos, int x, int y, DWORD id, DWORD* id_ptr, short* ox, short* oy, BYTE* alpha, BYTE* light, bool* callback)
{
	if(index>=spritesTreeSize)
	{
		spritesTreeSize=index+1;
		if(spritesTreeSize>=spritesTree.size()) Resize(spritesTreeSize+SPRITES_RESIZE_COUNT);
	}
	Sprite* spr=spritesTree[index];
	spr->MapPos=map_pos;
	spr->MapPosInd=index;
	spr->ScrX=x;
	spr->ScrY=y;
	spr->SprId=id;
	spr->PSprId=id_ptr;
	spr->OffsX=ox;
	spr->OffsY=oy;
	spr->Alpha=alpha;
	spr->Light=light;
	spr->Valid=true;
	spr->ValidCallback=callback;
	if(callback) *callback=true;
	spr->Egg=Sprite::EggNone;
	spr->Contour=Sprite::ContourNone;
	spr->ContourColor=0;
	spr->Color=0;
	spr->FlashMask=0;
	return *spr;
}

Sprite& Sprites::AddSprite(DWORD map_pos, int x, int y, DWORD id, DWORD* id_ptr, short* ox, short* oy, BYTE* alpha, BYTE* light, bool* callback)
{
	return PutSprite(spritesTreeSize,map_pos,x,y,id,id_ptr,ox,oy,alpha,light,callback);
}

Sprite& Sprites::InsertSprite(DWORD map_pos, int x, int y, DWORD id, DWORD* id_ptr, short* ox, short* oy, BYTE* alpha, BYTE* light, bool* callback)
{
	size_t index=0;
	for(SpriteVecIt it=spritesTree.begin(),end=spritesTree.begin()+spritesTreeSize;it!=end;++it)
	{
		Sprite* spr=*it;
		if(spr->MapPos>map_pos) break;
		index++;
	}
	spritesTreeSize++;
	if(spritesTreeSize>=spritesTree.size()) Resize(spritesTreeSize+SPRITES_RESIZE_COUNT);
	if(index<spritesTreeSize-1)
	{
		spritesTree.insert(spritesTree.begin()+index,spritesTree.back());
		spritesTree.pop_back();
	}
	return PutSprite(index,map_pos,x,y,id,id_ptr,ox,oy,alpha,light,callback);
}

void Sprites::Resize(size_t size)
{
	size_t tree_size=spritesTree.size();
	size_t pool_size=spritesPool.size();
	if(size>tree_size) // Get from pool
	{
		size_t diff=size-tree_size;
		if(diff>pool_size) GrowPool(diff>SPRITES_POOL_GROW_SIZE?diff:SPRITES_POOL_GROW_SIZE);
		spritesTree.reserve(tree_size+diff);
		//spritesTree.insert(spritesTree.end(),spritesPool.rbegin(),spritesPool.rbegin()+diff);
		//spritesPool.erase(spritesPool.begin()+tree_size-diff,spritesPool.end());
		for(size_t i=0;i<diff;i++)
		{
			spritesTree.push_back(spritesPool.back());
			spritesPool.pop_back();
		}
	}
	else if(size<tree_size) // Put in pool
	{
		size_t diff=tree_size-size;
		if(diff>tree_size-spritesTreeSize) spritesTreeSize-=diff-(tree_size-spritesTreeSize);

		// Unvalidate putted sprites
		for(SpriteVec::reverse_iterator it=spritesTree.rbegin(),end=spritesTree.rbegin()+diff;it!=end;++it)
			(*it)->Unvalidate();

		// Put
		spritesPool.reserve(pool_size+diff);
		//spritesPool.insert(spritesPool.end(),spritesTree.rbegin(),spritesTree.rbegin()+diff);
		//spritesTree.erase(spritesTree.begin()+tree_size-diff,spritesTree.end());
		for(size_t i=0;i<diff;i++)
		{
			spritesPool.push_back(spritesTree.back());
			spritesTree.pop_back();
		}
	}
}

void Sprites::Unvalidate()
{
	for(SpriteVecIt it=spritesTree.begin(),end=spritesTree.begin()+spritesTreeSize;it!=end;++it)
		(*it)->Unvalidate();
	spritesTreeSize=0;
}

SprInfoVec* SortSpritesSurfSprData=NULL;
bool SortSpritesSurf(Sprite* spr1, Sprite* spr2)
{
	SpriteInfo* si1=(*SortSpritesSurfSprData)[spr1->PSprId?*spr1->PSprId:spr1->SprId];
	SpriteInfo* si2=(*SortSpritesSurfSprData)[spr2->PSprId?*spr2->PSprId:spr2->SprId];
	return si1->Surf && si2->Surf && si1->Surf->Texture<si2->Surf->Texture;
}
void Sprites::SortBySurfaces()
{
	std::sort(spritesTree.begin(),spritesTree.begin()+spritesTreeSize,SortSpritesSurf);
}

bool SortSpritesMapPos(Sprite* spr1, Sprite* spr2)
{
	if(spr1->MapPos==spr2->MapPos) return spr1->MapPosInd<spr2->MapPosInd;
	return spr1->MapPos<spr2->MapPos;
}
void Sprites::SortByMapPos()
{
	std::sort(spritesTree.begin(),spritesTree.begin()+spritesTreeSize,SortSpritesMapPos);
}

/************************************************************************/
/* Sprite manager                                                       */
/************************************************************************/

SpriteManager::SpriteManager():
isInit(0),flushSprCnt(0),curSprCnt(0),hWnd(NULL),direct3D(NULL),SurfType(0),
spr3dRT(NULL),spr3dRTEx(NULL),spr3dDS(NULL),spr3dRTData(NULL),spr3dSurfWidth(256),spr3dSurfHeight(256),sceneBeginned(false),
d3dDevice(NULL),pVB(NULL),pIB(NULL),waitBuf(NULL),vbPoints(NULL),vbPointsSize(0),PreRestore(NULL),PostRestore(NULL),
baseTexture(0),eggSurfWidth(1.0f),eggSurfHeight(1.0f),eggSprWidth(1),eggSprHeight(1),
contoursTexture(NULL),contoursTextureSurf(NULL),contoursMidTexture(NULL),contoursMidTextureSurf(NULL),contours3dRT(NULL),
contoursPS(NULL),contoursCT(NULL),contoursAdded(false),
modeWidth(0),modeHeight(0)
{
	//ZeroMemory(&displayMode,sizeof(displayMode));
	ZeroMemory(&presentParams,sizeof(presentParams));
	ZeroMemory(&mngrParams,sizeof(mngrParams));
	ZeroMemory(&deviceCaps,sizeof(deviceCaps));
	baseColor=D3DCOLOR_ARGB(255,128,128,128);
	surfList.reserve(100);
	dipQueue.reserve(1000);
	SortSpritesSurfSprData=&sprData; // For sprites sorting
	contoursConstWidthStep=NULL;
	contoursConstHeightStep=NULL;
	contoursConstSpriteBorders=NULL;
	contoursConstSpriteBordersHeight=NULL;
	contoursConstContourColor=NULL;
	contoursConstContourColorOffs=NULL;
	ZeroMemory(sprDefaultEffect,sizeof(sprDefaultEffect));
	curDefaultEffect=NULL;
}

bool SpriteManager::Init(SpriteMngrParams& params)
{
	if(isInit) return false;
	WriteLog("Sprite manager initialization...\n");

	mngrParams=params;
	PreRestore=params.PreRestoreFunc;
	PostRestore=params.PostRestoreFunc;
	flushSprCnt=GameOpt.FlushVal;
	baseTexture=GameOpt.BaseTexture;
	modeWidth=GameOpt.ScreenWidth;
	modeHeight=GameOpt.ScreenHeight;
	curSprCnt=0;

	direct3D=Direct3DCreate(D3D_SDK_VERSION);
	if(!direct3D)
	{
		WriteLog("Create Direct3D fail.\n");
		return false;
	}

	//ZeroMemory(&displayMode,sizeof(displayMode));
	//D3D_HR(direct3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,&displayMode));
	ZeroMemory(&deviceCaps,sizeof(deviceCaps));
	D3D_HR(direct3D->GetDeviceCaps(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,&deviceCaps));

	ZeroMemory(&presentParams,sizeof(presentParams));
	presentParams.BackBufferCount=1;
	presentParams.Windowed=(GameOpt.FullScreen?FALSE:TRUE);
	presentParams.SwapEffect=D3DSWAPEFFECT_DISCARD;
	presentParams.EnableAutoDepthStencil=TRUE;
	presentParams.AutoDepthStencilFormat=D3DFMT_D24S8;
	presentParams.hDeviceWindow=params.WndHeader;
	presentParams.BackBufferWidth=modeWidth;
	presentParams.BackBufferHeight=modeHeight;
	presentParams.BackBufferFormat=D3DFMT_X8R8G8B8;
	if(!GameOpt.VSync) presentParams.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;

	presentParams.MultiSampleType=(D3DMULTISAMPLE_TYPE)GameOpt.MultiSampling;
	if(GameOpt.MultiSampling<0)
	{
		presentParams.MultiSampleType=D3DMULTISAMPLE_NONE;
		for(int i=4;i>=1;i--)
		{
			if(SUCCEEDED(direct3D->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,
				presentParams.BackBufferFormat,!GameOpt.FullScreen,(D3DMULTISAMPLE_TYPE)i,NULL)))
			{
				presentParams.MultiSampleType=(D3DMULTISAMPLE_TYPE)i;
				break;
			}
		}
	}
	if(presentParams.MultiSampleType!=D3DMULTISAMPLE_NONE)
	{
		HRESULT hr=direct3D->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,presentParams.BackBufferFormat,!GameOpt.FullScreen,
			presentParams.MultiSampleType,&presentParams.MultiSampleQuality);
		if(FAILED(hr))
		{
			WriteLog("Multisampling %dx not supported. Disabled.\n",(int)presentParams.MultiSampleType);
			presentParams.MultiSampleType=D3DMULTISAMPLE_NONE;
			presentParams.MultiSampleQuality=0;
		}
		if(presentParams.MultiSampleQuality) presentParams.MultiSampleQuality--;
	}

	int vproc=D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	if(!GameOpt.SoftwareSkinning && deviceCaps.DevCaps&D3DDEVCAPS_HWTRANSFORMANDLIGHT &&
		deviceCaps.VertexShaderVersion>=D3DPS_VERSION(2,0) && deviceCaps.MaxVertexBlendMatrices>=2)
		vproc=D3DCREATE_HARDWARE_VERTEXPROCESSING;

	D3D_HR(direct3D->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,params.WndHeader,vproc,&presentParams,&d3dDevice));

	// Contours
	if(deviceCaps.PixelShaderVersion>=D3DPS_VERSION(2,0))
	{
		// Contours shader
		ID3DXBuffer* shader=NULL,*errors=NULL;
		HRESULT hr=D3DXCompileShaderFromResource(NULL,MAKEINTRESOURCE(IDR_PS_CONTOUR),NULL,NULL,"Main","ps_2_0",D3DXSHADER_SKIPVALIDATION,&shader,&errors,&contoursCT);
		if(SUCCEEDED(hr))
		{
			hr=d3dDevice->CreatePixelShader((DWORD*)shader->GetBufferPointer(),&contoursPS);
			shader->Release();
			if(FAILED(hr))
			{
				WriteLog(__FUNCTION__" - Can't create contours pixel shader, error<%s>. Used old style contours.\n",DXGetErrorString(hr));
				contoursPS=NULL;
			}
		}
		else
		{
			if(errors) WriteLog(__FUNCTION__" - Shader 2d contours compilation messages:\n<\n%s>\n",errors->GetBufferPointer());
			WriteLog(__FUNCTION__" - Shader 2d contours compilation fail, error<%s>. Used old style contours.\n",DXGetErrorString(hr));
		}
		SAFEREL(errors);

		if(contoursPS)
		{
			contoursConstWidthStep=contoursCT->GetConstantByName(NULL,"WidthStep");
			contoursConstHeightStep=contoursCT->GetConstantByName(NULL,"HeightStep");
			contoursConstSpriteBorders=contoursCT->GetConstantByName(NULL,"SpriteBorders");
			contoursConstSpriteBordersHeight=contoursCT->GetConstantByName(NULL,"SpriteBordersHeight");
			contoursConstContourColor=contoursCT->GetConstantByName(NULL,"ContourColor");
			contoursConstContourColorOffs=contoursCT->GetConstantByName(NULL,"ContourColorOffs");
		}
	}

	if(!Animation3d::StartUp(d3dDevice,GameOpt.SoftwareSkinning)) return false;
	if(!InitRenderStates()) return false;
	if(!InitBuffers()) return false;

	// Sprites buffer
	sprData.resize(SPR_BUFFER_COUNT);
	for(SprInfoVecIt it=sprData.begin(),end=sprData.end();it!=end;++it) (*it)=NULL;

	// Transparent egg
	isInit=true;
	eggValid=false;
	eggX=0;
	eggY=0;

	DWORD egg_spr_id=LoadSprite("egg.png",PT_ART_MISC,0);
	if(egg_spr_id)
	{
		sprEgg=GetSpriteInfo(egg_spr_id);
		eggSurfWidth=(float)surfList[0]->Width; // First added surface
		eggSurfHeight=(float)surfList[0]->Height;
		eggSprWidth=sprEgg->Width;
		eggSprHeight=sprEgg->Height;
	}
	else
	{
		WriteLog("Load sprite 'egg.png' fail. Egg disabled.\n");
	}

	D3D_HR(d3dDevice->Clear(0,NULL,D3DCLEAR_TARGET,D3DCOLOR_XRGB(0,0,0),1.0f,0));
	WriteLog("Sprite manager initialization complete.\n");
	return true;
}

bool SpriteManager::InitBuffers()
{
	SAFEREL(spr3dRT);
	SAFEREL(spr3dRTEx);
	SAFEREL(spr3dDS);
	SAFEREL(spr3dRTData);
	SAFEDELA(waitBuf);
	SAFEREL(pVB);
	SAFEREL(pIB);
	SAFEREL(contoursTexture);
	SAFEREL(contoursTextureSurf);
	SAFEREL(contoursMidTexture);
	SAFEREL(contoursMidTextureSurf);
	SAFEREL(contours3dRT);

	// Vertex buffer
	D3D_HR(d3dDevice->CreateVertexBuffer(flushSprCnt*4*sizeof(MYVERTEX),D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC,D3DFVF_MYVERTEX,D3DPOOL_DEFAULT,&pVB,NULL));

	// Index buffer
	D3D_HR(d3dDevice->CreateIndexBuffer(flushSprCnt*6*sizeof(WORD),D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_DEFAULT,&pIB,NULL));

	WORD* ind=new WORD[6*flushSprCnt];
	if(!ind) return false;
	for(int i=0;i<flushSprCnt;i++)
	{
		ind[6*i+0]=4*i+0;
		ind[6*i+1]=4*i+1;
		ind[6*i+2]=4*i+3;
		ind[6*i+3]=4*i+1;
		ind[6*i+4]=4*i+2;
		ind[6*i+5]=4*i+3;
	}

	void* buf;
	D3D_HR(pIB->Lock(0,0,(void**)&buf,0));
	memcpy(buf,ind,flushSprCnt*6*sizeof(WORD));
	D3D_HR(pIB->Unlock());
	delete[] ind;

	waitBuf=new MYVERTEX[flushSprCnt*4];
	if(!waitBuf) return false;

	// Default effects
	curDefaultEffect=Loader3d::LoadEffect(d3dDevice,"2D_Default.fx");
	sprDefaultEffect[DEFAULT_EFFECT_GENERIC]=curDefaultEffect;
	sprDefaultEffect[DEFAULT_EFFECT_TILE]=curDefaultEffect;
	sprDefaultEffect[DEFAULT_EFFECT_ROOF]=curDefaultEffect;
	sprDefaultEffect[DEFAULT_EFFECT_IFACE]=Loader3d::LoadEffect(d3dDevice,"Interface_Default.fx");
	sprDefaultEffect[DEFAULT_EFFECT_POINT]=Loader3d::LoadEffect(d3dDevice,"Primitive_Default.fx");

	// Contours
	if(contoursPS)
	{
		// Contours render target
		D3D_HR(direct3D->CheckDepthStencilMatch(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,D3DFMT_A8R8G8B8,D3DFMT_D24S8));
		D3D_HR(D3DXCreateTexture(d3dDevice,modeWidth,modeHeight,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&contoursTexture));
		D3D_HR(contoursTexture->GetSurfaceLevel(0,&contoursTextureSurf));
		D3D_HR(D3DXCreateTexture(d3dDevice,modeWidth,modeHeight,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&contoursMidTexture));
		D3D_HR(contoursMidTexture->GetSurfaceLevel(0,&contoursMidTextureSurf));
		D3D_HR(d3dDevice->CreateRenderTarget(modeWidth,modeHeight,D3DFMT_A8R8G8B8,presentParams.MultiSampleType,presentParams.MultiSampleQuality,FALSE,&contours3dRT,NULL));
	}

	// 3d models prerendering
	D3D_HR(d3dDevice->CreateRenderTarget(spr3dSurfWidth,spr3dSurfHeight,D3DFMT_A8R8G8B8,presentParams.MultiSampleType,presentParams.MultiSampleQuality,FALSE,&spr3dRT,NULL));
	if(presentParams.MultiSampleType!=D3DMULTISAMPLE_NONE)
		D3D_HR(d3dDevice->CreateRenderTarget(spr3dSurfWidth,spr3dSurfHeight,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&spr3dRTEx,NULL));
	D3D_HR(d3dDevice->CreateDepthStencilSurface(spr3dSurfWidth,spr3dSurfHeight,D3DFMT_D24S8,presentParams.MultiSampleType,presentParams.MultiSampleQuality,TRUE,&spr3dDS,NULL));
	D3D_HR(d3dDevice->CreateOffscreenPlainSurface(spr3dSurfWidth,spr3dSurfHeight,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&spr3dRTData,NULL));

	return true;
}

bool SpriteManager::InitRenderStates()
{
	D3D_HR(d3dDevice->SetRenderState(D3DRS_LIGHTING,FALSE));
	D3D_HR(d3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE));
	D3D_HR(d3dDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL));
	D3D_HR(d3dDevice->SetRenderState(D3DRS_STENCILENABLE,FALSE));
	D3D_HR(d3dDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
	D3D_HR(d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE));
	D3D_HR(d3dDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA));
	D3D_HR(d3dDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA));

	if(deviceCaps.AlphaCmpCaps&D3DPCMPCAPS_GREATEREQUAL)
	{
		D3D_HR(d3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE));
		D3D_HR(d3dDevice->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATEREQUAL));
		D3D_HR(d3dDevice->SetRenderState(D3DRS_ALPHAREF,1));
	}

	D3D_HR(d3dDevice->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));
	D3D_HR(d3dDevice->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR)); // Zoom Out
	D3D_HR(d3dDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR)); // Zoom In

	D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLOROP,  D3DTOP_MODULATE2X));
	D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE));
	D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE));
	D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_ALPHAOP , D3DTOP_MODULATE));
	D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE));
	D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE));
	D3D_HR(d3dDevice->SetTextureStageState(1,D3DTSS_COLOROP , D3DTOP_DISABLE));
	D3D_HR(d3dDevice->SetTextureStageState(1,D3DTSS_COLORARG1,D3DTA_TEXTURE));
	D3D_HR(d3dDevice->SetTextureStageState(1,D3DTSS_COLORARG2,D3DTA_CURRENT));
	D3D_HR(d3dDevice->SetTextureStageState(1,D3DTSS_ALPHAOP , D3DTOP_MODULATE));
	D3D_HR(d3dDevice->SetTextureStageState(1,D3DTSS_ALPHAARG1,D3DTA_TEXTURE));
	D3D_HR(d3dDevice->SetTextureStageState(1,D3DTSS_ALPHAARG2,D3DTA_CURRENT));

	D3D_HR(d3dDevice->SetRenderState(D3DRS_LIGHTING,TRUE));
	D3D_HR(d3dDevice->SetRenderState(D3DRS_DITHERENABLE,TRUE));
	D3D_HR(d3dDevice->SetRenderState(D3DRS_SPECULARENABLE,FALSE));
	//D3D_HR(d3dDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CCW));
	D3D_HR(d3dDevice->SetRenderState(D3DRS_AMBIENT,D3DCOLOR_XRGB(80,80,80)));
	D3D_HR(d3dDevice->SetRenderState(D3DRS_NORMALIZENORMALS,TRUE));

	return true;
}

void SpriteManager::Clear()
{
	WriteLog("Sprite manager finish...\n");

	for(SurfVecIt it=surfList.begin(),end=surfList.end();it!=end;++it) SAFEDEL(*it);
	surfList.clear();
	for(SprInfoVecIt it=sprData.begin(),end=sprData.end();it!=end;++it) SAFEDEL(*it);
	sprData.clear();
	dipQueue.clear();

	Animation3d::Finish();
	SAFEREL(spr3dRT);
	SAFEREL(spr3dRTEx);
	SAFEREL(spr3dDS);
	SAFEREL(spr3dRTData);
	SAFEREL(pVB);
	SAFEREL(pIB);
	SAFEDELA(waitBuf);
	SAFEREL(vbPoints);
	SAFEREL(d3dDevice);
	SAFEREL(contoursTextureSurf);
	SAFEREL(contoursTexture);
	SAFEREL(contoursMidTextureSurf);
	SAFEREL(contoursMidTexture);
	SAFEREL(contours3dRT);
	SAFEREL(contoursCT);
	SAFEREL(contoursPS);
	SAFEREL(direct3D);

	isInit=false;
	WriteLog("Sprite manager finish complete.\n");
}

bool SpriteManager::BeginScene(DWORD clear_color)
{
	HRESULT hr=d3dDevice->TestCooperativeLevel();
	if(hr!=D3D_OK && (hr!=D3DERR_DEVICENOTRESET || !Restore())) return false;

	if(clear_color) D3D_HR(d3dDevice->Clear(0,NULL,D3DCLEAR_TARGET,clear_color,1.0f,0));
	D3D_HR(d3dDevice->BeginScene());
	sceneBeginned=true;
	Animation3d::BeginScene();
	return true;
}

void SpriteManager::EndScene()
{
	Flush();
	d3dDevice->EndScene();
	sceneBeginned=false;
	d3dDevice->Present(NULL,NULL,NULL,NULL);
}

bool SpriteManager::Restore()
{
	if(!isInit) return false;

	// Release resources
	SAFEREL(spr3dRT);
	SAFEREL(spr3dRTEx);
	SAFEREL(spr3dDS);
	SAFEREL(spr3dRTData);
	SAFEREL(pVB);
	SAFEREL(pIB);
	SAFEREL(vbPoints);
	SAFEREL(contoursTexture);
	SAFEREL(contoursTextureSurf);
	SAFEREL(contoursMidTexture);
	SAFEREL(contoursMidTextureSurf);
	SAFEREL(contours3dRT);
	Animation3d::PreRestore();
	if(PreRestore) (*PreRestore)();

	// Reset device
	D3D_HR(d3dDevice->Reset(&presentParams));
	D3D_HR(d3dDevice->Clear(0,NULL,D3DCLEAR_TARGET,D3DCOLOR_XRGB(0,0,0),1.0f,0));

	// Create resources
	if(!InitRenderStates()) return false;
	if(!InitBuffers()) return false;
	if(PostRestore) (*PostRestore)();
	if(!Animation3d::StartUp(d3dDevice,GameOpt.SoftwareSkinning)) return false;

	return true;
}

bool SpriteManager::CreateRenderTarget(LPDIRECT3DSURFACE& surf, int w, int h)
{
	SAFEREL(surf);
	D3D_HR(d3dDevice->CreateRenderTarget(w,h,D3DFMT_X8R8G8B8,presentParams.MultiSampleType,presentParams.MultiSampleQuality,FALSE,&surf,NULL));
	return true;
}

bool SpriteManager::ClearRenderTarget(LPDIRECT3DSURFACE& surf, DWORD color)
{
	if(!surf) return true;

	LPDIRECT3DSURFACE old_rt=NULL,old_ds=NULL;
	D3D_HR(d3dDevice->GetRenderTarget(0,&old_rt));
	D3D_HR(d3dDevice->GetDepthStencilSurface(&old_ds));
	D3D_HR(d3dDevice->SetDepthStencilSurface(NULL));
	D3D_HR(d3dDevice->SetRenderTarget(0,surf));
	D3D_HR(d3dDevice->Clear(0,NULL,D3DCLEAR_TARGET,color,1.0f,0));
	D3D_HR(d3dDevice->SetRenderTarget(0,old_rt));
	D3D_HR(d3dDevice->SetDepthStencilSurface(old_ds));
	old_rt->Release();
	old_ds->Release();
	return true;
}

bool SpriteManager::ClearCurRenderTarget(DWORD color)
{
	D3D_HR(d3dDevice->Clear(0,NULL,D3DCLEAR_TARGET,color,1.0f,0));
	return true;
}

Surface* SpriteManager::CreateNewSurface(int w, int h)
{
	if(!isInit) return NULL;

	// Check power of two
	int ww=w+SURF_SPRITES_OFFS*2;
	w=baseTexture;
	int hh=h+SURF_SPRITES_OFFS*2;
	h=baseTexture;
	while(w<ww) w*=2;
	while(h<hh) h*=2;

	LPDIRECT3DTEXTURE tex=NULL;
	D3D_HR(d3dDevice->CreateTexture(w,h,1,0,TEX_FRMT,D3DPOOL_MANAGED,&tex,NULL));

	Surface* surf=new Surface();
	surf->Type=SurfType;
	surf->Texture=tex;
	surf->Width=w;
	surf->Height=h;
	surf->BusyH=SURF_SPRITES_OFFS;
	surf->FreeX=SURF_SPRITES_OFFS;
	surf->FreeY=SURF_SPRITES_OFFS;
	surfList.push_back(surf);
	return surf;
}

Surface* SpriteManager::FindSurfacePlace(SpriteInfo* si, int& x, int& y)
{
	// Find place in already created surface
	int w=si->Width+SURF_SPRITES_OFFS*2;
	int h=si->Height+SURF_SPRITES_OFFS*2;
	for(SurfVecIt it=surfList.begin(),end=surfList.end();it!=end;++it)
	{
		Surface* surf=*it;
		if(surf->Type==SurfType)
		{
			if(surf->Width-surf->FreeX>=w && surf->Height-surf->FreeY>=h)
			{
				x=surf->FreeX+SURF_SPRITES_OFFS;
				y=surf->FreeY;
				return surf;
			}
			else if(surf->Width>=w && surf->Height-surf->BusyH>=h)
			{
				x=SURF_SPRITES_OFFS;
				y=surf->BusyH+SURF_SPRITES_OFFS;
				return surf;
			}
		}
	}

	// Create new
	Surface* surf=CreateNewSurface(si->Width,si->Height);
	if(!surf) return NULL;
	x=surf->FreeX;
	y=surf->FreeY;
	return surf;
}

void SpriteManager::FreeSurfaces(int surf_type)
{
	for(SurfVecIt it=surfList.begin();it!=surfList.end();)
	{
		Surface* surf=*it;
		if(surf->Type==surf_type)
		{
			for(SprInfoVecIt it_=sprData.begin(),end_=sprData.end();it_!=end_;++it_)
			{
				SpriteInfo* si=*it_;
				if(si && si->Surf==surf)
				{
					delete si;
					(*it_)=NULL;
				}
			}

			delete surf;
			it=surfList.erase(it);
		}
		else ++it;
	}
}

void SpriteManager::SaveSufaces()
{
	static int folder_cnt=0;
	static int rnd_num=0;
	if(!rnd_num) rnd_num=Random(1000,9999);

	int surf_size=0;
	for(SurfVecIt it=surfList.begin(),end=surfList.end();it!=end;++it)
	{
		Surface* surf=*it;
		surf_size+=surf->Width*surf->Height*4;
	}

	char path[256];
	sprintf(path,".\\%d_%03d_%d.%03dmb\\",rnd_num,folder_cnt,surf_size/1000000,surf_size%1000000/1000);
	CreateDirectory(path,NULL);

	int cnt=0;
	char name[256];
	for(SurfVecIt it=surfList.begin(),end=surfList.end();it!=end;++it)
	{
		Surface* surf=*it;
		LPDIRECT3DSURFACE s;
		surf->Texture->GetSurfaceLevel(0,&s);
		sprintf(name,"%s%d_%d_%ux%u.",path,surf->Type,cnt,surf->Width,surf->Height);
		StringAppend(name,"png");
		D3DXSaveSurfaceToFile(name,D3DXIFF_PNG,s,NULL,NULL);
		s->Release();
		cnt++;
	}

	folder_cnt++;
}

DWORD SpriteManager::FillSurfaceFromMemory(SpriteInfo* si, void* data, DWORD size)
{
	// Parameters
	DWORD w,h;
	bool fast=(*(DWORD*)data==MAKEFOURCC('F','0','F','A'));
	if(!si) si=new SpriteInfo();

	// Get width, height
	// FOnline fast format
	if(fast)
	{
		w=*((DWORD*)data+1);
		h=*((DWORD*)data+2);
	}
	// From file in memory
	else
	{
		D3DXIMAGE_INFO img;
		D3D_HR(D3DXGetImageInfoFromFileInMemory(data,size,&img));
		w=img.Width;
		h=img.Height;
	}

	// Find place on surface
	si->Width=w;
	si->Height=h;
	int x,y;
	Surface* surf=FindSurfacePlace(si,x,y);
	if(!surf) return 0;

	LPDIRECT3DSURFACE dst_surf;
	D3DLOCKED_RECT rdst;
	D3D_HR(surf->Texture->GetSurfaceLevel(0,&dst_surf));

	// Copy
	// FOnline fast format
	if(fast)
	{
		RECT r={x-1,y-1,x+w+1,y+h+1};
		D3D_HR(dst_surf->LockRect(&rdst,&r,0));
		BYTE* ptr=(BYTE*)((DWORD*)data+3);
		for(int i=0;i<h;i++) memcpy((BYTE*)rdst.pBits+rdst.Pitch*(i+1)+4,ptr+w*4*i,w*4);
	}
	// From file in memory
	else
	{
		// Try load image
		LPDIRECT3DSURFACE src_surf;
		D3D_HR(d3dDevice->CreateOffscreenPlainSurface(w,h,TEX_FRMT,D3DPOOL_SCRATCH,&src_surf,NULL));
		D3D_HR(D3DXLoadSurfaceFromFileInMemory(src_surf,NULL,NULL,data,size,NULL,D3DX_FILTER_NONE,D3DCOLOR_XRGB(0,0,0xFF),NULL));

		D3DLOCKED_RECT rsrc;
		RECT src_r={0,0,w,h};
		D3D_HR(src_surf->LockRect(&rsrc,&src_r,D3DLOCK_READONLY));

		RECT dest_r={x-1,y-1,x+w+1,y+h+1};
		D3D_HR(dst_surf->LockRect(&rdst,&dest_r,0));

		for(int i=0;i<h;i++) memcpy((BYTE*)rdst.pBits+rdst.Pitch*(i+1)+4,(BYTE*)rsrc.pBits+rsrc.Pitch*i,w*4);

		D3D_HR(src_surf->UnlockRect());
		src_surf->Release();
	}

	if(GameOpt.DebugSprites)
	{
		DWORD rnd_color=D3DCOLOR_XRGB(Random(0,255),Random(0,255),Random(0,255));
		for(DWORD yy=1;yy<h+1;yy++)
		{
			for(DWORD xx=1;xx<w+1;xx++)
			{
				DWORD& p=SURF_POINT(rdst,xx,yy);
				if(p && (!SURF_POINT(rdst,xx-1,yy-1) || !SURF_POINT(rdst,xx,yy-1) || !SURF_POINT(rdst,xx+1,yy-1) ||
					!SURF_POINT(rdst,xx-1,yy) || !SURF_POINT(rdst,xx+1,yy) || !SURF_POINT(rdst,xx-1,yy+1) ||
					!SURF_POINT(rdst,xx,yy+1) || !SURF_POINT(rdst,xx+1,yy+1))) p=rnd_color;
			}
		}
	}

	for(int i=0;i<h+2;i++) SURF_POINT(rdst,0,i)=SURF_POINT(rdst,1,i); // Left
	for(int i=0;i<h+2;i++) SURF_POINT(rdst,w+1,i)=SURF_POINT(rdst,w,i); // Right
	for(int i=0;i<w+2;i++) SURF_POINT(rdst,i,0)=SURF_POINT(rdst,i,1); // Top
	for(int i=0;i<w+2;i++) SURF_POINT(rdst,i,h+1)=SURF_POINT(rdst,i,h); // Bottom

	D3D_HR(dst_surf->UnlockRect());
	dst_surf->Release();

	// Set parameters
	si->Surf=surf;
	surf->FreeX=x+w;
	surf->FreeY=y;
	if(y+h>surf->BusyH) surf->BusyH=y+h;
	si->SprRect.L=float(x)/float(surf->Width);
	si->SprRect.T=float(y)/float(surf->Height);
	si->SprRect.R=float(x+w)/float(surf->Width);
	si->SprRect.B=float(y+h)/float(surf->Height);

	// Store sprite
	size_t index=1;
	for(size_t j=sprData.size();index<j;index++) if(!sprData[index]) break;
	if(index<sprData.size()) sprData[index]=si;
	else sprData.push_back(si);
	return index;
}

DWORD SpriteManager::ReloadSprite(DWORD spr_id, const char* fname, int path_type)
{
	if(!isInit) return spr_id;
	if(!fname || !fname[0]) return spr_id;

	SpriteInfo* si=(spr_id?GetSpriteInfo(spr_id):NULL);
	if(!si)
	{
		spr_id=LoadSprite(fname,path_type);
	}
	else
	{
		for(SurfVecIt it=surfList.begin(),end=surfList.end();it!=end;++it)
		{
			Surface* surf=*it;
			if(si->Surf==surf)
			{
				delete surf;
				surfList.erase(it);
				SAFEDEL(sprData[spr_id]);
				spr_id=LoadSprite(fname,path_type);
				break;
			}
		}
	}

	return spr_id;
}

DWORD SpriteManager::LoadSprite(const char* fname, int path_type, int dir /* = 0 */)
{
	if(!isInit) return 0;
	if(!fname || !fname[0]) return 0;

	const char* ext=FileManager::GetExtension(fname);
	if(!ext)
	{
		WriteLog(__FUNCTION__" - Extension not found, file<%s>.\n",fname);
		return 0;
	}

	if(!_stricmp(ext,"x") || !_stricmp(ext,"3ds") || !_stricmp(ext,"fo3d")) return LoadSprite3d(fname,path_type,dir);
	else if(_stricmp(ext,"frm") && _stricmp(ext,"fr0") && _stricmp(ext,"fr1") && _stricmp(ext,"fr2") &&
		_stricmp(ext,"fr3") && _stricmp(ext,"fr4") && _stricmp(ext,"fr5")) return LoadSpriteAlt(fname,path_type);

	FileManager fm;
	if(!fm.LoadFile(fname,path_type)) return 0;

	SpriteInfo* si=new SpriteInfo();

	short offs_x, offs_y;
	fm.SetCurPos(0xA);
	offs_x=fm.GetBEWord();
	fm.SetCurPos(0x16);
	offs_y=fm.GetBEWord();

	si->OffsX=offs_x;
	si->OffsY=offs_y;

	fm.SetCurPos(0x3E);
	WORD w=fm.GetBEWord();
	WORD h=fm.GetBEWord();

	// Create FOnline fast format
	DWORD size=12+h*w*4;
	BYTE* data=new BYTE[size];
	*((DWORD*)data)=MAKEFOURCC('F','0','F','A'); //FOnline FAst
	*((DWORD*)data+1)=w;
	*((DWORD*)data+2)=h;
	DWORD* ptr=(DWORD*)data+3;
	DWORD* palette=(DWORD*)FoPalette;
	fm.SetCurPos(0x4A);
	for(int i=0,j=w*h;i<j;i++) *(ptr+i)=palette[fm.GetByte()];

	// Fill
	DWORD result=FillSurfaceFromMemory(si,data,size);
	delete[] data;
	return result;
}

DWORD SpriteManager::LoadSpriteAlt(const char* fname, int path_type)
{
	const char* ext=FileManager::GetExtension(fname);
	if(!ext)
	{
		WriteLog(__FUNCTION__" - Unknown extension of file<%s>.\n",fname);
		return 0;
	}

	if(!_stricmp(ext,"fofrm"))
	{
		IniParser fofrm;
		fofrm.LoadFile(fname,path_type);

		char frm_fname[MAX_FOPATH];
		FileManager::ExtractPath(fname,frm_fname);
		char* frm_name=frm_fname+strlen(frm_fname);
		bool is_frm=(fofrm.GetStr("frm","",frm_name) || fofrm.GetStr("frm_0","",frm_name) || fofrm.GetStr("dir_0","frm_0","",frm_name));
		if(!is_frm) return 0;

		DWORD spr_id=LoadSprite(frm_fname,path_type);
		if(!spr_id) return 0;

		SpriteInfo* si=GetSpriteInfo(spr_id);
		si->OffsX+=fofrm.GetInt("offs_x",0);
		si->OffsY+=fofrm.GetInt("offs_y",0);

		if(fofrm.IsKey("effect"))
		{
			char effect_name[MAX_FOPATH];
			if(fofrm.GetStr("effect","",effect_name))
			{
				EffectEx* effect=Loader3d::LoadEffect(d3dDevice,effect_name);
				if(effect) si->Effect=effect;
			}
		}

		return spr_id;
	}
	else if(!_stricmp(ext,"rix"))
	{
		return LoadRix(fname,path_type);
	}

	// Dx8, Dx9: .bmp, .dds, .dib, .hdr, .jpg, .pfm, .png, .ppm, .tga
	FileManager fm;
	if(!fm.LoadFile(fname,path_type)) return 0;

	SpriteInfo* si=new SpriteInfo();
	si->OffsX=0;
	si->OffsY=0;

	DWORD result=FillSurfaceFromMemory(si,fm.GetBuf(),fm.GetFsize());
	return result;
}

DWORD SpriteManager::LoadSprite3d(const char* fname, int path_type, int dir)
{
	Animation3d* anim3d=Load3dAnimation(fname,path_type);
	if(!anim3d) return false;

	// Render
	if(!sceneBeginned) D3D_HR(d3dDevice->BeginScene());
	LPDIRECT3DSURFACE old_rt=NULL,old_ds=NULL;
	D3D_HR(d3dDevice->GetRenderTarget(0,&old_rt));
	D3D_HR(d3dDevice->GetDepthStencilSurface(&old_ds));
	D3D_HR(d3dDevice->SetDepthStencilSurface(spr3dDS));
	D3D_HR(d3dDevice->SetRenderTarget(0,spr3dRT));
	D3D_HR(d3dDevice->Clear(0,NULL,D3DCLEAR_TARGET,0,1.0f,0));

	Animation3d::SetScreenSize(spr3dSurfWidth,spr3dSurfHeight);
	anim3d->EnableSetupBorders(false);
	if(dir<0 || dir>5) anim3d->SetDirAngle(dir);
	else anim3d->SetDir(dir);
	Draw3d(spr3dSurfWidth/2,spr3dSurfHeight-spr3dSurfHeight/4,1.0f,anim3d,NULL,baseColor);
	anim3d->EnableSetupBorders(true);
	anim3d->SetupBorders();
	Animation3d::SetScreenSize(modeWidth,modeHeight);

	D3D_HR(d3dDevice->SetRenderTarget(0,old_rt));
	D3D_HR(d3dDevice->SetDepthStencilSurface(old_ds));
	old_rt->Release();
	old_ds->Release();
	if(!sceneBeginned) D3D_HR(d3dDevice->EndScene());

	// Calculate sprite borders
	INTRECT fb=anim3d->GetFullBorders();
	RECT r_={fb.L,fb.T,fb.R+1,fb.B+1};

	// Grow surfaces while sprite not fitted in it
	if(fb.L<0 || fb.R>=spr3dSurfWidth || fb.T<0 || fb.B>=spr3dSurfHeight)
	{
		// Grow x2
 		if(fb.L<0 || fb.R>=spr3dSurfWidth) spr3dSurfWidth*=2;
 		if(fb.T<0 || fb.B>=spr3dSurfHeight) spr3dSurfHeight*=2;

		// Recreate
		SAFEREL(spr3dRT);
		SAFEREL(spr3dRTEx);
		SAFEREL(spr3dDS);
		SAFEREL(spr3dRTData);
		D3D_HR(d3dDevice->CreateRenderTarget(spr3dSurfWidth,spr3dSurfHeight,D3DFMT_A8R8G8B8,presentParams.MultiSampleType,presentParams.MultiSampleQuality,FALSE,&spr3dRT,NULL));
		if(presentParams.MultiSampleType!=D3DMULTISAMPLE_NONE)
			D3D_HR(d3dDevice->CreateRenderTarget(spr3dSurfWidth,spr3dSurfHeight,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&spr3dRTEx,NULL));
		D3D_HR(d3dDevice->CreateDepthStencilSurface(spr3dSurfWidth,spr3dSurfHeight,D3DFMT_D24S8,presentParams.MultiSampleType,presentParams.MultiSampleQuality,TRUE,&spr3dDS,NULL));
		D3D_HR(d3dDevice->CreateOffscreenPlainSurface(spr3dSurfWidth,spr3dSurfHeight,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&spr3dRTData,NULL));

		// Try load again
		return LoadSprite3d(fname,path_type,dir);
	}

	// Get render target data
	if(presentParams.MultiSampleType!=D3DMULTISAMPLE_NONE)
	{
		D3D_HR(d3dDevice->StretchRect(spr3dRT,&r_,spr3dRTEx,&r_,D3DTEXF_NONE));
		D3D_HR(d3dDevice->GetRenderTargetData(spr3dRTEx,spr3dRTData));
	}
	else
	{
		D3D_HR(d3dDevice->GetRenderTargetData(spr3dRT,spr3dRTData));
	}

	// Copy to system memory
	D3DLOCKED_RECT lr;
	D3D_HR(spr3dRTData->LockRect(&lr,&r_,D3DLOCK_READONLY));
	DWORD w=fb.W();
	DWORD h=fb.H();
	DWORD size=12+h*w*4;
	BYTE* data=new BYTE[size];
	*((DWORD*)data)=MAKEFOURCC('F','0','F','A'); // FOnline FAst
	*((DWORD*)data+1)=w;
	*((DWORD*)data+2)=h;
	for(int i=0;i<h;i++) memcpy(data+12+w*4*i,(BYTE*)lr.pBits+lr.Pitch*i,w*4);
	D3D_HR(spr3dRTData->UnlockRect());

	// Fill from memory
	SpriteInfo* si=new SpriteInfo();
	INTPOINT p=anim3d->GetFullBordersPivot();
	si->OffsX=fb.W()/2-p.X;
	si->OffsY=fb.H()-p.Y;
	DWORD result=FillSurfaceFromMemory(si,data,size);
	delete[] data;
	return result;
}

DWORD SpriteManager::LoadRix(const char* fname, int path_type)
{
	if(!isInit) return 0;
	if(!fname || !fname[0]) return 0;

	FileManager fm;
	if(!fm.LoadFile(fname,path_type)) return 0;

	SpriteInfo* si=new SpriteInfo();
	fm.SetCurPos(0x4);
	WORD w;fm.CopyMem(&w,2);
	WORD h;fm.CopyMem(&h,2);

	// Create FOnline fast format
	DWORD size=12+h*w*4;
	BYTE* data=new BYTE[size];
	*((DWORD*)data)=MAKEFOURCC('F','0','F','A'); //FOnline FAst
	*((DWORD*)data+1)=w;
	*((DWORD*)data+2)=h;
	DWORD* ptr=(DWORD*)data+3;
	fm.SetCurPos(0xA);
	BYTE* palette=fm.GetCurBuf();
	fm.SetCurPos(0xA+256*3);
	for(int i=0,j=w*h;i<j;i++)
	{
		DWORD index=fm.GetByte();
		DWORD r=*(palette+index*3+0)*4;
		DWORD g=*(palette+index*3+1)*4;
		DWORD b=*(palette+index*3+2)*4;
		*(ptr+i)=D3DCOLOR_XRGB(r,g,b);
	}

	DWORD result=FillSurfaceFromMemory(si,data,size);
	delete[] data;
	return result;
}

AnyFrames* SpriteManager::LoadAnyAnimation(const char* fname, int path_type, bool anim_pix, int dir)
{
	if(!isInit || !fname || !fname[0]) return NULL;

	const char* ext=FileManager::GetExtension(fname);
	if(!ext)
	{
		WriteLog(__FUNCTION__" - Extension not found, file<%s>.\n",fname);
		return NULL;
	}

	if(_stricmp(ext,"frm") && _stricmp(ext,"fr0") && _stricmp(ext,"fr1") && _stricmp(ext,"fr2") &&
		_stricmp(ext,"fr3") && _stricmp(ext,"fr4") && _stricmp(ext,"fr5"))
	{
		if(!_stricmp(ext,"fofrm")) return LoadAnyAnimationFofrm(fname,path_type,dir);
		return LoadAnyAnimationOneSpr(fname,path_type,dir);
	}

	if(dir<0 || dir>5) return NULL;

	FileManager fm;
	if(!fm.LoadFile(fname,path_type)) return NULL;

	AnyFrames* anim=new(nothrow) AnyFrames();
	if(!anim)
	{
		WriteLog(__FUNCTION__" - Memory allocation fail.\n");
		return NULL;
	}

	fm.SetCurPos(0x4);
	WORD frm_fps=fm.GetBEWord();
	if(!frm_fps) frm_fps=10;

	fm.SetCurPos(0x8);
	WORD frm_num=fm.GetBEWord();

	fm.SetCurPos(0xA+dir*2);
	short offs_x=fm.GetBEWord();
	anim->OffsX=offs_x;
	fm.SetCurPos(0x16+dir*2);
	short offs_y=fm.GetBEWord();
	anim->OffsY=offs_y;

	anim->CntFrm=frm_num; 
	anim->Ticks=1000/frm_fps*frm_num;

	anim->Ind=new DWORD[frm_num];
	if(!anim->Ind) return NULL;
	anim->NextX=new short[frm_num];
	if(!anim->NextX) return NULL;
	anim->NextY=new short[frm_num];
	if(!anim->NextY) return NULL;

	fm.SetCurPos(0x22+dir*4);
	DWORD offset=0x3E+fm.GetBEDWord();

	if(offset==0x3E && dir)
	{
		delete anim;
		return NULL;
	}

	int anim_pix_type=0;
	// 0x00 - None
	// 0x01 - Slime, 229 - 232, 4
	// 0x02 - Monitors, 233 - 237, 5
	// 0x04 - FireSlow, 238 - 242, 5
	// 0x08 - FireFast, 243 - 247, 5
	// 0x10 - Shoreline, 248 - 253, 6
	// 0x20 - BlinkingRed, 254, parse on 15 frames
	const BYTE blinking_red_vals[10]={254,210,165,120,75,45,90,135,180,225};

	for(int frm=0;frm<frm_num;frm++)
	{
		SpriteInfo* si=new SpriteInfo(); // TODO: Memory leak
		if(!si) return NULL;
		fm.SetCurPos(offset);
		WORD w=fm.GetBEWord();
		WORD h=fm.GetBEWord();

		fm.GoForward(4); // Frame size

		si->OffsX=offs_x;
		si->OffsY=offs_y;

		anim->NextX[frm]=fm.GetBEWord();
		anim->NextY[frm]=fm.GetBEWord();

		// Create FOnline fast format
		DWORD size=12+h*w*4;
		BYTE* data=new BYTE[size];
		if(!data)
		{
			WriteLog(__FUNCTION__" - Not enough memory, size<%u>.\n",size);
			delete anim;
			return NULL;
		}
		*((DWORD*)data)=MAKEFOURCC('F','0','F','A'); // FOnline FAst
		*((DWORD*)data+1)=w;
		*((DWORD*)data+2)=h;
		DWORD* ptr=(DWORD*)data+3;
		DWORD* palette=(DWORD*)FoPalette;
		fm.SetCurPos(offset+12);

		if(!anim_pix_type)
		{
			for(int i=0,j=w*h;i<j;i++) *(ptr+i)=palette[fm.GetByte()];
		}
		else
		{
			for(int i=0,j=w*h;i<j;i++)
			{
				BYTE index=fm.GetByte();
				if(index>=229 && index<255)
				{
					if(index>=229 && index<=232) {index-=frm%4; if(index<229) index+=4;}
					else if(index>=233 && index<=237) {index-=frm%5; if(index<233) index+=5;}
					else if(index>=238 && index<=242) {index-=frm%5; if(index<238) index+=5;}
					else if(index>=243 && index<=247) {index-=frm%5; if(index<243) index+=5;}
					else if(index>=248 && index<=253) {index-=frm%6; if(index<248) index+=6;}
					else
					{
						*(ptr+i)=D3DCOLOR_XRGB(blinking_red_vals[frm%10],0,0);
						continue;
					}
				}
				*(ptr+i)=palette[index];
			}
		}

		// Check for animate pixels
		if(!frm && anim_pix)
		{
			fm.SetCurPos(offset+12);
			for(int i=0,j=w*h;i<j;i++)
			{
				BYTE index=fm.GetByte();
				if(index<229 || index==255) continue;
				if(index>=229 && index<=232) anim_pix_type|=0x01;
				else if(index>=233 && index<=237) anim_pix_type|=0x02;
				else if(index>=238 && index<=242) anim_pix_type|=0x04;
				else if(index>=243 && index<=247) anim_pix_type|=0x08;
				else if(index>=248 && index<=253) anim_pix_type|=0x10;
				else anim_pix_type|=0x20;
			}

			if(anim_pix_type&0x01) anim->Ticks=200;
			if(anim_pix_type&0x04) anim->Ticks=200;
			if(anim_pix_type&0x10) anim->Ticks=200;
			if(anim_pix_type&0x08) anim->Ticks=142;
			if(anim_pix_type&0x02) anim->Ticks=100;
			if(anim_pix_type&0x20) anim->Ticks=100;

			if(anim_pix_type)
			{
				int divs[4]; divs[0]=1; divs[1]=1; divs[2]=1; divs[3]=1;
				if(anim_pix_type&0x01) divs[0]=4;
				if(anim_pix_type&0x02) divs[1]=5;
				if(anim_pix_type&0x04) divs[1]=5;
				if(anim_pix_type&0x08) divs[1]=5;
				if(anim_pix_type&0x10) divs[2]=6;
				if(anim_pix_type&0x20) divs[3]=10;

				frm_num=4;
				for(int i=0;i<4;i++)
				{
					if(!(frm_num%divs[i])) continue;
					frm_num++;
					i=-1;
				}

				anim->Ticks*=frm_num;
				anim->CntFrm=frm_num; 
				short nx=anim->NextX[0];
				short ny=anim->NextY[0];
				SAFEDELA(anim->Ind);
				SAFEDELA(anim->NextX);
				SAFEDELA(anim->NextY);
				anim->Ind=new DWORD[frm_num];
				if(!anim->Ind) return NULL;
				anim->NextX=new short[frm_num];
				if(!anim->NextX) return NULL;
				anim->NextY=new short[frm_num];
				if(!anim->NextY) return NULL;
				anim->NextX[0]=nx;
				anim->NextY[0]=ny;
			}
		}

		if(!anim_pix_type) offset+=w*h+12;

		DWORD result=FillSurfaceFromMemory(si,data,size);
		delete[] data;
		if(!result)
		{
			delete anim;
			return NULL;
		}
		anim->Ind[frm]=result;
	}

	return anim;
}

AnyFrames* SpriteManager::LoadAnyAnimationFofrm(const char* fname, int path_type, int dir)
{
	IniParser fofrm;
	if(!fofrm.LoadFile(fname,path_type)) return NULL;

	WORD frm_fps=fofrm.GetInt("fps",0);
	if(!frm_fps) frm_fps=10;

	WORD frm_num=fofrm.GetInt("count",0);
	if(!frm_num) frm_num=1;

	AutoPtr<AnyFrames> anim(new AnyFrames());
	if(!anim.IsValid())
	{
		WriteLog(__FUNCTION__" - Memory allocation fail.\n");
		return NULL;
	}

	anim->OffsX=fofrm.GetInt("offs_x",0);
	anim->OffsY=fofrm.GetInt("offs_y",0);
	anim->CntFrm=frm_num;
	anim->Ticks=1000/frm_fps*frm_num;
	anim->Ind=new DWORD[frm_num];
	if(!anim->Ind) return 0;
	anim->NextX=new short[frm_num];
	if(!anim->NextX) return 0;
	anim->NextY=new short[frm_num];
	if(!anim->NextY) return 0;

	EffectEx* effect=NULL;
	if(fofrm.IsKey("effect"))
	{
		char effect_name[MAX_FOPATH];
		if(fofrm.GetStr("effect","",effect_name)) effect=Loader3d::LoadEffect(d3dDevice,effect_name);
	}

	char dir_str[16];
	sprintf(dir_str,"dir_%d",dir);
	bool no_app=(dir==0 && !fofrm.IsApp("dir_0"));

	if(!no_app)
	{
		anim->OffsX=fofrm.GetInt(dir_str,"offs_x",anim->OffsX);
		anim->OffsY=fofrm.GetInt(dir_str,"offs_y",anim->OffsY);
	}

	char frm_fname[MAX_FOPATH];
	FileManager::ExtractPath(fname,frm_fname);
	char* frm_name=frm_fname+strlen(frm_fname);

	for(int frm=0;frm<frm_num;frm++)
	{
		anim->NextX[frm]=fofrm.GetInt(no_app?NULL:dir_str,Str::Format("next_x_%d",frm),0);
		anim->NextY[frm]=fofrm.GetInt(no_app?NULL:dir_str,Str::Format("next_y_%d",frm),0);

		if(!fofrm.GetStr(no_app?NULL:dir_str,Str::Format("frm_%d",frm),"",frm_name) &&
			(frm!=0 || !fofrm.GetStr(no_app?NULL:dir_str,Str::Format("frm",frm),"",frm_name))) return NULL;

		DWORD spr_id=LoadSprite(frm_fname,path_type);
		if(!spr_id) return NULL;

		SpriteInfo* si=GetSpriteInfo(spr_id);
		si->OffsX+=anim->OffsX;
		si->OffsY+=anim->OffsY;
		si->Effect=effect;
		anim->Ind[frm]=spr_id;
	}

	return anim.Release();
}

AnyFrames* SpriteManager::LoadAnyAnimationOneSpr(const char* fname, int path_type, int dir)
{
	DWORD spr_id=LoadSprite(fname,path_type,dir);
	if(!spr_id) return NULL;

	AnyFrames* anim=new(nothrow) AnyFrames();
	if(!anim)
	{
		WriteLog(__FUNCTION__" - Memory allocation fail.\n");
		return NULL;
	}

	anim->OffsX=0;
	anim->OffsY=0;
	anim->CntFrm=1;
	anim->Ticks=1000;
	anim->Ind=new DWORD[1];
	anim->Ind[0]=spr_id;
	anim->NextX=new short[1];
	anim->NextX[0]=0;
	anim->NextY=new short[1];
	anim->NextY[0]=0;
	return anim;
}

Animation3d* SpriteManager::Load3dAnimation(const char* fname, int path_type)
{
	// Fill data
	Animation3d* anim3d=Animation3d::GetAnimation(fname,path_type,false);
	if(!anim3d) return NULL;

	SpriteInfo* si=new SpriteInfo();
	size_t index=1;
	for(size_t j=sprData.size();index<j;index++) if(!sprData[index]) break;
	if(index<sprData.size()) sprData[index]=si;
	else sprData.push_back(si);

	// Cross links
	anim3d->SetSprId(index);
	si->Anim3d=anim3d;
	return anim3d;
}

bool SpriteManager::Flush()
{
	if(!isInit) return false;
	if(!curSprCnt) return true;

	void* ptr;
	int mulpos=4*curSprCnt;
	D3D_HR(pVB->Lock(0,sizeof(MYVERTEX)*mulpos,(void**)&ptr,D3DLOCK_DISCARD));
	memcpy(ptr,waitBuf,sizeof(MYVERTEX)*mulpos);
	D3D_HR(pVB->Unlock());

	if(!dipQueue.empty())
	{
		D3D_HR(d3dDevice->SetIndices(pIB));
		D3D_HR(d3dDevice->SetStreamSource(0,pVB,0,sizeof(MYVERTEX)));
		D3D_HR(d3dDevice->SetFVF(D3DFVF_MYVERTEX));

		DWORD rpos=0;
		for(DipDataVecIt it=dipQueue.begin(),end=dipQueue.end();it!=end;++it)
		{
			DipData& dip=*it;
			EffectEx* effect_ex=(dip.Effect?dip.Effect:curDefaultEffect);

			D3D_HR(d3dDevice->SetTexture(0,dip.Texture));

			if(effect_ex)
			{
				ID3DXEffect* effect=effect_ex->Effect;

				if(effect_ex->EffectParams) D3D_HR(effect->ApplyParameterBlock(effect_ex->EffectParams));
				D3D_HR(effect->SetTechnique(effect_ex->TechniqueSimple));
				if(effect_ex->IsNeedProcess) Loader3d::EffectProcessVariables(effect_ex,-1);

				UINT passes;
				D3D_HR(effect->Begin(&passes,effect_ex->EffectFlags));
				for(UINT pass=0;pass<passes;pass++)
				{
					if(effect_ex->IsNeedProcess) Loader3d::EffectProcessVariables(effect_ex,pass);

					D3D_HR(effect->BeginPass(pass));
					D3D_HR(d3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,mulpos,rpos,2*dip.SprCount));
					D3D_HR(effect->EndPass());
				}
				D3D_HR(effect->End());
			}
			else
			{
				D3D_HR(d3dDevice->SetVertexShader(NULL));
				D3D_HR(d3dDevice->SetPixelShader(NULL));
				D3D_HR(d3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,mulpos,rpos,2*dip.SprCount));
			}

			rpos+=6*dip.SprCount;
		}

		dipQueue.clear();
	}

	curSprCnt=0;
	return true;
}

bool SpriteManager::DrawSprite(DWORD id, int x, int y, DWORD color /* = 0 */)
{
	if(!id) return false;

	SpriteInfo* si=sprData[id];
	if(!si) return false;

	if(si->Anim3d)
	{
		Flush();
		Draw3d(x,y,1.0f,si->Anim3d,NULL,color);
		return true;
	}

	EffectEx* effect=(si->Effect?si->Effect:sprDefaultEffect[DEFAULT_EFFECT_IFACE]);
	if(dipQueue.empty() || dipQueue.back().Texture!=si->Surf->Texture || dipQueue.back().Effect!=effect)
		dipQueue.push_back(DipData(si->Surf->Texture,effect));
	else
		dipQueue.back().SprCount++;

	int mulpos=curSprCnt*4;

	if(!color) color=COLOR_IFACE;

	waitBuf[mulpos].x=x-0.5f;
	waitBuf[mulpos].y=y+si->Height-0.5f;
	waitBuf[mulpos].tu=si->SprRect.L;
	waitBuf[mulpos].tv=si->SprRect.B;
	waitBuf[mulpos++].Diffuse=color;

	waitBuf[mulpos].x=x-0.5f;
	waitBuf[mulpos].y=y-0.5f;
	waitBuf[mulpos].tu=si->SprRect.L;
	waitBuf[mulpos].tv=si->SprRect.T;
	waitBuf[mulpos++].Diffuse=color;

	waitBuf[mulpos].x=x+si->Width-0.5f;
	waitBuf[mulpos].y=y-0.5f;
	waitBuf[mulpos].tu=si->SprRect.R;
	waitBuf[mulpos].tv=si->SprRect.T;
	waitBuf[mulpos++].Diffuse=color;

	waitBuf[mulpos].x=x+si->Width-0.5f;
	waitBuf[mulpos].y=y+si->Height-0.5f;
	waitBuf[mulpos].tu=si->SprRect.R;
	waitBuf[mulpos].tv=si->SprRect.B;
	waitBuf[mulpos].Diffuse=color;

	if(++curSprCnt==flushSprCnt) Flush();
	return true;
}

bool SpriteManager::DrawSpriteSize(DWORD id, int x, int y, float w, float h, bool stretch_up, bool center, DWORD color /* = 0 */)
{
	if(!id) return false;

	SpriteInfo* si=sprData[id];
	if(!si) return false;

	float w_real=(float)si->Width;
	float h_real=(float)si->Height;
// 	if(si->Anim3d)
// 	{
// 		si->Anim3d->SetupBorders();
// 		INTRECT fb=si->Anim3d->GetBaseBorders();
// 		w_real=fb.W();
// 		h_real=fb.H();
// 	}

	float wf=w_real;
	float hf=h_real;
	float k=min(w/w_real,h/h_real);

	if(k<1.0f || (k>1.0f && stretch_up))
	{
		wf*=k;
		hf*=k;
	}

	if(center)
	{
		x+=(w-wf)/2.0f;
		y+=(h-hf)/2.0f;
	}

	if(si->Anim3d)
	{
		Flush();
		Draw3d(x,y,1.0f,si->Anim3d,NULL,color);
		return true;
	}

	EffectEx* effect=(si->Effect?si->Effect:sprDefaultEffect[DEFAULT_EFFECT_IFACE]);
	if(dipQueue.empty() || dipQueue.back().Texture!=si->Surf->Texture || dipQueue.back().Effect!=effect)
		dipQueue.push_back(DipData(si->Surf->Texture,effect));
	else
		dipQueue.back().SprCount++;

	int mulpos=curSprCnt*4;

	if(!color) color=COLOR_IFACE;

	waitBuf[mulpos].x=x-0.5f;
	waitBuf[mulpos].y=y+hf-0.5f;
	waitBuf[mulpos].tu=si->SprRect.L;
	waitBuf[mulpos].tv=si->SprRect.B;
	waitBuf[mulpos++].Diffuse=color;

	waitBuf[mulpos].x=x-0.5f;
	waitBuf[mulpos].y=y-0.5f;
	waitBuf[mulpos].tu=si->SprRect.L;
	waitBuf[mulpos].tv=si->SprRect.T;
	waitBuf[mulpos++].Diffuse=color;

	waitBuf[mulpos].x=x+wf-0.5f;
	waitBuf[mulpos].y=y-0.5f;
	waitBuf[mulpos].tu=si->SprRect.R;
	waitBuf[mulpos].tv=si->SprRect.T;
	waitBuf[mulpos++].Diffuse=color;

	waitBuf[mulpos].x=x+wf-0.5f;
	waitBuf[mulpos].y=y+hf-0.5f;
	waitBuf[mulpos].tu=si->SprRect.R;
	waitBuf[mulpos].tv=si->SprRect.B;
	waitBuf[mulpos].Diffuse=color;

	if(++curSprCnt==flushSprCnt) Flush();
	return true;
}

void SpriteManager::PrepareSquare(PointVec& points, FLTRECT& r, DWORD color)
{
	points.push_back(PrepPoint(r.L,r.B,color,NULL,NULL));
	points.push_back(PrepPoint(r.L,r.T,color,NULL,NULL));
	points.push_back(PrepPoint(r.R,r.B,color,NULL,NULL));
	points.push_back(PrepPoint(r.L,r.T,color,NULL,NULL));
	points.push_back(PrepPoint(r.R,r.T,color,NULL,NULL));
	points.push_back(PrepPoint(r.R,r.B,color,NULL,NULL));
}

bool SpriteManager::PrepareBuffer(Sprites& dtree, LPDIRECT3DSURFACE surf, BYTE alpha)
{
	if(!dtree.Size()) return true;
	if(!surf) return false;
	Flush();

	// Set new render target
	LPDIRECT3DSURFACE old_rt=NULL,old_ds=NULL;
	D3D_HR(d3dDevice->GetRenderTarget(0,&old_rt));
	D3D_HR(d3dDevice->GetDepthStencilSurface(&old_ds));
	D3D_HR(d3dDevice->SetDepthStencilSurface(NULL));
	D3D_HR(d3dDevice->SetRenderTarget(0,surf));
	D3D_HR(d3dDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT));
	D3D_HR(d3dDevice->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT));

	// Draw
	for(SpriteVecIt it=dtree.Begin(),end=dtree.End();it!=end;++it)
	{
		Sprite* spr=*it;
		if(!spr->Valid) continue;
		SpriteInfo* si=sprData[spr->SprId];
		if(!si) continue;

		int x=spr->ScrX-si->Width/2+si->OffsX+32;
		int y=spr->ScrY-si->Height+si->OffsY+24;
		if(spr->OffsX) x+=*spr->OffsX;
		if(spr->OffsY) y+=*spr->OffsY;

		if(dipQueue.empty() || dipQueue.back().Texture!=si->Surf->Texture || dipQueue.back().Effect!=si->Effect)
			dipQueue.push_back(DipData(si->Surf->Texture,si->Effect));
		else
			dipQueue.back().SprCount++;

		DWORD color=baseColor;
		if(spr->Alpha) ((BYTE*)&color)[3]=*spr->Alpha;
		else if(alpha) ((BYTE*)&color)[3]=alpha;

		// Casts
		float xf=(float)x/GameOpt.SpritesZoom-0.5f;
		float yf=(float)y/GameOpt.SpritesZoom-0.5f;
		float wf=(float)si->Width/GameOpt.SpritesZoom;
		float hf=(float)si->Height/GameOpt.SpritesZoom;

		// Fill buffer
		int mulpos=curSprCnt*4;

		waitBuf[mulpos].x=xf;
		waitBuf[mulpos].y=yf+hf;
		waitBuf[mulpos].tu=si->SprRect.L;
		waitBuf[mulpos].tv=si->SprRect.B;
		waitBuf[mulpos++].Diffuse=color;

		waitBuf[mulpos].x=xf;
		waitBuf[mulpos].y=yf;
		waitBuf[mulpos].tu=si->SprRect.L;
		waitBuf[mulpos].tv=si->SprRect.T;
		waitBuf[mulpos++].Diffuse=color;

		waitBuf[mulpos].x=xf+wf;
		waitBuf[mulpos].y=yf;
		waitBuf[mulpos].tu=si->SprRect.R;
		waitBuf[mulpos].tv=si->SprRect.T;
		waitBuf[mulpos++].Diffuse=color;

		waitBuf[mulpos].x=xf+wf;
		waitBuf[mulpos].y=yf+hf;
		waitBuf[mulpos].tu=si->SprRect.R;
		waitBuf[mulpos].tv=si->SprRect.B;
		waitBuf[mulpos++].Diffuse=color;

		if(++curSprCnt==flushSprCnt) Flush();
	}
	Flush();

	// Restore render target
	D3D_HR(d3dDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR));
	D3D_HR(d3dDevice->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));
	D3D_HR(d3dDevice->SetRenderTarget(0,old_rt));
	D3D_HR(d3dDevice->SetDepthStencilSurface(old_ds));
	old_rt->Release();
	old_ds->Release();
	return true;
}

bool SpriteManager::DrawPrepared(LPDIRECT3DSURFACE& surf)
{
	if(!surf) return true;
	Flush();

	int ox=(int)((float)(32-GameOpt.ScrOx)/GameOpt.SpritesZoom);
	int oy=(int)((float)(24-GameOpt.ScrOy)/GameOpt.SpritesZoom);
	RECT src={ox,oy,ox+modeWidth,oy+modeHeight};

	LPDIRECT3DSURFACE backbuf=NULL;
	D3D_HR(d3dDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&backbuf));
	D3D_HR(d3dDevice->StretchRect(surf,&src,backbuf,NULL,D3DTEXF_NONE));
	backbuf->Release();
	return true;
}

bool SpriteManager::DrawSurface(LPDIRECT3DSURFACE& surf, RECT& dst)
{
	if(!surf) return true;
	Flush();

	LPDIRECT3DSURFACE backbuf=NULL;
	D3D_HR(d3dDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&backbuf));
	D3D_HR(d3dDevice->StretchRect(surf,NULL,backbuf,&dst,D3DTEXF_LINEAR));
	backbuf->Release();
	return true;
}

DWORD SpriteManager::GetColor(int r, int g, int b)
{
	r=CLAMP(r,0,255);
	g=CLAMP(g,0,255);
	b=CLAMP(b,0,255);
	return D3DCOLOR_XRGB(r,g,b);
}

void SpriteManager::GetDrawCntrRect(Sprite* prep, INTRECT* prect)
{
	DWORD id=(prep->PSprId?*prep->PSprId:prep->SprId);
	if(id>=sprData.size()) return;
	SpriteInfo* si=sprData[id];
	if(!si) return;

	if(!si->Anim3d)
	{
		int x=prep->ScrX-si->Width/2+si->OffsX;
		int y=prep->ScrY-si->Height+si->OffsY;
		if(prep->OffsX) x+=*prep->OffsX;
		if(prep->OffsY) y+=*prep->OffsY;
		prect->L=x;
		prect->T=y;
		prect->R=x+si->Width;
		prect->B=y+si->Height;
	}
	else
	{
		*prect=si->Anim3d->GetBaseBorders();
		(*prect).L*=GameOpt.SpritesZoom;
		(*prect).T*=GameOpt.SpritesZoom;
		(*prect).R*=GameOpt.SpritesZoom;
		(*prect).B*=GameOpt.SpritesZoom;
	}
}

bool SpriteManager::CompareHexEgg(WORD hx, WORD hy, Sprite::EggType egg)
{
	if(egg==Sprite::EggAlways) return true;
	if(eggHy==hy && hx&1 && !(eggHx&1)) hy--;
	switch(egg)
	{
	case Sprite::EggX: if(hx>=eggHx) return true; break;
	case Sprite::EggY: if(hy>=eggHy) return true; break;
	case Sprite::EggXandY: if(hx>=eggHx || hy>=eggHy) return true; break;
	case Sprite::EggXorY: if(hx>=eggHx && hy>=eggHy) return true; break;
	default: break;
	}
	return false;
}

void SpriteManager::SetEgg(WORD hx, WORD hy, Sprite* spr)
{
	if(!sprEgg) return;

	DWORD id=(spr->PSprId?*spr->PSprId:spr->SprId);
	SpriteInfo* si=sprData[id];
	if(!si) return;

	if(!si->Anim3d)
	{
		eggX=spr->ScrX-si->Width/2+si->OffsX+si->Width/2-sprEgg->Width/2+*spr->OffsX;
		eggY=spr->ScrY-si->Height+si->OffsY+si->Height/2-sprEgg->Height/2+*spr->OffsY;
	}
	else
	{
		INTRECT bb=si->Anim3d->GetBaseBorders();
		eggX=bb.CX()-sprEgg->Width/2-GameOpt.ScrOx;
		eggY=bb.CY()-sprEgg->Height/2-GameOpt.ScrOy;
	}

	eggHx=hx;
	eggHy=hy;
	eggValid=true;
}

bool SpriteManager::DrawSprites(Sprites& dtree, bool collect_contours, bool use_egg, DWORD pos_min, DWORD pos_max)
{
	PointVec borders;

	if(!eggValid) use_egg=false;
	bool egg_trans=false;
	int ex=eggX+GameOpt.ScrOx;
	int ey=eggY+GameOpt.ScrOy;
	DWORD cur_tick=Timer::FastTick();

	for(SpriteVecIt it=dtree.Begin(),end=dtree.End();it!=end;++it)
	{
		// Data
		Sprite* spr=*it;
		if(!spr->Valid || spr->MapPos<pos_min) continue;

		DWORD pos=spr->MapPos;
		if(pos>pos_max) break;

		DWORD id=(spr->PSprId?*spr->PSprId:spr->SprId);
		SpriteInfo* si=sprData[id];
		if(!si) continue;

		int x=spr->ScrX-si->Width/2+si->OffsX;
		int y=spr->ScrY-si->Height+si->OffsY;
		x+=GameOpt.ScrOx;
		y+=GameOpt.ScrOy;
		if(spr->OffsX) x+=*spr->OffsX;
		if(spr->OffsY) y+=*spr->OffsY;

		// Check borders
		if(!si->Anim3d)
		{
			if(x/GameOpt.SpritesZoom>modeWidth || (x+si->Width)/GameOpt.SpritesZoom<0 || y/GameOpt.SpritesZoom>modeHeight || (y+si->Height)/GameOpt.SpritesZoom<0) continue;
		}
		else
		{
			// Todo: check 3d borders
//			INTRECT fb=si->Anim3d->GetExtraBorders();
//			if(x/GameOpt.SpritesZoom>modeWidth || (x+si->Width)/GameOpt.SpritesZoom<0 || y/GameOpt.SpritesZoom>modeHeight || (y+si->Height)/GameOpt.SpritesZoom<0) continue;
		}

		// Base color
		DWORD cur_color;
		if(spr->Color)
			cur_color=(spr->Color|0xFF000000);
		else
			cur_color=baseColor;

		// Light
		if(spr->Light)
		{
			int lr=*spr->Light;
			int lg=*(spr->Light+1);
			int lb=*(spr->Light+2);
			BYTE& r=((BYTE*)&cur_color)[2];
			BYTE& g=((BYTE*)&cur_color)[1];
			BYTE& b=((BYTE*)&cur_color)[0];
			int ir=(int)r+lr;
			int ig=(int)g+lg;
			int ib=(int)b+lb;
			if(ir>0xFF) ir=0xFF;
			if(ig>0xFF) ig=0xFF;
			if(ib>0xFF) ib=0xFF;
			r=ir;
			g=ig;
			b=ib;
		}

		// Alpha
		if(spr->Alpha)
		{
			((BYTE*)&cur_color)[3]=*spr->Alpha;
		}

		// Process flashing
		if(spr->FlashMask)
		{
			static int cnt=0;
			static DWORD tick=cur_tick+100;
			static bool add=true;
			if(cur_tick>=tick)
			{
				cnt+=(add?10:-10);
				if(cnt>40)
				{
					cnt=40;
					add=false;
				}
				else if(cnt<-40)
				{
					cnt=-40;
					add=true;
				}
				tick=cur_tick+100;
			}
			int r=((cur_color>>16)&0xFF)+cnt;
			int g=((cur_color>>8)&0xFF)+cnt;
			int b=(cur_color&0xFF)+cnt;
			r=CLAMP(r,0,0xFF);
			g=CLAMP(g,0,0xFF);
			b=CLAMP(b,0,0xFF);
			((BYTE*)&cur_color)[2]=r;
			((BYTE*)&cur_color)[1]=g;
			((BYTE*)&cur_color)[0]=b;
			cur_color&=spr->FlashMask;
		}

		// 3d model
		if(si->Anim3d)
		{
			// Draw collected sprites and disable egg
			Flush();
			if(egg_trans)
			{
				D3D_HR(d3dDevice->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
				D3D_HR(d3dDevice->SetTexture(1,NULL));
				egg_trans=false;
			}

			// Draw 3d animation
			Draw3d(x/GameOpt.SpritesZoom,y/GameOpt.SpritesZoom,1.0f/GameOpt.SpritesZoom,si->Anim3d,NULL,cur_color);

			// Process contour effect
			if(collect_contours && spr->Contour!=Sprite::ContourNone) CollectContour(x,y,si,spr);

			// Debug borders
			if(GameOpt.DebugInfo)
			{
				INTRECT eb=si->Anim3d->GetExtraBorders();
				INTRECT bb=si->Anim3d->GetBaseBorders();
				PrepareSquare(borders,FLTRECT(eb.L,eb.T,eb.R,eb.B),0x5f750075);
				PrepareSquare(borders,FLTRECT(bb.L,bb.T,bb.R,bb.B),0x5f757575);
			}

			continue;
		}

		// 2d sprite
		// Egg process
		bool egg_added=false;
		if(use_egg && spr->Egg!=Sprite::EggNone && CompareHexEgg(HEX_X_POS(pos),HEX_Y_POS(pos),spr->Egg))
		{
			int x1=x-ex;
			int y1=y-ey;
			int x2=x1+si->Width;
			int y2=y1+si->Height;

			if(!(x1>=eggSprWidth || y1>=eggSprHeight || x2<0 || y2<0))
			{
				if(!egg_trans)
				{
					Flush();
					D3D_HR(d3dDevice->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_SELECTARG2));
					D3D_HR(d3dDevice->SetTexture(1,sprEgg->Surf->Texture));
					egg_trans=true;
				}

				x1=max(x1,0);
				y1=max(y1,0);
				x2=min(x2,eggSprWidth);
				y2=min(y2,eggSprHeight);

				float x1f=(float)(x1+SURF_SPRITES_OFFS);
				float x2f=(float)(x2+SURF_SPRITES_OFFS);
				float y1f=(float)(y1+SURF_SPRITES_OFFS);
				float y2f=(float)(y2+SURF_SPRITES_OFFS);

				int mulpos=curSprCnt*4;
				waitBuf[mulpos].tu2=x1f/eggSurfWidth;
				waitBuf[mulpos].tv2=y2f/eggSurfHeight;
				waitBuf[mulpos+1].tu2=x1f/eggSurfWidth;
				waitBuf[mulpos+1].tv2=y1f/eggSurfHeight;
				waitBuf[mulpos+2].tu2=x2f/eggSurfWidth;
				waitBuf[mulpos+2].tv2=y1f/eggSurfHeight;
				waitBuf[mulpos+3].tu2=x2f/eggSurfWidth;
				waitBuf[mulpos+3].tv2=y2f/eggSurfHeight;
				egg_added=true;
			}
		}

		if(!egg_added && egg_trans)
		{
			Flush();
			D3D_HR(d3dDevice->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
			D3D_HR(d3dDevice->SetTexture(1,NULL));
			egg_trans=false;
		}

		// Choose surface
		if(dipQueue.empty() || dipQueue.back().Texture!=si->Surf->Texture || dipQueue.back().Effect!=si->Effect)
			dipQueue.push_back(DipData(si->Surf->Texture,si->Effect));
		else
			dipQueue.back().SprCount++;

		// Process contour effect
		if(collect_contours && spr->Contour!=Sprite::ContourNone) CollectContour(x,y,si,spr);

		// Casts
		float xf=(float)x/GameOpt.SpritesZoom-0.5f;
		float yf=(float)y/GameOpt.SpritesZoom-0.5f;
		float wf=(float)si->Width/GameOpt.SpritesZoom;
		float hf=(float)si->Height/GameOpt.SpritesZoom;

		// Fill buffer
		int mulpos=curSprCnt*4;

		waitBuf[mulpos].x=xf;
		waitBuf[mulpos].y=yf+hf;
		waitBuf[mulpos].tu=si->SprRect.L;
		waitBuf[mulpos].tv=si->SprRect.B;
		waitBuf[mulpos++].Diffuse=cur_color;

		waitBuf[mulpos].x=xf;
		waitBuf[mulpos].y=yf;
		waitBuf[mulpos].tu=si->SprRect.L;
		waitBuf[mulpos].tv=si->SprRect.T;
		waitBuf[mulpos++].Diffuse=cur_color;

		waitBuf[mulpos].x=xf+wf;
		waitBuf[mulpos].y=yf;
		waitBuf[mulpos].tu=si->SprRect.R;
		waitBuf[mulpos].tv=si->SprRect.T;
		waitBuf[mulpos++].Diffuse=cur_color;

		waitBuf[mulpos].x=xf+wf;
		waitBuf[mulpos].y=yf+hf;
		waitBuf[mulpos].tu=si->SprRect.R;
		waitBuf[mulpos].tv=si->SprRect.B;
		waitBuf[mulpos++].Diffuse=cur_color;

		// Set default texture coordinates for egg texture
		if(!egg_added)
		{
			waitBuf[mulpos-1].tu2=0.0f;
			waitBuf[mulpos-1].tv2=0.0f;
			waitBuf[mulpos-2].tu2=0.0f;
			waitBuf[mulpos-2].tv2=0.0f;
			waitBuf[mulpos-3].tu2=0.0f;
			waitBuf[mulpos-3].tv2=0.0f;
			waitBuf[mulpos-4].tu2=0.0f;
			waitBuf[mulpos-4].tv2=0.0f;
		}

		// Draw
		if(++curSprCnt==flushSprCnt) Flush();
	}

	Flush();
	if(egg_trans)
	{
		D3D_HR(d3dDevice->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
		D3D_HR(d3dDevice->SetTexture(1,NULL));
	}

	if(GameOpt.DebugInfo) DrawPoints(borders,D3DPT_TRIANGLELIST);
	return true;
}

bool SpriteManager::IsPixNoTransp(DWORD spr_id, int offs_x, int offs_y, bool with_zoom)
{
	if(offs_x<0 || offs_y<0) return false;
	SpriteInfo* si=GetSpriteInfo(spr_id);
	if(!si) return false;

	// 3d animation
	if(si->Anim3d)
	{
		/*if(!si->Anim3d->GetDrawIndex()) return false;

		IDirect3DSurface9* zstencil;
		D3D_HR(d3dDevice->GetDepthStencilSurface(&zstencil));

		D3DSURFACE_DESC sDesc;
		D3D_HR(zstencil->GetDesc(&sDesc));
		int width=sDesc.Width;
		int height=sDesc.Height;

		D3DLOCKED_RECT desc;
		D3D_HR(zstencil->LockRect(&desc,NULL,D3DLOCK_READONLY));
		BYTE* ptr=(BYTE*)desc.pBits;
		int pitch=desc.Pitch;

		int stencil_offset=offs_y*pitch+offs_x*4+3;
		WriteLog("===========%d %d====%u\n",offs_x,offs_y,ptr[stencil_offset]);
		if(stencil_offset<pitch*height && ptr[stencil_offset]==si->Anim3d->GetDrawIndex())
		{
		D3D_HR(zstencil->UnlockRect());
		D3D_HR(zstencil->Release());
		return true;
		}

		D3D_HR(zstencil->UnlockRect());
		D3D_HR(zstencil->Release());*/
		return false;
	}

	// 2d animation
	if(with_zoom && (offs_x>si->Width/GameOpt.SpritesZoom || offs_y>si->Height/GameOpt.SpritesZoom)) return false;
	if(!with_zoom && (offs_x>si->Width || offs_y>si->Height)) return false;

	if(with_zoom)
	{
		offs_x*=GameOpt.SpritesZoom;
		offs_y*=GameOpt.SpritesZoom;
	}

	D3DSURFACE_DESC sDesc;
	D3D_HR(si->Surf->Texture->GetLevelDesc(0,&sDesc));
	int width=sDesc.Width;
	int height=sDesc.Height;

	D3DLOCKED_RECT desc;
	D3D_HR(si->Surf->Texture->LockRect(0,&desc,NULL,D3DLOCK_READONLY));
	BYTE* ptr=(BYTE*)desc.pBits;
	int pitch=desc.Pitch;

	offs_x+=si->SprRect.L*(float)width;
	offs_y+=si->SprRect.T*(float)height;
	int alpha_offset=offs_y*pitch+offs_x*4+3;
	if(alpha_offset<pitch*height && ptr[alpha_offset]>0)
	{
		D3D_HR(si->Surf->Texture->UnlockRect(0));
		return true;
	}

	D3D_HR(si->Surf->Texture->UnlockRect(0));
	return false;

	//	pDst[i] - есть байт 
	//	b       - pDst[ y*iHeight*4 + x*4];
	//	g       - pDst[ y*iHeight*4 + x*4+1];
	//	r       - pDst[ y*iHeight*4 + x*4+2];
	//	alpha	- pDst[ y*iHeight*4 + x*4+3];
	//	x, y - координаты точки
}

bool SpriteManager::IsEggTransp(int pix_x, int pix_y)
{
	if(!eggValid) return false;

	int ex=eggX+GameOpt.ScrOx;
	int ey=eggY+GameOpt.ScrOy;
	int ox=pix_x-ex/GameOpt.SpritesZoom;
	int oy=pix_y-ey/GameOpt.SpritesZoom;

	if(ox<0 || oy<0 || ox>=int(eggSurfWidth/GameOpt.SpritesZoom) || oy>=int(eggSurfHeight/GameOpt.SpritesZoom)) return false;

	ox*=GameOpt.SpritesZoom;
	oy*=GameOpt.SpritesZoom;

	D3DSURFACE_DESC sDesc;
	D3D_HR(sprEgg->Surf->Texture->GetLevelDesc(0,&sDesc));

	int sWidth=sDesc.Width;
	int sHeight=sDesc.Height;

	D3DLOCKED_RECT lrDst;
	D3D_HR(sprEgg->Surf->Texture->LockRect(0,&lrDst,NULL,D3DLOCK_READONLY));

	BYTE* pDst=(BYTE*)lrDst.pBits;

	if(pDst[oy*sHeight*4+ox*4+3]<170)
	{
		D3D_HR(sprEgg->Surf->Texture->UnlockRect(0));
		return true;
	}

	D3D_HR(sprEgg->Surf->Texture->UnlockRect(0));
	return false;
}

bool SpriteManager::DrawPoints(PointVec& points, D3DPRIMITIVETYPE prim, float* zoom /* = NULL */, FLTRECT* stencil /* = NULL */, FLTPOINT* offset /* = NULL */)
{
	if(points.empty()) return true;
	Flush();

	int count=(int)points.size();

	// Draw stencil quad
	if(stencil)
	{
		struct Vertex
		{
			FLOAT x,y,z,rhw;
			DWORD diffuse;
		} vb[6]=
		{
			{stencil->L-0.5f,stencil->B-0.5f,1.0f,1.0f,-1},
			{stencil->L-0.5f,stencil->T-0.5f,1.0f,1.0f,-1},
			{stencil->R-0.5f,stencil->B-0.5f,1.0f,1.0f,-1},
			{stencil->L-0.5f,stencil->T-0.5f,1.0f,1.0f,-1},
			{stencil->R-0.5f,stencil->T-0.5f,1.0f,1.0f,-1},
			{stencil->R-0.5f,stencil->B-0.5f,1.0f,1.0f,-1},
		};

		D3D_HR(d3dDevice->SetRenderState(D3DRS_STENCILENABLE,TRUE));
		D3D_HR(d3dDevice->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_NEVER));
		D3D_HR(d3dDevice->SetRenderState(D3DRS_STENCILFAIL,D3DSTENCILOP_REPLACE));
		D3D_HR(d3dDevice->SetRenderState(D3DRS_STENCILREF,1));
		D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_DISABLE));

		D3D_HR(d3dDevice->SetVertexShader(NULL));
		D3D_HR(d3dDevice->SetPixelShader(NULL));
		D3D_HR(d3dDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));

		D3D_HR(d3dDevice->Clear(0,NULL,D3DCLEAR_STENCIL,0,1.0f,0));
		D3D_HR(d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,(void*)vb,sizeof(Vertex)));

		D3D_HR(d3dDevice->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_NOTEQUAL));
		D3D_HR(d3dDevice->SetRenderState(D3DRS_STENCILREF,0));
		D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE2X));
	}

	// Create or resize vertex buffer
	if(!vbPoints || count>vbPointsSize)
	{
		SAFEREL(vbPoints);
		vbPointsSize=0;
		D3D_HR(d3dDevice->CreateVertexBuffer(count*sizeof(MYVERTEX_PRIMITIVE),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFVF_MYVERTEX_PRIMITIVE,D3DPOOL_DEFAULT,&vbPoints,NULL));
		vbPointsSize=count;
	}

	// Copy data
	void* vertices;
	D3D_HR(vbPoints->Lock(0,count*sizeof(MYVERTEX_PRIMITIVE),(void**)&vertices,D3DLOCK_DISCARD));
	for(size_t i=0,j=points.size();i<j;i++)
	{
		PrepPoint& point=points[i];
		MYVERTEX_PRIMITIVE* vertex=(MYVERTEX_PRIMITIVE*)vertices+i;
		vertex->x=(float)point.PointX;
		vertex->y=(float)point.PointY;
		if(point.PointOffsX) vertex->x+=(float)*point.PointOffsX;
		if(point.PointOffsY) vertex->y+=(float)*point.PointOffsY;
		if(zoom)
		{
			vertex->x/=*zoom;
			vertex->y/=*zoom;
		}
		if(offset)
		{
			vertex->x+=offset->X;
			vertex->y+=offset->Y;
		}
		vertex->Diffuse=point.PointColor;
		vertex->z=0.0f;
		vertex->rhw=1.0f;
	}
	D3D_HR(vbPoints->Unlock());

	// Calculate primitive count
	switch(prim)
	{
	case D3DPT_POINTLIST: break;
	case D3DPT_LINELIST: count/=2; break;
	case D3DPT_LINESTRIP: count-=1; break;
	case D3DPT_TRIANGLELIST: count/=3; break;
	case D3DPT_TRIANGLESTRIP: count-=2; break;
	case D3DPT_TRIANGLEFAN: count-=2; break;
	default: break;
	}
	if(count<=0) return false;

	D3D_HR(d3dDevice->SetStreamSource(0,vbPoints,0,sizeof(MYVERTEX_PRIMITIVE)));
	D3D_HR(d3dDevice->SetFVF(D3DFVF_MYVERTEX_PRIMITIVE));

	if(sprDefaultEffect[DEFAULT_EFFECT_POINT])
	{
		// Draw with effect
		EffectEx* effect_ex=sprDefaultEffect[DEFAULT_EFFECT_POINT];
		ID3DXEffect* effect=effect_ex->Effect;

		if(effect_ex->EffectParams) D3D_HR(effect->ApplyParameterBlock(effect_ex->EffectParams));
		D3D_HR(effect->SetTechnique(effect_ex->TechniqueSimple));
		if(effect_ex->IsNeedProcess) Loader3d::EffectProcessVariables(effect_ex,-1);

		UINT passes;
		D3D_HR(effect->Begin(&passes,effect_ex->EffectFlags));
		for(UINT pass=0;pass<passes;pass++)
		{
			if(effect_ex->IsNeedProcess) Loader3d::EffectProcessVariables(effect_ex,pass);

			D3D_HR(effect->BeginPass(pass));
			D3D_HR(d3dDevice->DrawPrimitive(prim,0,count));
			D3D_HR(effect->EndPass());
		}
		D3D_HR(effect->End());
	}
	else
	{
		// Prepare
		D3D_HR(d3dDevice->SetVertexShader(NULL));
		D3D_HR(d3dDevice->SetPixelShader(NULL));
		D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_DISABLE));

		// Draw
		D3D_HR(d3dDevice->DrawPrimitive(prim,0,count));

		// Restore
		if(stencil) D3D_HR(d3dDevice->SetRenderState(D3DRS_STENCILENABLE,FALSE));
		D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE2X));
	}
	return true;
}

bool SpriteManager::Draw3d(int x, int y, float scale, Animation3d* anim3d, FLTRECT* stencil, DWORD color)
{
	// Draw previous sprites
	Flush();

	// Draw 3d
	anim3d->Draw(x,y,scale,stencil,color);

	// Restore 2d stream
	D3D_HR(d3dDevice->SetIndices(pIB));
	D3D_HR(d3dDevice->SetStreamSource(0,pVB,0,sizeof(MYVERTEX)));
	return true;
}

bool SpriteManager::Draw3dSize(FLTRECT rect, bool stretch_up, bool center, Animation3d* anim3d, FLTRECT* stencil, DWORD color)
{
	Flush();

	INTPOINT xy=anim3d->GetBaseBordersPivot();
	INTRECT borders=anim3d->GetBaseBorders();
	float w_real=(float)borders.W();
	float h_real=(float)borders.H();
	float scale=min(rect.W()/w_real,rect.H()/h_real);
	if(scale>1.0f && !stretch_up) scale=1.0f;
	if(center)
	{
		xy.X+=(rect.W()-w_real*scale)/2.0f;
		xy.Y+=(rect.H()-h_real*scale)/2.0f;
	}

	anim3d->Draw(rect.L+(float)xy.X*scale,rect.T+(float)xy.Y*scale,scale,stencil,color);

	// Restore 2d stream
	D3D_HR(d3dDevice->SetIndices(pIB));
	D3D_HR(d3dDevice->SetStreamSource(0,pVB,0,sizeof(MYVERTEX)));
	return true;
}

bool SpriteManager::DrawContours()
{
	if(contoursPS && contoursAdded)
	{
		struct Vertex
		{
			FLOAT x,y,z,rhw;
			float tu,tv;
		} vb[6]=
		{
			{-0.5f                 ,(float)modeHeight-0.5f ,0.0f,1.0f,0.0f,1.0f},
			{-0.5f                 ,-0.5f                  ,0.0f,1.0f,0.0f,0.0f},
			{(float)modeWidth-0.5f ,(float)modeHeight-0.5f ,0.0f,1.0f,1.0f,1.0f},
			{-0.5f                 ,-0.5f                  ,0.0f,1.0f,0.0f,0.0f},
			{(float)modeWidth-0.5f ,-0.5f                  ,0.0f,1.0f,1.0f,0.0f},
			{(float)modeWidth-0.5f ,(float)modeHeight-0.5f ,0.0f,1.0f,1.0f,1.0f},
		};

		D3D_HR(d3dDevice->SetVertexShader(NULL));
		D3D_HR(d3dDevice->SetPixelShader(NULL));
		D3D_HR(d3dDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));
		D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));
		D3D_HR(d3dDevice->SetTexture(0,contoursTexture));

		D3D_HR(d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,(void*)vb,sizeof(Vertex)));

		D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE2X));
		contoursAdded=false;
	}
	else if(spriteContours.Size())
	{
		D3D_HR(d3dDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT)); // Zoom In
		SetCurEffect2D(DEFAULT_EFFECT_GENERIC);
		DrawSprites(spriteContours,false,false,0,-1);
		D3D_HR(d3dDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR)); // Zoom In
		spriteContours.Unvalidate();
	}
	return true;
}

bool SpriteManager::CollectContour(int x, int y, SpriteInfo* si, Sprite* spr)
{
	if(!contoursPS)
	{
		if(!si->Anim3d)
		{
			DWORD contour_id=GetSpriteContour(si,spr);
			if(contour_id)
			{
				Sprite& contour_spr=spriteContours.AddSprite(0,spr->ScrX,spr->ScrY,contour_id,NULL,spr->OffsX,spr->OffsY,NULL,NULL,NULL);
				contour_spr.Color=0xFFAFAFAF;
				if(spr->Contour==Sprite::ContourRed) contour_spr.FlashMask=0xFFFF0000;
				else if(spr->Contour==Sprite::ContourYellow) contour_spr.FlashMask=0xFFFFFF00;
				else
				{
					contour_spr.FlashMask=0xFFFFFFFF;
					contour_spr.Color=spr->ContourColor;
				}
				return true;
			}
		}

		return false;
	}

	// Shader contour
	Animation3d* anim3d=si->Anim3d;
	INTRECT borders=(anim3d?anim3d->GetExtraBorders():INTRECT(x-1,y-1,x+si->Width+1,y+si->Height+1));
	LPDIRECT3DTEXTURE texture=(anim3d?contoursMidTexture:si->Surf->Texture);
	float ws,hs;
	FLTRECT tuv,tuvh;

	if(!anim3d)
	{
		if(borders.L>=modeWidth*GameOpt.SpritesZoom || borders.R<0 || borders.T>=modeHeight*GameOpt.SpritesZoom || borders.B<0) return true;

		if(GameOpt.SpritesZoom==1.0f)
		{
			ws=1.0f/(float)si->Surf->Width;
			hs=1.0f/(float)si->Surf->Height;
			tuv=FLTRECT(si->SprRect.L-ws,si->SprRect.T-hs,si->SprRect.R+ws,si->SprRect.B+hs);
			tuvh=tuv;
		}
		else
		{
			borders(x/GameOpt.SpritesZoom,y/GameOpt.SpritesZoom,(x+si->Width)/GameOpt.SpritesZoom,(y+si->Height)/GameOpt.SpritesZoom);
			struct Vertex
			{
				FLOAT x,y,z,rhw;
				FLOAT tu,tv;
			} vb[6]=
			{
				{(float)borders.L,(float)borders.B,1.0f,1.0f,si->SprRect.L,si->SprRect.B},
				{(float)borders.L,(float)borders.T,1.0f,1.0f,si->SprRect.L,si->SprRect.T},
				{(float)borders.R,(float)borders.B,1.0f,1.0f,si->SprRect.R,si->SprRect.B},
				{(float)borders.L,(float)borders.T,1.0f,1.0f,si->SprRect.L,si->SprRect.T},
				{(float)borders.R,(float)borders.T,1.0f,1.0f,si->SprRect.R,si->SprRect.T},
				{(float)borders.R,(float)borders.B,1.0f,1.0f,si->SprRect.R,si->SprRect.B},
			};

			borders.L--;
			borders.T--;
			borders.R++;
			borders.B++;

			LPDIRECT3DSURFACE old_rt,old_ds;
			D3D_HR(d3dDevice->GetRenderTarget(0,&old_rt));
			D3D_HR(d3dDevice->GetDepthStencilSurface(&old_ds));
			D3D_HR(d3dDevice->SetDepthStencilSurface(NULL));
			D3D_HR(d3dDevice->SetRenderTarget(0,contoursMidTextureSurf));
			D3D_HR(d3dDevice->SetVertexShader(NULL));
			D3D_HR(d3dDevice->SetPixelShader(NULL));
			D3D_HR(d3dDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));
			D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));
			D3D_HR(d3dDevice->SetTexture(0,si->Surf->Texture));

			D3DRECT clear_r={borders.L-1,borders.T-1,borders.R+1,borders.B+1};
			D3D_HR(d3dDevice->Clear(1,&clear_r,D3DCLEAR_TARGET,0,1.0f,0));
			D3D_HR(d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,(void*)vb,sizeof(Vertex)));

			D3D_HR(d3dDevice->SetRenderTarget(0,old_rt));
			D3D_HR(d3dDevice->SetDepthStencilSurface(old_ds));
			old_rt->Release();
			old_ds->Release();

			float w=modeWidth;
			float h=modeHeight;
			ws=1.0f/modeWidth;
			hs=1.0f/modeHeight;
			tuv=FLTRECT((float)borders.L/w,(float)borders.T/h,(float)borders.R/w,(float)borders.B/h);
			tuvh=tuv;
			texture=contoursMidTexture;
		}
	}
	else
	{
		if(borders.L>=modeWidth || borders.R<0 || borders.T>=modeHeight || borders.B<0) return true;

		INTRECT init_borders=borders;
		if(borders.L<=0) borders.L=1;
		if(borders.T<=0) borders.T=1;
		if(borders.R>=modeWidth) borders.R=modeWidth-1;
		if(borders.B>=modeHeight) borders.B=modeHeight-1;

		float w=modeWidth;
		float h=modeHeight;
		tuv.L=(float)borders.L/w;
		tuv.T=(float)borders.T/h;
		tuv.R=(float)borders.R/w;
		tuv.B=(float)borders.B/h;
		tuvh.T=(float)init_borders.T/h;
		tuvh.B=(float)init_borders.B/h;

		ws=0.1f/modeWidth;
		hs=0.1f/modeHeight;

		// Render to contours texture
		struct Vertex
		{
			FLOAT x,y,z,rhw;
			DWORD diffuse;
		} vb[6]=
		{
			{(float)borders.L-0.5f,(float)borders.B-0.5f,0.99999f,1.0f,0xFFFF00FF},
			{(float)borders.L-0.5f,(float)borders.T-0.5f,0.99999f,1.0f,0xFFFF00FF},
			{(float)borders.R-0.5f,(float)borders.B-0.5f,0.99999f,1.0f,0xFFFF00FF},
			{(float)borders.L-0.5f,(float)borders.T-0.5f,0.99999f,1.0f,0xFFFF00FF},
			{(float)borders.R-0.5f,(float)borders.T-0.5f,0.99999f,1.0f,0xFFFF00FF},
			{(float)borders.R-0.5f,(float)borders.B-0.5f,0.99999f,1.0f,0xFFFF00FF},
		};

		LPDIRECT3DSURFACE old_rt;
		D3D_HR(d3dDevice->GetRenderTarget(0,&old_rt));
		D3D_HR(d3dDevice->SetRenderTarget(0,contours3dRT));

		D3D_HR(d3dDevice->SetRenderState(D3DRS_ZENABLE,TRUE));
		D3D_HR(d3dDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_NOTEQUAL));
		D3D_HR(d3dDevice->SetVertexShader(NULL));
		D3D_HR(d3dDevice->SetPixelShader(NULL));
		D3D_HR(d3dDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));
		D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_DISABLE));

		D3DRECT clear_r={borders.L-2,borders.T-2,borders.R+2,borders.B+2};
		D3D_HR(d3dDevice->Clear(1,&clear_r,D3DCLEAR_TARGET,0,1.0f,0));
		D3D_HR(d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,(void*)vb,sizeof(Vertex)));

		D3D_HR(d3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE));
		D3D_HR(d3dDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL));
		D3D_HR(d3dDevice->SetRenderTarget(0,old_rt));
		old_rt->Release();

		// Copy to mid surface
		RECT r={borders.L-1,borders.T-1,borders.R+1,borders.B+1};
		D3D_HR(d3dDevice->StretchRect(contours3dRT,&r,contoursMidTextureSurf,&r,D3DTEXF_NONE));
	}

	// Calculate contour color
	DWORD contour_color=0;
	if(spr->Contour==Sprite::ContourRed) contour_color=0xFFAF0000;
	else if(spr->Contour==Sprite::ContourYellow)
	{
		contour_color=0xFFAFAF00;
		tuvh.T=-1.0f; // Disable flashing
		tuvh.B=-1.0f;
	}
	else if(spr->Contour==Sprite::ContourCustom) contour_color=spr->ContourColor;
	else contour_color=0xFFAFAFAF;

	static float color_offs=0.0f;
	static DWORD last_tick=0;
	DWORD tick=Timer::FastTick();
	if(tick-last_tick>=50)
	{
		color_offs-=0.05f;
		if(color_offs<-1.0f) color_offs=1.0f;
		last_tick=tick;
	}

	// Draw contour
	struct Vertex
	{
		FLOAT x,y,z,rhw;
		float tu,tv;
	} vb[6]=
	{
		{(float)borders.L-0.5f,(float)borders.B-0.5f,0.0f,1.0f,tuv.L,tuv.B},
		{(float)borders.L-0.5f,(float)borders.T-0.5f,0.0f,1.0f,tuv.L,tuv.T},
		{(float)borders.R-0.5f,(float)borders.B-0.5f,0.0f,1.0f,tuv.R,tuv.B},
		{(float)borders.L-0.5f,(float)borders.T-0.5f,0.0f,1.0f,tuv.L,tuv.T},
		{(float)borders.R-0.5f,(float)borders.T-0.5f,0.0f,1.0f,tuv.R,tuv.T},
		{(float)borders.R-0.5f,(float)borders.B-0.5f,0.0f,1.0f,tuv.R,tuv.B},
	};

	LPDIRECT3DSURFACE ds;
	D3D_HR(d3dDevice->GetDepthStencilSurface(&ds));
	D3D_HR(d3dDevice->SetDepthStencilSurface(NULL));
	LPDIRECT3DSURFACE old_rt;
	D3D_HR(d3dDevice->GetRenderTarget(0,&old_rt));
	D3D_HR(d3dDevice->SetRenderTarget(0,contoursTextureSurf));
	D3D_HR(d3dDevice->SetTexture(0,texture));
	D3D_HR(d3dDevice->SetVertexShader(NULL));
	D3D_HR(d3dDevice->SetPixelShader(contoursPS));
	D3D_HR(d3dDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));
	D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));

	if(contoursConstWidthStep) D3D_HR(contoursCT->SetFloat(d3dDevice,contoursConstWidthStep,ws));
	if(contoursConstHeightStep) D3D_HR(contoursCT->SetFloat(d3dDevice,contoursConstHeightStep,hs));
	float sb[4]={tuv.L,tuv.T,tuv.R,tuv.B};
	if(contoursConstSpriteBorders) D3D_HR(contoursCT->SetFloatArray(d3dDevice,contoursConstSpriteBorders,sb,4));
	float sbh[3]={tuvh.T,tuvh.B,tuvh.B-tuvh.T};
	if(contoursConstSpriteBordersHeight) D3D_HR(contoursCT->SetFloatArray(d3dDevice,contoursConstSpriteBordersHeight,sbh,3));
	float cc[4]={float((contour_color>>16)&0xFF)/255.0f,float((contour_color>>8)&0xFF)/255.0f,float((contour_color)&0xFF)/255.0f,float((contour_color>>24)&0xFF)/255.0f};
	if(contoursConstContourColor) D3D_HR(contoursCT->SetFloatArray(d3dDevice,contoursConstContourColor,cc,4));
	if(contoursConstContourColorOffs) D3D_HR(contoursCT->SetFloat(d3dDevice,contoursConstContourColorOffs,color_offs));

	if(!contoursAdded) D3D_HR(d3dDevice->Clear(0,NULL,D3DCLEAR_TARGET,0,0.9f,0));
	D3D_HR(d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,(void*)vb,sizeof(Vertex)));

	// Restore 2d stream
	D3D_HR(d3dDevice->SetDepthStencilSurface(ds));
	ds->Release();
	D3D_HR(d3dDevice->SetRenderTarget(0,old_rt));
	old_rt->Release();
	D3D_HR(d3dDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE2X));
	contoursAdded=true;
	return true;
}

DWORD SpriteManager::GetSpriteContour(SpriteInfo* si, Sprite* spr)
{
	// Find created
	DWORD spr_id=(spr->PSprId?*spr->PSprId:spr->SprId);
	DwordMapIt it=createdSpriteContours.find(spr_id);
	if(it!=createdSpriteContours.end()) return (*it).second;

	// Create new
	LPDIRECT3DSURFACE surf;
	D3D_HR(si->Surf->Texture->GetSurfaceLevel(0,&surf));
	D3DSURFACE_DESC desc;
	D3D_HR(surf->GetDesc(&desc));
	RECT r={desc.Width*si->SprRect.L,desc.Height*si->SprRect.T,desc.Width*si->SprRect.R,desc.Height*si->SprRect.B};
	D3DLOCKED_RECT lr;
	D3D_HR(surf->LockRect(&lr,&r,D3DLOCK_READONLY));

	DWORD sw=si->Width;
	DWORD sh=si->Height;
	DWORD iw=sw+2;
	DWORD ih=sh+2;

	// Create FOnline fast format
	DWORD size=12+ih*iw*4;
	BYTE* data=new BYTE[size];
	memset(data,0,size);
	*((DWORD*)data)=MAKEFOURCC('F','0','F','A'); // FOnline FAst
	*((DWORD*)data+1)=iw;
	*((DWORD*)data+2)=ih;
	DWORD* ptr=(DWORD*)data+3+iw+1;

	// Write contour
	WriteContour4(ptr,iw,lr,sw,sh,D3DCOLOR_XRGB(0x7F,0x7F,0x7F));
	D3D_HR(surf->UnlockRect());
	surf->Release();

	// End
	SpriteInfo* contour_si=new SpriteInfo();
	contour_si->OffsX=si->OffsX;
	contour_si->OffsY=si->OffsY+1;
	int st=SurfType;
	SurfType=si->Surf->Type;
	DWORD result=FillSurfaceFromMemory(contour_si,data,size);
	SurfType=st;
	delete[] data;
	createdSpriteContours.insert(DwordMapVal(spr_id,result));
	return result;
}

#define SET_IMAGE_POINT(x,y) *(buf+(y)*buf_w+(x))=color
void SpriteManager::WriteContour4(DWORD* buf, DWORD buf_w, D3DLOCKED_RECT& r, DWORD w, DWORD h, DWORD color)
{
	for(DWORD y=0;y<h;y++)
	{
		for(DWORD x=0;x<w;x++)
		{
			DWORD p=SURF_POINT(r,x,y);
			if(!p) continue;
			if(!x || !SURF_POINT(r,x-1,y)) SET_IMAGE_POINT(x-1,y);
			if(x==w-1 || !SURF_POINT(r,x+1,y)) SET_IMAGE_POINT(x+1,y);
			if(!y || !SURF_POINT(r,x,y-1)) SET_IMAGE_POINT(x,y-1);
			if(y==h-1 || !SURF_POINT(r,x,y+1)) SET_IMAGE_POINT(x,y+1);
		}
	}
}

void SpriteManager::WriteContour8(DWORD* buf, DWORD buf_w, D3DLOCKED_RECT& r, DWORD w, DWORD h, DWORD color)
{
	for(DWORD y=0;y<h;y++)
	{
		for(DWORD x=0;x<w;x++)
		{
			DWORD p=SURF_POINT(r,x,y);
			if(!p) continue;
			if(!x || !SURF_POINT(r,x-1,y)) SET_IMAGE_POINT(x-1,y);
			if(x==w-1 || !SURF_POINT(r,x+1,y)) SET_IMAGE_POINT(x+1,y);
			if(!y || !SURF_POINT(r,x,y-1)) SET_IMAGE_POINT(x,y-1);
			if(y==h-1 || !SURF_POINT(r,x,y+1)) SET_IMAGE_POINT(x,y+1);
			if((!x && !y) || !SURF_POINT(r,x-1,y-1)) SET_IMAGE_POINT(x-1,y-1);
			if((x==w-1 && !y) || !SURF_POINT(r,x+1,y-1)) SET_IMAGE_POINT(x+1,y-1);
			if((x==w-1 && y==h-1) || !SURF_POINT(r,x+1,y+1)) SET_IMAGE_POINT(x+1,y+1);
			if((y==h-1 && !x) || !SURF_POINT(r,x-1,y+1)) SET_IMAGE_POINT(x-1,y+1);
		}
	}
}

/************************************************************************/
/* Fonts                                                                */
/************************************************************************/
#define FONT_BUF_LEN	    (0x5000)
#define FONT_MAX_LINES	    (1000)
#define FORMAT_TYPE_DRAW    (0)
#define FORMAT_TYPE_SPLIT   (1)
#define FORMAT_TYPE_LCOUNT  (2)

struct Letter
{
	short W;
	short H;
	short OffsW;
	short OffsH;
	short XAdvance;

	Letter():W(0),H(0),OffsW(0),OffsH(0),XAdvance(0){};
};

struct Font
{
	LPDIRECT3DTEXTURE FontTex;
	LPDIRECT3DTEXTURE FontTexBordered;

	Letter Letters[256];
	int SpaceWidth;
	int MaxLettHeight;
	int EmptyVer;
	FLOAT ArrXY[256][4];
	EffectEx* Effect;

	Font()
	{
		FontTex=NULL;
		FontTexBordered=NULL;
		SpaceWidth=0;
		MaxLettHeight=0;
		EmptyVer=0;
		for(int i=0;i<256;i++)
		{
			ArrXY[i][0]=0.0f;
			ArrXY[i][1]=0.0f;
			ArrXY[i][2]=0.0f;
			ArrXY[i][3]=0.0f;
		}
		Effect=NULL;
	}
};
typedef vector<Font*> FontVec;
typedef vector<Font*>::iterator FontVecIt;

FontVec Fonts;

int DefFontIndex=-1;
DWORD DefFontColor=0;

Font* GetFont(int num)
{
	if(num<0) num=DefFontIndex;
	if(num<0 || num>=Fonts.size()) return NULL;
	return Fonts[num];
}

struct FontFormatInfo
{
	Font* CurFont;
	DWORD Flags;
	INTRECT Rect;
	char Str[FONT_BUF_LEN];
	char* PStr;
	DWORD LinesAll;
	DWORD LinesInRect;
	int CurX;
	int CurY;
	DWORD ColorDots[FONT_BUF_LEN];
	short LineWidth[FONT_MAX_LINES];
	WORD LineSpaceWidth[FONT_MAX_LINES];
	DWORD OffsColDots;
	DWORD DefColor;
	StrVec* StrLines;
	bool IsError;

	void Init(Font* font, DWORD flags, INTRECT& rect, const char* str_in)
	{
		CurFont=font;
		Flags=flags;
		LinesAll=1;
		LinesInRect=0;
		IsError=false;
		CurX=0;
		CurY=0;
		Rect=rect;
		ZeroMemory(ColorDots,sizeof(ColorDots));
		ZeroMemory(LineWidth,sizeof(LineWidth));
		ZeroMemory(LineSpaceWidth,sizeof(LineSpaceWidth));
		OffsColDots=0;
		StringCopy(Str,str_in);
		PStr=Str;
		DefColor=COLOR_TEXT;
		StrLines=NULL;
	}

	FontFormatInfo& operator=(const FontFormatInfo& _fi)
	{
		CurFont=_fi.CurFont;
		Flags=_fi.Flags;
		LinesAll=_fi.LinesAll;
		LinesInRect=_fi.LinesInRect;
		IsError=_fi.IsError;
		CurX=_fi.CurX;
		CurY=_fi.CurY;
		Rect=_fi.Rect;
		memcpy(Str,_fi.Str,sizeof(Str));
		memcpy(ColorDots,_fi.ColorDots,sizeof(ColorDots));
		memcpy(LineWidth,_fi.LineWidth,sizeof(LineWidth));
		memcpy(LineSpaceWidth,_fi.LineSpaceWidth,sizeof(LineSpaceWidth));
		OffsColDots=_fi.OffsColDots;
		DefColor=_fi.DefColor;
		PStr=Str;
		StrLines=_fi.StrLines;
		return *this;
	}
};

void SpriteManager::SetDefaultFont(int index, DWORD color)
{
	DefFontIndex=index;
	DefFontColor=color;
}

void SpriteManager::SetFontEffect(int index, EffectEx* effect)
{
	Font* font=GetFont(index);
	if(font) font->Effect=effect;
}

bool SpriteManager::LoadFontOld(int index, const char* font_name, int size_mod)
{
	if(index<0)
	{
		WriteLog(__FUNCTION__" - Invalid index.\n");
		return false;
	}

	Font font;
	int tex_w=256*size_mod;
	int tex_h=256*size_mod;

	BYTE* data=new BYTE[tex_w*tex_h*4]; // TODO: Leak
	if(!data)
	{
		WriteLog(__FUNCTION__" - Data allocation fail.\n");
		return false;
	}
	ZeroMemory(data,tex_w*tex_h*4);

	FileManager fm;
	if(!fm.LoadFile(Str::Format("%s.bmp",font_name),PT_FONTS))
	{
		WriteLog(__FUNCTION__" - File<%s> not found.\n",Str::Format("%s.bmp",font_name));
		delete[] data;
		return false;
	}

	LPDIRECT3DTEXTURE image=NULL;
	D3D_HR(D3DXCreateTextureFromFileInMemoryEx(d3dDevice,fm.GetBuf(),fm.GetFsize(),D3DX_DEFAULT,D3DX_DEFAULT,1,0,
		D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,D3DX_DEFAULT,D3DX_DEFAULT,D3DCOLOR_ARGB(255,0,0,0),NULL,NULL,&image));

	D3DLOCKED_RECT lr;
	D3D_HR(image->LockRect(0,&lr,NULL,D3DLOCK_READONLY));

	if(!fm.LoadFile(Str::Format("%s.fnt0",font_name),PT_FONTS))
	{
		WriteLog(__FUNCTION__" - File<%s> not found.\n",Str::Format("%s.fnt0",font_name));
		delete[] data;
		SAFEREL(image);
		return false;
	}

	int empty_hor=fm.GetBEDWord();
	font.EmptyVer=fm.GetBEDWord();
	font.MaxLettHeight=fm.GetBEDWord();
	font.SpaceWidth=fm.GetBEDWord();

	struct LetterOldFont
	{
		WORD Dx;
		WORD Dy;
		BYTE W;
		BYTE H;
		short OffsH;
	} letters[256];

	if(!fm.CopyMem(letters,sizeof(LetterOldFont)*256))
	{
		WriteLog(__FUNCTION__" - Incorrect size in file<%s>.\n",Str::Format("%s.fnt0",font_name));
		delete[] data;
		SAFEREL(image);
		return false;
	}

	D3DSURFACE_DESC sd;
	D3D_HR(image->GetLevelDesc(0,&sd));
	DWORD wd=sd.Width;

	int cur_x=0;
	int cur_y=0;
	for(int i=0;i<256;i++)
	{
		LetterOldFont& letter_old=letters[i];
		Letter& letter=font.Letters[i];

		int w=letter_old.W;
		int h=letter_old.H;
		if(!w || !h) continue;

		letter.W=letter_old.W;
		letter.H=letter_old.H;
		letter.OffsH=letter.OffsH;
		letter.XAdvance=w+empty_hor;

		if(cur_x+w+2>=tex_w)
		{
			cur_x=0;
			cur_y+=font.MaxLettHeight+2;
			if(cur_y+font.MaxLettHeight+2>=tex_h)
			{
				delete[] data;
				SAFEREL(image);
				//WriteLog("<%s> growed to %d\n",font_name,size_mod*2);
				return LoadFontOld(index,font_name,size_mod*2);
			}
		}

		for(int j=0;j<h;j++) memcpy(data+(cur_y+j+1)*tex_w*4+(cur_x+1)*4,(BYTE*)lr.pBits+(letter_old.Dy+j)*sd.Width*4+letter_old.Dx*4,w*4);

		font.ArrXY[i][0]=(FLOAT)cur_x/tex_w;
		font.ArrXY[i][1]=(FLOAT)cur_y/tex_h;
		font.ArrXY[i][2]=(FLOAT)(cur_x+w+2)/tex_w;
		font.ArrXY[i][3]=(FLOAT)(cur_y+h+2)/tex_h;
		cur_x+=w+2;
	}

	D3D_HR(image->UnlockRect(0));
	SAFEREL(image);

	// Create texture
	D3D_HR(D3DXCreateTexture(d3dDevice,tex_w,tex_h,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&font.FontTex));
	D3D_HR(font.FontTex->LockRect(0,&lr,NULL,0));
	memcpy(lr.pBits,data,tex_w*tex_h*4);
	WriteContour8((DWORD*)data,tex_w,lr,tex_w,tex_h,D3DCOLOR_ARGB(0xFF,0,0,0)); // Create border
	D3D_HR(font.FontTex->UnlockRect(0));

	// Create bordered texture
	D3D_HR(D3DXCreateTexture(d3dDevice,tex_w,tex_h,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&font.FontTexBordered));
	D3D_HR(font.FontTexBordered->LockRect(0,&lr,NULL,0));
	memcpy(lr.pBits,data,tex_w*tex_h*4);
	D3D_HR(font.FontTexBordered->UnlockRect(0));
	delete[] data;

	// Register
	if(index>=Fonts.size()) Fonts.resize(index+1);
	SAFEDEL(Fonts[index]);
	Fonts[index]=new Font(font);
	return true;
}

bool SpriteManager::LoadFontAAF(int index, const char* font_name, int size_mod)
{
	int tex_w=256*size_mod;
	int tex_h=256*size_mod;

	// Read file in buffer
	if(index<0)
	{
		WriteLog(__FUNCTION__" - Invalid index.\n");
		return false;
	}

	Font font;
	FileManager fm;

	if(!fm.LoadFile(Str::Format("%s.aaf",font_name),PT_FONTS))
	{
		WriteLog(__FUNCTION__" - File<%s> not found.\n",Str::Format("%s.aaf",font_name));
		return false;
	}

	// Check signature
	DWORD sign=fm.GetBEDWord();
	if(sign!=MAKEFOURCC('F','F','A','A'))
	{
		WriteLog(__FUNCTION__" - Signature AAFF not found.\n");
		return false;
	}

	// Read params
	// Максимальная высота изображения символа, включая надстрочные и подстрочные элементы.
	font.MaxLettHeight=fm.GetBEWord();
	// Горизонтальный зазор.
	// Зазор (в пикселах) между соседними изображениями символов.
	int empty_hor=fm.GetBEWord();
	// Ширина пробела.
	// Ширина символа 'Пробел'.
	font.SpaceWidth=fm.GetBEWord();
	// Вертикальный зазор.
	// Зазор (в пикселах) между двумя строками символов.
	font.EmptyVer=fm.GetBEWord();

	// Write font image
	const DWORD pix_light[9]={0x22808080,0x44808080,0x66808080,0x88808080,0xAA808080,0xDD808080,0xFF808080,0xFF808080,0xFF808080};
	BYTE* data=new BYTE[tex_w*tex_h*4];
	if(!data)
	{
		WriteLog(__FUNCTION__" - Data allocation fail.\n");
		return false;
	}
	ZeroMemory(data,tex_w*tex_h*4);
	BYTE* begin_buf=fm.GetBuf();
	int cur_x=0;
	int cur_y=0;

	for(int i=0;i<256;i++)
	{
		Letter& l=font.Letters[i];

		l.W=fm.GetBEWord();
		l.H=fm.GetBEWord();
		DWORD offs=fm.GetBEDWord();
		l.OffsH=-(font.MaxLettHeight-l.H);

		if(cur_x+l.W+2>=tex_w)
		{
			cur_x=0;
			cur_y+=font.MaxLettHeight+2;
			if(cur_y+font.MaxLettHeight+2>=tex_h)
			{
				delete[] data;
				//WriteLog("<%s> growed to %d\n",font_name,size_mod*2);
				return LoadFontAAF(index,font_name,size_mod*2);
			}
		}

		BYTE* pix=&begin_buf[offs+0x080C];

		for(int h=0;h<l.H;h++)
		{
			DWORD* cur_data=(DWORD*)(data+(cur_y+h+1)*tex_w*4+(cur_x+1)*4);

			for(int w=0;w<l.W;w++,pix++,cur_data++)
			{
				int val=*pix;
				if(val>9) val=0;
				if(!val) continue;
				*cur_data=pix_light[val-1];
			}
		}

		l.XAdvance=l.W+empty_hor;

		font.ArrXY[i][0]=(FLOAT)cur_x/tex_w;
		font.ArrXY[i][1]=(FLOAT)cur_y/tex_h;
		font.ArrXY[i][2]=(FLOAT)(cur_x+int(l.W)+2)/tex_w;
		font.ArrXY[i][3]=(FLOAT)(cur_y+int(l.H)+2)/tex_h;
		cur_x+=l.W+2;
	}

	// Create texture
	D3D_HR(D3DXCreateTexture(d3dDevice,tex_w,tex_h,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&font.FontTex));
	D3DLOCKED_RECT lr;
	D3D_HR(font.FontTex->LockRect(0,&lr,NULL,0));
	memcpy(lr.pBits,data,tex_w*tex_h*4);
	WriteContour8((DWORD*)data,tex_w,lr,tex_w,tex_h,D3DCOLOR_ARGB(0xFF,0,0,0)); // Create border
	D3D_HR(font.FontTex->UnlockRect(0));

	// Create bordered texture
	D3D_HR(D3DXCreateTexture(d3dDevice,tex_w,tex_h,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&font.FontTexBordered));
	D3D_HR(font.FontTexBordered->LockRect(0,&lr,NULL,0));
	memcpy(lr.pBits,data,tex_w*tex_h*4);
	D3D_HR(font.FontTexBordered->UnlockRect(0));
	delete[] data;

	// Register
	if(index>=Fonts.size()) Fonts.resize(index+1);
	SAFEDEL(Fonts[index]);
	Fonts[index]=new Font(font);
	return true;
}

bool SpriteManager::LoadFontBMF(int index, const char* font_name)
{
	if(index<0)
	{
		WriteLog(__FUNCTION__" - Invalid index.\n");
		return false;
	}

	Font font;
	FileManager fm;
	FileManager fm_tex;

	if(!fm.LoadFile(Str::Format("%s.fnt",font_name),PT_FONTS))
	{
		WriteLog(__FUNCTION__" - Font file<%s> not found.\n",Str::Format("%s.fnt",font_name));
		return false;
	}

	DWORD signature=fm.GetLEDWord();
	if(signature!=MAKEFOURCC('B','M','F',3))
	{
		WriteLog(__FUNCTION__" - Invalid signature of font<%s>.\n",font_name);
		return false;
	}

	// Info
	fm.GetByte();
	DWORD block_len=fm.GetLEDWord();
	DWORD next_block=block_len+fm.GetCurPos()+1;

	fm.GoForward(7);
	if(fm.GetByte()!=1 || fm.GetByte()!=1 || fm.GetByte()!=1 || fm.GetByte()!=1)
	{
		WriteLog(__FUNCTION__" - Wrong padding in font<%s>.\n",font_name);
		return false;
	}

	// Common
	fm.SetCurPos(next_block);
	block_len=fm.GetLEDWord();
	next_block=block_len+fm.GetCurPos()+1;

	int line_height=fm.GetLEWord();
	int base_height=fm.GetLEWord();
	font.MaxLettHeight=base_height;

	int tex_w=fm.GetLEWord();
	int tex_h=fm.GetLEWord();

	if(fm.GetLEWord()!=1)
	{
		WriteLog(__FUNCTION__" - Texture for font<%s> must be single.\n",font_name);
		return false;
	}

	// Pages
	fm.SetCurPos(next_block);
	block_len=fm.GetLEDWord();
	next_block=block_len+fm.GetCurPos()+1;

	char texture_name[MAX_FOPATH]={0};
	fm.GetStr(texture_name);

	if(!fm_tex.LoadFile(texture_name,PT_FONTS))
	{
		WriteLog(__FUNCTION__" - Texture file<%s> not found.\n",texture_name);
		return false;
	}

	// Chars
	fm.SetCurPos(next_block);

	int count=fm.GetLEDWord()/20;
	for(int i=0;i<count;i++)
	{
		// Read data
		int id=fm.GetLEDWord();
		int x=fm.GetLEWord();
		int y=fm.GetLEWord();
		int w=fm.GetLEWord();
		int h=fm.GetLEWord();
		int ox=fm.GetLEWord();
		int oy=fm.GetLEWord();
		int xa=fm.GetLEWord();
		fm.GoForward(2);

		if(id<0 || id>255) continue;

		// Fill data
		Letter& let=font.Letters[id];
		let.W=w-2;
		let.H=h-2;
		let.OffsW=-ox;
		let.OffsH=-oy+(line_height-base_height);
		let.XAdvance=xa+1;

		// Texture coordinates
		font.ArrXY[id][0]=(FLOAT)x/tex_w;
		font.ArrXY[id][1]=(FLOAT)y/tex_h;
		font.ArrXY[id][2]=(FLOAT)(x+w)/tex_w;
		font.ArrXY[id][3]=(FLOAT)(y+h)/tex_h;
	}

	font.EmptyVer=1;
	font.SpaceWidth=font.Letters[' '].XAdvance;

	// Create texture
	D3D_HR(D3DXCreateTextureFromFileInMemoryEx(d3dDevice,fm_tex.GetBuf(),fm_tex.GetFsize(),D3DX_DEFAULT,D3DX_DEFAULT,1,0,
		D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,D3DX_DEFAULT,D3DX_DEFAULT,D3DCOLOR_ARGB(255,0,0,0),NULL,NULL,&font.FontTex));

	// Create bordered texture
	D3D_HR(D3DXCreateTexture(d3dDevice,tex_w,tex_h,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&font.FontTexBordered));

	D3DLOCKED_RECT lr,lrb;
	D3D_HR(font.FontTex->LockRect(0,&lr,NULL,0));
	D3D_HR(font.FontTexBordered->LockRect(0,&lrb,NULL,0));

	for(DWORD y=0;y<tex_h;y++)
		for(DWORD x=0;x<tex_w;x++)
			if(SURF_POINT(lr,x,y)&0xFF000000)
				SURF_POINT(lr,x,y)=(SURF_POINT(lr,x,y)&0xFF000000)|(0x7F7F7F);
			else
				SURF_POINT(lr,x,y)=0;

	memcpy(lrb.pBits,lr.pBits,lr.Pitch*tex_h);

	for(DWORD y=0;y<tex_h;y++)
		for(DWORD x=0;x<tex_w;x++)
			if(SURF_POINT(lr,x,y))
				for(int xx=-1;xx<=1;xx++)
					for(int yy=-1;yy<=1;yy++)
						if(!SURF_POINT(lrb,x+xx,y+yy))
							SURF_POINT(lrb,x+xx,y+yy)=0xFF000000;

	D3D_HR(font.FontTexBordered->UnlockRect(0));
	D3D_HR(font.FontTex->UnlockRect(0));

	// Register
	if(index>=Fonts.size()) Fonts.resize(index+1);
	SAFEDEL(Fonts[index]);
	Fonts[index]=new Font(font);
	return true;
}

void FormatText(FontFormatInfo& fi, int fmt_type)
{
	char* str=fi.PStr;
	DWORD flags=fi.Flags;
	Font* font=fi.CurFont;
	INTRECT& r=fi.Rect;
	int& curx=fi.CurX;
	int& cury=fi.CurY;
	DWORD& offs_col=fi.OffsColDots;

	if(fmt_type!=FORMAT_TYPE_DRAW && fmt_type!=FORMAT_TYPE_LCOUNT && fmt_type!=FORMAT_TYPE_SPLIT)
	{
		fi.IsError=true;
		return;
	}

	if(fmt_type==FORMAT_TYPE_SPLIT && !fi.StrLines)
	{
		fi.IsError=true;
		return;
	}

	// Colorize
	DWORD* dots=NULL;
	DWORD d_offs=0;
	char* str_=str;
	char* big_buf=Str::GetBigBuf();
	big_buf[0]=0;
	if(fmt_type==FORMAT_TYPE_DRAW && FLAG(flags,FT_COLORIZE)) dots=fi.ColorDots;

	while(*str_)
	{
		char* s0=str_;
		Str::GoTo(str_,'|');
		char* s1=str_;
		Str::GoTo(str_,' ');
		char* s2=str_;

		// TODO: optimize
		// if(!_str[0] && !*s1) break;
		if(dots)
		{
			DWORD d_len=(DWORD)s2-(DWORD)s1+1;
			DWORD d=strtoul(s1+1,NULL,0);

			dots[(DWORD)s1-(DWORD)str-d_offs]=d;
			d_offs+=d_len;
		}

		*s1=0;
		StringAppend(big_buf,0x10000,s0);

		if(!*str_) break;
		str_++;
	}

	StringCopy(str,FONT_BUF_LEN,big_buf);

	// Skip lines
	DWORD skip_line=(FLAG(flags,FT_SKIPLINES(0))?flags>>16:0);

	// Format
	curx=r.L;
	cury=r.T;

	for(int i=0;str[i];i++)
	{
		int lett_len;
		switch(str[i])
		{
		case '\r': continue;
		case ' ': lett_len=font->SpaceWidth; break;
		case '\t': lett_len=font->SpaceWidth*4; break;
		default: lett_len=font->Letters[(BYTE)str[i]].XAdvance; break;
		}

		if(curx+lett_len>r.R)
		{
			if(fmt_type==FORMAT_TYPE_DRAW && FLAG(flags,FT_NOBREAK))
			{
				str[i]='\0';
				break;
			}
			else if(FLAG(flags,FT_NOBREAK_LINE))
			{
				int j=i;
				for(;str[j];j++)
				{
					if(str[j]=='\n') break;
				}

				Str::EraseInterval(&str[i],j-i);
				if(fmt_type==FORMAT_TYPE_DRAW) for(int k=i,l=MAX_FOTEXT-(j-i);k<l;k++) fi.ColorDots[k]=fi.ColorDots[k+(j-i)];
			}
			else if(str[i]!='\n')
			{
				int j=i;
				for(;j>=0;j--)
				{
					if(str[j]==' ' || str[j]=='\t')
					{
						str[j]='\n';
						i=j;
						break;
					}
					else if(str[j]=='\n')
					{
						j=-1;
						break;
					}
				}

				if(j<0)
				{
					Str::Insert(&str[i],"\n");
					if(fmt_type==FORMAT_TYPE_DRAW) for(int k=MAX_FOTEXT-1;k>i;k--) fi.ColorDots[k]=fi.ColorDots[k-1];
				}

				if(FLAG(flags,FT_ALIGN) && !skip_line)
				{
					fi.LineSpaceWidth[fi.LinesAll-1]=1;
					// Erase next first spaces
					int ii=i+1;
					for(j=ii;;j++) if(str[j]!=' ') break;
					if(j>ii)
					{
						Str::EraseInterval(&str[ii],j-ii);
						if(fmt_type==FORMAT_TYPE_DRAW) for(int k=ii,l=MAX_FOTEXT-(j-ii);k<l;k++) fi.ColorDots[k]=fi.ColorDots[k+(j-ii)];
					}
				}
			}
		}

		switch(str[i])
		{
		case '\n':
			if(!skip_line)
			{
				cury+=font->MaxLettHeight+font->EmptyVer;
				if(cury+font->MaxLettHeight>r.B && !fi.LinesInRect) fi.LinesInRect=fi.LinesAll;

				if(fmt_type==FORMAT_TYPE_DRAW) 
				{
					if(fi.LinesInRect && !FLAG(flags,FT_UPPER))
					{
						//fi.LinesAll++;
						str[i]='\0';
						break;
					}
				}
				else if(fmt_type==FORMAT_TYPE_SPLIT)
				{
					if(fi.LinesInRect && !(fi.LinesAll%fi.LinesInRect))
					{
						str[i]='\0';
						(*fi.StrLines).push_back(str);
						str=&str[i+1];
						i=-1;
					}
				}

				if(str[i+1]) fi.LinesAll++;
			}
			else
			{
				skip_line--;
				Str::EraseInterval(str,i+1);
				offs_col+=i+1;
				//	if(fmt_type==FORMAT_TYPE_DRAW)
				//		for(int k=0,l=MAX_FOTEXT-(i+1);k<l;k++) fi.ColorDots[k]=fi.ColorDots[k+i+1];
				i=-1;
			}

			curx=r.L;
			continue;
		case '\0':
			break;
		default:
			curx+=lett_len;
			continue;
		}

		if(!str[i]) break;
	}

	if(skip_line)
	{
		fi.IsError=true;
		return;
	}

	if(!fi.LinesInRect) fi.LinesInRect=fi.LinesAll;

	if(fi.LinesAll>FONT_MAX_LINES)
	{
		fi.IsError=true;
		return;
	}

	if(fmt_type==FORMAT_TYPE_SPLIT)
	{
		(*fi.StrLines).push_back(string(str));
		return;
	}
	else if(fmt_type==FORMAT_TYPE_LCOUNT)
	{
		return;
	}

	// Up text
	if(FLAG(flags,FT_UPPER) && fi.LinesAll>fi.LinesInRect)
	{
		int j=0;
		int line_cur=0;
		DWORD last_col=0;
		for(;str[j];++j)
		{
			if(str[j]=='\n')
			{
				line_cur++;
				if(line_cur>=(fi.LinesAll-fi.LinesInRect)) break;
			}

			if(fi.ColorDots[j]) last_col=fi.ColorDots[j];
		}

		if(FLAG(flags,FT_COLORIZE))
		{
			offs_col+=j+1;
			if(last_col && !fi.ColorDots[j+1]) fi.ColorDots[j+1]=last_col;
		}

		str=&str[j+1];
		fi.PStr=str;

		fi.LinesAll=fi.LinesInRect;
	}

	// Align
	curx=r.L;
	cury=r.T;

	for(int i=0;i<fi.LinesAll;i++) fi.LineWidth[i]=curx;

	bool can_count=false;
	int spaces=0;
	int curstr=0;
	for(int i=0;;i++)
	{
		switch(str[i]) 
		{
		case ' ':
			curx+=font->SpaceWidth;
			if(can_count) spaces++;
			break;
		case '\t':
			curx+=font->SpaceWidth*4;
			break;
		case '\0':
		case '\n':
			fi.LineWidth[curstr]=curx;
			cury+=font->MaxLettHeight+font->EmptyVer;
			curx=r.L;

			// Erase last spaces
			/*for(int j=i-1;spaces>0 && j>=0;j--)
			{
			if(str[j]==' ')
			{
			spaces--;
			str[j]='\r';
			}
			else if(str[j]!='\r') break;
			}*/

			// Align
			if(fi.LineSpaceWidth[curstr]==1 && spaces>0)
			{
				fi.LineSpaceWidth[curstr]=font->SpaceWidth+(r.R-fi.LineWidth[curstr])/spaces;
				//WriteLog("%d) %d + ( %d - %d ) / %d = %u\n",curstr,font->SpaceWidth,r.R,fi.LineWidth[curstr],spaces,fi.LineSpaceWidth[curstr]);
			}
			else fi.LineSpaceWidth[curstr]=font->SpaceWidth;

			curstr++;
			can_count=false;
			spaces=0;
			break;
		case '\r':
			break;
		default:
			curx+=font->Letters[(BYTE)str[i]].XAdvance;
			//if(curx>fi.LineWidth[curstr]) fi.LineWidth[curstr]=curx;
			can_count=true;
			break;
		}

		if(!str[i]) break;
	}

	curx=r.L;
	cury=r.T;

	// Align X
	if(FLAG(flags,FT_CENTERX)) curx+=(r.R-fi.LineWidth[0])/2;
	else if(FLAG(flags,FT_CENTERR)) curx+=r.R-fi.LineWidth[0];
	// Align Y
	if(FLAG(flags,FT_CENTERY)) cury=r.T+(r.H()-fi.LinesAll*font->MaxLettHeight-(fi.LinesAll-1)*font->EmptyVer)/2+1;
	else if(FLAG(flags,FT_BOTTOM)) cury=r.B-(fi.LinesAll*font->MaxLettHeight+(fi.LinesAll-1)*font->EmptyVer);
}

bool SpriteManager::DrawStr(INTRECT& r, const char* str, DWORD flags, DWORD col /* = 0 */, int num_font /* = -1 */)
{
	// Check
	if(!str || !str[0]) return false;

	// Get font
	Font* font=GetFont(num_font);
	if(!font) return false;

	// Format
	if(!col && DefFontColor) col=DefFontColor;

	static FontFormatInfo fi;
	fi.Init(font,flags,r,str);
	fi.DefColor=col;
	FormatText(fi,FORMAT_TYPE_DRAW);
	if(fi.IsError) return false;

	char* str_=fi.PStr;
	DWORD offs_col=fi.OffsColDots;
	int curx=fi.CurX;
	int cury=fi.CurY;
	int curstr=0;

	if(curSprCnt) Flush();

	if(FLAG(flags,FT_COLORIZE))
	{
		for(int i=offs_col;i;i--)
		{
			if(fi.ColorDots[i])
			{
				col=fi.ColorDots[i];
				break;
			}
		}
	}

	bool variable_space=false;
	for(int i=0;str_[i];i++)
	{
		if(FLAG(flags,FT_COLORIZE) && fi.ColorDots[i+offs_col]) col=(col&0xFF000000)|(fi.ColorDots[i+offs_col]&0xFFFFFF);

		switch(str_[i]) 
		{
		case ' ':
			curx+=(variable_space?fi.LineSpaceWidth[curstr]:font->SpaceWidth);
			continue;
		case '\t':
			curx+=font->SpaceWidth*4;
			continue;
		case '\n':
			cury+=font->MaxLettHeight+font->EmptyVer;
			curx=r.L;
			curstr++;
			variable_space=false;
			if(FLAG(flags,FT_CENTERX)) curx+=(r.R-fi.LineWidth[curstr])/2;
			else if(FLAG(flags,FT_CENTERR)) curx+=r.R-fi.LineWidth[curstr];
			continue;
		case '\r':
			continue;
		default:
			Letter& l=font->Letters[(BYTE)str_[i]];
			int mulpos=curSprCnt*4;
			int x=curx-l.OffsW-1;
			int y=cury-l.OffsH-1;
			int w=l.W+2;
			int h=l.H+2;

			FLOAT x1=font->ArrXY[(BYTE)str_[i]][0];
			FLOAT y1=font->ArrXY[(BYTE)str_[i]][1];
			FLOAT x2=font->ArrXY[(BYTE)str_[i]][2];
			FLOAT y2=font->ArrXY[(BYTE)str_[i]][3];

			waitBuf[mulpos].x=x-0.5f;
			waitBuf[mulpos].y=y+h-0.5f;
			waitBuf[mulpos].tu=x1;
			waitBuf[mulpos].tv=y2;
			waitBuf[mulpos++].Diffuse=col;

			waitBuf[mulpos].x=x-0.5f;
			waitBuf[mulpos].y=y-0.5f;
			waitBuf[mulpos].tu=x1;
			waitBuf[mulpos].tv=y1;
			waitBuf[mulpos++].Diffuse=col;

			waitBuf[mulpos].x=x+w-0.5f;
			waitBuf[mulpos].y=y-0.5f;
			waitBuf[mulpos].tu=x2;
			waitBuf[mulpos].tv=y1;
			waitBuf[mulpos++].Diffuse=col;

			waitBuf[mulpos].x=x+w-0.5f;
			waitBuf[mulpos].y=y+h-0.5f;
			waitBuf[mulpos].tu=x2;
			waitBuf[mulpos].tv=y2;
			waitBuf[mulpos++].Diffuse=col;

			if(++curSprCnt==flushSprCnt)
			{
				dipQueue.push_back(DipData(FLAG(flags,FT_BORDERED)?font->FontTexBordered:font->FontTex,font->Effect));
				dipQueue.back().SprCount=curSprCnt;
				Flush();
			}

			curx+=l.XAdvance;
			variable_space=true;
		}
	}

	if(curSprCnt)
	{
		dipQueue.push_back(DipData(FLAG(flags,FT_BORDERED)?font->FontTexBordered:font->FontTex,font->Effect));
		dipQueue.back().SprCount=curSprCnt;
		Flush();
	}

	return true;
}

int SpriteManager::GetLinesCount(int width, int height, const char* str, int num_font)
{
	Font* font=GetFont(num_font);
	if(!font) return 0;

	if(!str) return height/(font->MaxLettHeight+font->EmptyVer);

	static FontFormatInfo fi;
	fi.Init(font,0,INTRECT(0,0,width?width:modeWidth,height?height:modeHeight),str);
	FormatText(fi,FORMAT_TYPE_LCOUNT);
	if(fi.IsError) return 0;
	return fi.LinesInRect;
}

int SpriteManager::GetLinesHeight(int width, int height, const char* str, int num_font)
{
	Font* font=GetFont(num_font);
	if(!font) return 0;
	int cnt=GetLinesCount(width,height,str,num_font);
	if(cnt<=0) return 0;
	return cnt*font->MaxLettHeight+(cnt-1)*font->EmptyVer;
}

int SpriteManager::SplitLines(INTRECT& r, const char* cstr, int num_font, StrVec& str_vec)
{
	// Check & Prepare
	str_vec.clear();
	if(!cstr || !cstr[0]) return 0;

	// Get font
	Font* font=GetFont(num_font);
	if(!font) return 0;
	static FontFormatInfo fi;
	fi.Init(font,0,r,cstr);
	fi.StrLines=&str_vec;
	FormatText(fi,FORMAT_TYPE_SPLIT);
	if(fi.IsError) return 0;
	return str_vec.size();
}

/************************************************************************/
/*                                                                      */
/************************************************************************/