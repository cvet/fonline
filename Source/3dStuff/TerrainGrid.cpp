#if !defined(FONLINE_CLIENT) && !defined(FONLINE_MAPPER)
#define EDITOR_VERSION
#endif

#ifdef EDITOR_VERSION
	#include "DXUT.h"
	#include "DXUTcamera.h"
	#include "DXUTsettingsdlg.h"
	#include "SDKmisc.h"
	#include "../resource.h"
	#include "../render.h"
#else
#include "Common.h"
#pragma warning (disable : 4101)
#endif

#include "TerrainGrid.h"
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>

TERRAIN_GRID	Terrain_Grid;

void* TerrainFileOpenDefault(const char* fname, int folder)
{
	if(!fname) return NULL;

	char path[MAX_PATH]={0};
	if(folder==FOLDER_TERRAIN) strcpy_s(path,"terrain\\");
	else if(folder==FOLDER_EFFECT) strcpy_s(path,"shaders\\");
	else if(folder==FOLDER_TEXTURE) strcpy_s(path,"textures\\");
	strcat_s(path,fname);

	FILE* file=NULL;
	if(fopen_s(&file,fname,"rb")) return NULL;
	return file;
}

void TerrainFileCloseDefault(void*& file)
{
	fclose((FILE*)file);
	file=NULL;
}

size_t TerrainFileReadDefault(void* file, void* buffer, size_t size)
{
	return fread(buffer,size,1,(FILE*)file);
}

size_t TerrainFileGetSizeDefault(void* file)
{
	FILE* f=(FILE*)file;
	long pos=ftell(f);
	fseek(f,0,SEEK_END);
	long len=ftell(f);
	fseek(f,0,SEEK_SET);
	return len;
}

__int64 TerrainFileGetTimeWriteDefault(void* file)
{
	struct _stat st;
	_fstat(_fileno((FILE*)file),&st);
	return st.st_mtime;
}

bool SaveDataDefault(const char* fname, int folder, void* buffer, size_t size)
{
	if(!fname) return NULL;

	char path[MAX_PATH]={0};
	if(folder==FOLDER_TERRAIN) strcpy_s(path,"terrain\\");
	else if(folder==FOLDER_EFFECT) strcpy_s(path,"shaders\\");
	else if(folder==FOLDER_TEXTURE) strcpy_s(path,"textures\\");
	strcat_s(path,fname);

	FILE* fsave=fopen(fname,"wb");
	if(fsave)
	{
		fwrite(buffer,size,1,fsave);
		fclose(fsave);
		return true;
	}
	return false;
}

void TerrainLogDefault(const char* msg)
{
}

TerrainFileOpen FileOpen=&TerrainFileOpenDefault;
TerrainFileClose FileClose=&TerrainFileCloseDefault;
TerrainFileRead FileRead=&TerrainFileReadDefault;
TerrainFileGetSize FileGetSize=&TerrainFileGetSizeDefault;
TerrainFileGetTimeWrite FileGetTimeWrite=&TerrainFileGetTimeWriteDefault;
TerrainSaveData SaveData=&SaveDataDefault;
TerrainLog Log=&TerrainLogDefault;

