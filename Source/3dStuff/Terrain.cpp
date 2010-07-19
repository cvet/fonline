#include "StdAfx.h"
#include "Terrain.h"
#include "TerrainGrid.h"
#include "Log.h"
#include "FileManager.h"

Terrain::Terrain(LPDIRECT3DDEVICE9 device, int hx, int hy, const char* terrain_name)
{
	isError=false;
	d3dDevice=device;
	hexX=hx;
	hexY=hy;
	::FileOpen=&Terrain::FileOpen;
	::FileClose=&Terrain::FileClose;
	::FileRead=&Terrain::FileRead;
	::FileGetSize=&Terrain::FileGetSize;
	::FileGetTimeWrite=&Terrain::FileGetTimeWrite;
	::SaveData=&Terrain::SaveData;
	::Log=&Terrain::Log;
	terrainGrid=new TERRAIN_GRID();
	terrainGrid->Init(device);
	terrainGrid->LoadTerrain((char*)terrain_name);

	d3dDevice->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
	d3dDevice->SetSamplerState(1,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
}

bool Terrain::Draw(LPDIRECT3DSURFACE surf_rt, LPDIRECT3DSURFACE surf_ds, float x, float y, float zoom)
{
	if(isError) return false;

	// Store states
	LPDIRECT3DSURFACE old_rt=NULL,old_ds=NULL;
	D3D_HR(d3dDevice->GetRenderTarget(0,&old_rt));
	D3D_HR(d3dDevice->GetDepthStencilSurface(&old_ds));
	D3D_HR(d3dDevice->SetDepthStencilSurface(surf_ds));
	D3D_HR(d3dDevice->SetRenderTarget(0,surf_rt));
	IDirect3DVertexBuffer9* stream_data;
	UINT stream_offset,stream_stride;
	D3D_HR(d3dDevice->GetStreamSource(0,&stream_data,&stream_offset,&stream_stride));
	DWORD fvf;
	D3D_HR(d3dDevice->GetFVF(&fvf));
	D3D_HR(d3dDevice->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR));
	D3D_HR(d3dDevice->SetSamplerState(1,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR));

	// Get proj/view matrices
	D3DVIEWPORT9 vp;
	D3DXMATRIX proj,view,ident;
	D3D_HR(d3dDevice->GetViewport(&vp));
	D3D_HR(d3dDevice->GetTransform(D3DTS_PROJECTION,&proj));
	D3D_HR(d3dDevice->GetTransform(D3DTS_VIEW,&view));
	D3DXMatrixIdentity(&ident);

	// Get world matrix
	D3DXVECTOR3 coords;
	D3DXVec3Unproject(&coords,&D3DXVECTOR3(x/zoom,y/zoom,0.0f),&vp,&proj,&view,&ident);
	D3DXMATRIX mat_trans,mat_scale;
	D3DXMatrixTranslation(&mat_trans,coords.x,coords.y,0.0f);
	float scale=1.0f/zoom;
	D3DXMatrixScaling(&mat_scale,scale,scale,scale);
	D3DXMATRIX matr=mat_scale*mat_trans;

	// Render
	terrainGrid->DrawTerrain(&view,&proj,&matr);

	// Restore states
	D3D_HR(d3dDevice->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));
	D3D_HR(d3dDevice->SetSamplerState(1,D3DSAMP_MIPFILTER,D3DTEXF_NONE));
	D3D_HR(d3dDevice->SetStreamSource(0,stream_data,stream_offset,stream_stride));
	D3D_HR(d3dDevice->SetVertexShader(NULL));
	D3D_HR(d3dDevice->SetPixelShader(NULL));
	D3D_HR(d3dDevice->SetFVF(fvf));
	D3D_HR(d3dDevice->SetRenderTarget(0,old_rt));
	D3D_HR(d3dDevice->SetDepthStencilSurface(old_ds));
	old_rt->Release();
	old_ds->Release();
	return true;
}

void Terrain::PreRestore()
{

}

void Terrain::PostRestore()
{

}

void* Terrain::FileOpen(const char* fname, int folder)
{
	if(!fname) return NULL;
	FileManager* fm=new(nothrow) FileManager();
	if(!fm) return NULL;

	int path_type=PT_ROOT;
	if(folder==FOLDER_TERRAIN) path_type=PT_ROOT; // PT_TERRAIN
	else if(folder==FOLDER_EFFECT) path_type=PT_EFFECTS;
	else if(folder==FOLDER_TEXTURE) path_type=PT_TEXTURES;

	if(!fm->LoadFile(fname,path_type))
	{
		delete fm;
		return NULL;
	}
	return fm;
}

void Terrain::FileClose(void*& file)
{
	if(file)
	{
		FileManager* fm=(FileManager*)file;
		delete fm;
		file=NULL;
	}
}

size_t Terrain::FileRead(void* file, void* buffer, size_t size)
{
	FileManager* fm=(FileManager*)file;
	return fm->CopyMem(buffer,size)?1:0;
}

size_t Terrain::FileGetSize(void* file)
{
	FileManager* fm=(FileManager*)file;
	return fm->GetFsize();
}

__int64 Terrain::FileGetTimeWrite(void* file)
{
	FileManager* fm=(FileManager*)file;
	FILETIME write_time;
	fm->GetTime(NULL,NULL,&write_time);
	LARGE_INTEGER li;
	li.HighPart=write_time.dwHighDateTime;
	li.LowPart=write_time.dwLowDateTime;
	return li.QuadPart;
}

bool Terrain::SaveData(const char* fname, int folder, void* buffer, size_t size)
{
	int path_type=PT_ROOT;
	if(folder==FOLDER_TERRAIN) path_type=PT_TERRAIN;
	else if(folder==FOLDER_EFFECT) path_type=PT_EFFECTS;
	else if(folder==FOLDER_TEXTURE) path_type=PT_TEXTURES;

	FileManager fm;
	fm.SetData(buffer,size);
	return fm.SaveOutBufToFile(fname,path_type);
}

void Terrain::Log(const char* msg)
{
	WriteLog(__FUNCTION__" - %s",msg);
}