void	TERRAIN_GRID::Init(LPDIRECT3DDEVICE9 d3ddev)
{
	d3dDevice=d3ddev;

	D3DVERTEXELEMENT9  decl1[] =
	{
		{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		{ 0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
		D3DDECL_END()
	};
	d3dDevice->CreateVertexDeclaration(decl1, &WorldVDecl[0]);

	ZeroMemory(D3D_FX,sizeof(D3D_FX));
	ResD3DXCreateCompiledEffectFromFile(d3dDevice, "terrain.psh", 0, NULL, 0, 
		NULL, &D3D_FX[0].FX[0], NULL,0,0);

	for(int i=0;i<128;i++)
	{
		if(!D3D_FX[0].FX[i])
			continue;
		D3D_FX[0].Handles[i][0]= D3D_FX[0].FX[i]->GetParameterByName(0,"WorldViewProj");

		D3D_FX[0].Handles[i][80]= D3D_FX[0].FX[i]->GetTechniqueByName("Terrain1Tex");
		D3D_FX[0].Handles[i][81]= D3D_FX[0].FX[i]->GetTechniqueByName("Terrain2Tex");
		D3D_FX[0].Handles[i][82]= D3D_FX[0].FX[i]->GetTechniqueByName("Terrain3Tex");
		D3D_FX[0].Handles[i][83]= D3D_FX[0].FX[i]->GetTechniqueByName("Terrain4Tex");
		D3D_FX[0].Handles[i][84]= D3D_FX[0].FX[i]->GetTechniqueByName("Terrain5Tex");
	}
	d3dDevice->SetSamplerState( 5,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 5,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 5,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 4,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 4,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 4,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 3,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 3,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 3,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 2,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 2,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 2,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 1,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 1,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 1,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
	d3dDevice->SetSamplerState( 0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);

}
//*************************************************
void	TERRAIN_GRID::ResD3DXCreateCompiledEffectFromFile(LPDIRECT3DDEVICE9 pDevice,
											char* pSrcFile,
											const D3DXMACRO *pDefines,
											LPD3DXINCLUDE pInclude,
											DWORD Flags,
											LPD3DXEFFECTPOOL pPool,
											LPD3DXEFFECT *ppEffect,
											LPD3DXBUFFER *ppCompilationErrors,
											DWORD num,DWORD code
											)
{
	void*	Adr=0;
	DWORD	len;
	char	NewName[512];
	char	NewName1[512];
	char*	name1;
	char*	name2;
	LPD3DXEFFECTCOMPILER  ef_comp;
	LPD3DXBUFFER ef_buf;
	void* data,*data1,*data2;

	len=strlen((char*)pSrcFile);
	if(!len)
		return;
	memcpy(NewName,pSrcFile,len);
	sprintf(&NewName[len-4],"_%8.8x_%d_.fxc",code,num);

	data=FileOpen((char*)pSrcFile,FOLDER_EFFECT);
	data1=FileOpen((char*)NewName,FOLDER_EFFECT);

	if(data && (!data1 || (FileGetTimeWrite(data)>FileGetTimeWrite(data1)/* || FileGetTimeWrite(data)<=FileGetTimeWrite(data2)*/)))
	{
		size_t size=FileGetSize(data);
		char* buf=new(std::nothrow) char[size];
		if(!buf) return;
		FileRead(data,buf,size);

		LPD3DXBUFFER errors=0;
		LPD3DXBUFFER errors1=0;
		HRESULT hr=D3DXCreateEffectCompiler(buf,size,pDefines,NULL,D3DXSHADER_USE_LEGACY_D3DX9_31_DLL,&ef_comp,&errors);
		delete[] buf;

		FileClose(data);
		if(data1) FileClose(data1);

		void *adr1,*adr;
		if(errors)
			adr1=errors->GetBufferPointer();
		ef_comp->CompileEffect(0,&ef_buf,&errors1);
		if(errors1)
			adr=errors1->GetBufferPointer();

		D3DXCreateEffect(d3dDevice,ef_buf->GetBufferPointer(),ef_buf->GetBufferSize(),0,0,D3DXFX_NOT_CLONEABLE,0,ppEffect,0);

		SaveData(NewName,FOLDER_EFFECT,ef_buf->GetBufferPointer(),ef_buf->GetBufferSize());
		ef_comp->Release();
		ef_buf->Release();
	}
	else if(data1)
	{
		size_t size=FileGetSize(data1);
		char* buf=new(std::nothrow) char[size];
		if(!buf) return;
		FileRead(data1,buf,size);
		FileClose(data1);

		D3DXCreateEffect(d3dDevice,buf,size,NULL,NULL,D3DXFX_NOT_CLONEABLE|D3DXFX_LARGEADDRESSAWARE,NULL,ppEffect,NULL);
		delete[] buf;
	}

	if(data) FileClose(data);
	if(data1) FileClose(data1);
}

//***************************************
void	TERRAIN_GRID::DrawTerrain(D3DXMATRIX *view,D3DXMATRIX *proj,D3DXMATRIX *world)

{
	D3DXMATRIX	World,View,Proj,WVP;

	if(!VertBuf)
		return;

//	d3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DXCOLOR( 0.0f, 0.25f, 0.25f, 0.55f ), 1.0f,0 );

	// Render the scene
	//d3dDevice->BeginScene();

	D3DXMATRIX mat_rot_x,mat_rot_z;
	D3DXMatrixRotationX(&mat_rot_x,64.3f*D3DX_PI/180.0f);
	D3DXMatrixRotationZ(&mat_rot_z,30.0f*D3DX_PI/180.0f);
	World=mat_rot_z*mat_rot_x**world;

	WVP=World**view**proj;
	ViewFrustum.Init(&WVP);

	DWORD ShaderNum=0;

	TERRAIN_VERTEX*	verts;
	DWORD	NumPrim=0;
	DWORD	NumVerts=0;

	VertBuf->Lock(0,0,(void**)&verts,0);

	for(DWORD i=0;i<NumY;i++)
	{
		for(DWORD k=0;k<NumX;k++)
		{
			D3DXVECTOR3	vmax,vmin;
			DWORD id=k+i*NumX;

			vmax=Terrain[id].m_pnts[0];
			vmin=Terrain[id].m_pnts[2];
			if(ViewFrustum.TestBox(&vmax,&vmin))
			{
				verts[NumVerts+0].xyz=Terrain[id].m_pnts[0];
				verts[NumVerts+0].uv[0]=Terrain[id].m_uv0[0];
				verts[NumVerts+0].uv[1]=Terrain[id].m_uv1[0];
				verts[NumVerts+1].xyz=Terrain[id].m_pnts[1];
				verts[NumVerts+1].uv[0]=Terrain[id].m_uv0[1];
				verts[NumVerts+1].uv[1]=Terrain[id].m_uv1[1];
				verts[NumVerts+2].xyz=Terrain[id].m_pnts[2];
				verts[NumVerts+2].uv[0]=Terrain[id].m_uv0[2];
				verts[NumVerts+2].uv[1]=Terrain[id].m_uv1[2];

				verts[NumVerts+3].xyz=Terrain[id].m_pnts[0];
				verts[NumVerts+3].uv[0]=Terrain[id].m_uv0[0];
				verts[NumVerts+3].uv[1]=Terrain[id].m_uv1[0];
				verts[NumVerts+4].xyz=Terrain[id].m_pnts[2];
				verts[NumVerts+4].uv[0]=Terrain[id].m_uv0[2];
				verts[NumVerts+4].uv[1]=Terrain[id].m_uv1[2];
				verts[NumVerts+5].xyz=Terrain[id].m_pnts[3];
				verts[NumVerts+5].uv[0]=Terrain[id].m_uv0[3];
				verts[NumVerts+5].uv[1]=Terrain[id].m_uv1[3];

				NumPrim+=2;
				NumVerts+=6;
			}
		}
	}

	VertBuf->Unlock();
	if(NumPrim)
	{
		d3dDevice->SetVertexDeclaration(WorldVDecl[0]);
		d3dDevice->SetStreamSource(0, VertBuf,0, 28);
		d3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE);

		D3D_FX[0].FX[ShaderNum]->SetTechnique(D3D_FX[0].Handles[ShaderNum][81]);
		D3D_FX[0].FX[ShaderNum]->SetValue(D3D_FX[0].Handles[ShaderNum][0], &WVP, sizeof(D3DXMATRIX));
		D3D_FX[0].FX[ShaderNum]->Begin(0,D3DXFX_DONOTSAVESTATE);
		D3D_FX[0].FX[ShaderNum]->BeginPass(0);

		if(Terrain[0].m_materials[0]!=0xffff)
			d3dDevice->SetTexture(0,TerrTextures[Terrain[0].m_materials[0]]->Texture);
		if(Terrain[0].m_materials[1]!=0xffff)
			d3dDevice->SetTexture(1,TerrTextures[Terrain[0].m_materials[1]]->Texture);
		if(Terrain[0].m_alphatex!=0xffff)
			d3dDevice->SetTexture(5,TerrTextures[Terrain[0].m_alphatex]->Texture);
		d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST,0,NumPrim);

		D3D_FX[0].FX[ShaderNum]->EndPass();
		D3D_FX[0].FX[ShaderNum]->End();
	}
	//d3dDevice->EndScene();
}
//************************************************************
void	TERRAIN_GRID::LoadTerrain(char *name)
{
	void *inp;

	inp=FileOpen(name,FOLDER_TERRAIN);
	if(inp==NULL)
		return;

	FileRead(inp,&NumX,sizeof(DWORD));
	FileRead(inp,&NumY,sizeof(DWORD));
	FileRead(inp,&ElementSize,sizeof(DWORD));
	DWORD size0;
	FileRead(inp,&size0,sizeof(DWORD));

	//GridSizeX=NumX;
	//GridSizeY=NumY;
	//GridOneElementSize=ElementSize;
	if(Terrain)
		delete [] Terrain;
	Terrain=new TERRAIN_ELEMENT[NumX*NumY];

	for(DWORD i=0;i<NumY;i++)
	{
		for(DWORD k=0;k<NumX;k++)
		{
			FileRead(inp,&Terrain[k+i*NumX],size0);
		}
	}

	for(DWORD i=0;i<TerrTextures.size();i++)
	{
		delete TerrTextures[i];
	}
	TerrTextures.clear();
	TERRTEX tt;
	TERRTEX* tt1;

	DWORD numtex;
	DWORD size1;
	FileRead(inp,&numtex,sizeof(DWORD));
	FileRead(inp,&size1,sizeof(DWORD));
	for(DWORD i=0;i<numtex;i++)
	{
		FileRead(inp,&tt,size1);
		tt.Texture=0;

		// Erase "textures/"
		if(!_strnicmp(tt.Name,"textures/",9)) // strlen("textures/") == 9
		{
			char tmp[sizeof(tt.Name)];
			strcpy_s(tmp,tt.Name);
			strcpy_s(tt.Name,&tmp[9]);
		}

		DWORD tex0=AddTerrTexture(tt.Name);
		if(tex0==0xffff)
		{
			tt1=new TERRTEX;
			TerrTextures.push_back(tt1);
		}
	}
	FileClose(inp);

	if(VertBuf)
		VertBuf->Release();
	d3dDevice->CreateVertexBuffer(NumX*NumY*6*sizeof(TERRAIN_VERTEX),0,0,D3DPOOL_MANAGED, &VertBuf,0 ); 

}
//************************************************************
DWORD	TERRAIN_GRID::AddTerrTexture(char* FileName1)
{
	DWORD	i,k,found,len1,len2,model,usedds;
	char	name[300]="textures";
	char	name2[300]="*.*";
	char	name3[300]="textures";
	char*	name1;
	void	*inp;
	long	data;
	//_finddata_t	Info;
	void*	test;
	char	name4[16];
	LPDIRECT3DTEXTURE9	texture;
	char FileName[256];

	TERRTEX*	ttex=0;

	memcpy(FileName,FileName1,strlen(FileName1));
	name1=strstr(FileName,".");
	if(name1)
		name1[4]=0;
	else
		return 0;

	found=0;
	len1=strlen(FileName);

	for(i=0;i<TerrTextures.size();i++)
	{
		if(TerrTextures[i]->Texture!=NULL)// && bump==0)
		{
			len2=strlen(TerrTextures[i]->Name);
			if(len1==len2)
			{
				if(_strnicmp(FileName,TerrTextures[i]->Name,len1)==0)
				{
					return i;
				}
			}
		}
		else
			break;
	}

	inp=FileOpen(FileName,FOLDER_TEXTURE);
	if(inp==NULL)
		return 0;
	FileClose(inp);

	i=TerrTextures.size();
	ttex=new TERRTEX;
	TerrTextures.push_back(ttex);

	usedds=0;
	memcpy(TerrTextures[i]->Name,FileName,strlen(FileName));
	memcpy(&name[0],FileName,128);
	name1=strstr( name,".");
	name1[1]='d';
	name1[2]='d';
	name1[3]='s';
	inp=FileOpen(name,FOLDER_TEXTURE);
	if(inp)
	{
		usedds=1;
	}
	else
	{
		name1=strstr(name,".");
		name1[1]='d';
		name1[2]='d';
		name1[3]='c';
		inp=FileOpen(name,FOLDER_TEXTURE);
		if(inp)
			usedds=1;
		else
			memcpy(&name[0],FileName,strlen(FileName));
	}

	if(!inp) inp=FileOpen(name,FOLDER_TEXTURE);
	if(!inp) return 0;

	size_t size=FileGetSize(inp);
	char* buf=new(std::nothrow) char[size];
	if(!buf) return 0;
	FileRead(inp,buf,size);
	FileClose(inp);

	D3DXCreateTextureFromFileInMemoryEx(d3dDevice,buf,size,
		D3DX_DEFAULT,D3DX_DEFAULT,0,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,
		D3DX_DEFAULT,D3DX_DEFAULT,0,NULL,NULL,&TerrTextures[i]->Texture);
	delete[] buf;

	return i;
}
//************************************************************
#define DW_AS_FLT(DW) (*(FLOAT*)&(DW))
#define FLT_AS_DW(F) (*(DWORD*)&(F))
#define FLT_SIGN(F) ((FLT_AS_DW(F) & 0x80000000L))
#define ALMOST_ZERO(F) ((FLT_AS_DW(F) & 0x7f800000L)==0)
#define IS_SPECIAL(F)  ((FLT_AS_DW(F) & 0x7f800000L)==0x7f800000L)
static inline BOOL PlaneIntersection1( D3DXVECTOR3* intersectPt, const D3DXPLANE* p0, const D3DXPLANE* p1, const D3DXPLANE* p2 )
{
	D3DXVECTOR3 n0( p0->a, p0->b, p0->c );
	D3DXVECTOR3 n1( p1->a, p1->b, p1->c );
	D3DXVECTOR3 n2( p2->a, p2->b, p2->c );

	D3DXVECTOR3 n1_n2, n2_n0, n0_n1;  

	D3DXVec3Cross( &n1_n2, &n1, &n2 );
	D3DXVec3Cross( &n2_n0, &n2, &n0 );
	D3DXVec3Cross( &n0_n1, &n0, &n1 );

	float cosTheta = D3DXVec3Dot( &n0, &n1_n2 );

	if ( ALMOST_ZERO(cosTheta) || IS_SPECIAL(cosTheta) )
		return FALSE;

	float secTheta = 1.f / cosTheta;

	n1_n2 = n1_n2 * p0->d;
	n2_n0 = n2_n0 * p1->d;
	n0_n1 = n0_n1 * p2->d;

	*intersectPt = -(n1_n2 + n2_n0 + n0_n1) * secTheta;
	return TRUE;
}

//  build a frustum from a camera (projection, or viewProjection) matrix
void VIEW_FRUSTUM::Init(const D3DXMATRIX* matrix)
{
	D3DXVECTOR3	orig;

	//  build a view frustum based on the current view & projection matrices...
	D3DXVECTOR4 column4( matrix->_14, matrix->_24, matrix->_34, matrix->_44 );
	D3DXVECTOR4 column1( matrix->_11, matrix->_21, matrix->_31, matrix->_41 );
	D3DXVECTOR4 column2( matrix->_12, matrix->_22, matrix->_32, matrix->_42 );
	D3DXVECTOR4 column3( matrix->_13, matrix->_23, matrix->_33, matrix->_43 );

	D3DXVECTOR4 planes[6];
	planes[0] = column4 - column1;  // left
	planes[1] = column4 + column1;  // right
	planes[2] = column4 - column2;  // bottom
	planes[3] = column4 + column2;  // top
	planes[4] = column4 - column3;  // near
	planes[5] = column4 + column3;  // far
	// ignore near & far plane

	int p;

	for (p=0; p<6; p++)  // normalize the planes
	{
		float dot = planes[p].x*planes[p].x + planes[p].y*planes[p].y + planes[p].z*planes[p].z;
		dot = 1.f / sqrtf(dot);
		planes[p] = planes[p] * dot;
	}

	for (p=0; p<6; p++)
		camPlanes[p] = D3DXPLANE( planes[p].x, planes[p].y, planes[p].z, planes[p].w );

	//  build a bit-field that will tell us the indices for the nearest and farthest vertices from each plane...
	for (int i=0; i<6; i++)
		nVertexLUT[i] = ((planes[i].x<0.f)?1:0) | ((planes[i].y<0.f)?2:0) | ((planes[i].z<0.f)?4:0);

	for (int i=0; i<8; i++)  // compute extrema
	{
		const D3DXPLANE& p0 = (i&1)?camPlanes[4] : camPlanes[5];
		const D3DXPLANE& p1 = (i&2)?camPlanes[3] : camPlanes[2];
		const D3DXPLANE& p2 = (i&4)?camPlanes[0] : camPlanes[1];

		PlaneIntersection1( &pntList[i], &p0, &p1, &p2 );
	}
}
//  build a frustum from a camera (projection, or viewProjection) matrix
void VIEW_FRUSTUM::Init1(const D3DXMATRIX* matrix)
{
	D3DXVECTOR3	orig;

	//  build a view frustum based on the current view & projection matrices...
	D3DXVECTOR4 column4( matrix->_14, matrix->_24, matrix->_34, matrix->_44 );
	D3DXVECTOR4 column1( matrix->_11, matrix->_21, matrix->_31, matrix->_41 );
	D3DXVECTOR4 column2( matrix->_12, matrix->_22, matrix->_32, matrix->_42 );
	D3DXVECTOR4 column3( matrix->_13, matrix->_23, matrix->_33, matrix->_43 );

	D3DXVECTOR4 planes[6];
	planes[0] = column4 - column1;  // left
	planes[1] = column4 + column1;  // right
	planes[2] = column4 - column2;  // bottom
	planes[3] = column4 + column2;  // top
	planes[4] = column4 - column3;  // near
	planes[5] = column4 + column3;  // far
	// ignore near & far plane

	int p;

	for (p=0; p<6; p++)  // normalize the planes
	{
		float dot = planes[p].x*planes[p].x + planes[p].y*planes[p].y + planes[p].z*planes[p].z;
		dot = 1.f / sqrtf(dot);
		planes[p] = planes[p] * dot;
	}

	for (p=0; p<6; p++)
		camPlanes[p] = D3DXPLANE( planes[p].x, planes[p].y, planes[p].z, planes[p].w );

	//  build a bit-field that will tell us the indices for the nearest and farthest vertices from each plane...
	for (int i=0; i<6; i++)
		nVertexLUT[i] = ((planes[i].x<0.f)?1:0) | ((planes[i].y<0.f)?2:0) | ((planes[i].z<0.f)?4:0);

	for (int i=0; i<8; i++)  // compute extrema
	{
		const D3DXPLANE& p0 = (i&1)?camPlanes[4] : camPlanes[5];
		const D3DXPLANE& p1 = (i&2)?camPlanes[3] : camPlanes[2];
		const D3DXPLANE& p2 = (i&4)?camPlanes[0] : camPlanes[1];

		PlaneIntersection1( &pntList[i], &p0, &p1, &p2 );
	}
}
//************************************************************
int	VIEW_FRUSTUM::TestD3DVECTOR3(const D3DXVECTOR3 *vectors,const DWORD num)
{
	DWORD	i,k,outside;;

	for(i=0;i<6;i++)
	{
		outside=0;
		for(k=0;k<num;k++)
		{
			if(D3DXPlaneDotCoord(&camPlanes[i], &vectors[k])<0)
			{
				outside++;
			}
		}
		if(outside==num)
			return 0;
	}
	return 1;
}

//************************************************************
//  test if a sphere is within the view frustum
bool VIEW_FRUSTUM::TestSphere(const D3DXVECTOR3* centerVec,const float radius) const
{
	bool inside = true;

	for (int i=0; (i<6) && inside; i++)
		inside &= ((D3DXPlaneDotCoord(&camPlanes[i], centerVec) + radius) >= 0.f);

	return inside;
}
//************************************************************
//  Tests if an AABB is inside/intersecting the view frustum
//0-not vis,1-full inside,2-intersect
int VIEW_FRUSTUM::TestBox( const D3DXVECTOR3* maxPt, const D3DXVECTOR3* minPt) const
{
	bool intersect = false;
	DWORD	i;


	for (i=0; i<6; i++)
	{
		int nV = nVertexLUT[i];
		// pVertex is diagonally opposed to nVertex
		D3DXVECTOR3 nVertex( (nV&1)?minPt->x:maxPt->x, (nV&2)?minPt->y:maxPt->y, (nV&4)?minPt->z:maxPt->z );
		D3DXVECTOR3 pVertex( (nV&1)?maxPt->x:minPt->x, (nV&2)?maxPt->y:minPt->y, (nV&4)?maxPt->z:minPt->z );

		if ( D3DXPlaneDotCoord(&camPlanes[i], &nVertex) < 0.f )
			return 0;
		if ( D3DXPlaneDotCoord(&camPlanes[i], &pVertex) < 0.f )
			intersect = true;
	}
	return (intersect)?2 : 1;
}
