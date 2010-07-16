//	stl
#include <set>
#include <cmath>
#include <vector>
#include <map>
#include <hash_map>
#include <list>
#include <memory>
#include <algorithm>
#include <iostream>
#include <fstream>

#define FOLDER_TERRAIN           (0)
#define FOLDER_EFFECT            (1)
#define FOLDER_TEXTURE           (2)

typedef void* (*TerrainFileOpen)(const char* fname, int folder);
typedef void (*TerrainFileClose)(void*& file);
typedef size_t (*TerrainFileRead)(void* file, void* buffer, size_t size);
typedef size_t (*TerrainFileGetSize)(void* file);
typedef __int64 (*TerrainFileGetTimeWrite)(void* file);
typedef bool (*TerrainSaveData)(const char* fname, int folder, void* buffer, size_t size);
typedef void (*TerrainLog)(const char* msg);
extern TerrainFileOpen FileOpen;
extern TerrainFileClose FileClose;
extern TerrainFileRead FileRead;
extern TerrainFileGetSize FileGetSize;
extern TerrainFileGetTimeWrite FileGetTimeWrite;
extern TerrainSaveData SaveData;
extern TerrainLog Log;

struct VIEW_FRUSTUM
{
	void Init( const D3DXMATRIX* matrix );
	void Init1( const D3DXMATRIX* matrix );

	bool TestSphere(const D3DXVECTOR3* centerVec,const float radius) const;
	int TestBox( const D3DXVECTOR3* maxPt, const D3DXVECTOR3* minPt) const;
	int	TestD3DVECTOR3(const D3DXVECTOR3 *vectors,const DWORD num);

	D3DXPLANE camPlanes[6];
	int nVertexLUT[6];
	D3DXVECTOR3 pntList[8];
};


class TERRAIN_GRID
{
	struct D3D_EFFECTS
	{
		LPD3DXEFFECT	FX[128];
		D3DXHANDLE Handles[128][256];
	};
	struct	TERRAIN_ELEMENT
	{
		D3DXVECTOR3	m_pnts[4];
		D3DXVECTOR2 m_uv0[4];
		D3DXVECTOR2 m_uv1[4];
		WORD		m_materials[5];
		WORD		m_alphatex;
	};
	struct TERRAIN_VERTEX
	{
		D3DXVECTOR3	xyz;
		D3DXVECTOR2	uv[2];
	};
	struct	TERRTEX
	{
		LPDIRECT3DTEXTURE9        Texture;
		char	Name[128];
		//**************************************
		TERRTEX()
		{
			Texture=0;
			ZeroMemory(Name,128);
		};
		~TERRTEX()
		{
			if(Texture)
				Texture->Release();
		};

	};
//****************************************
public:

	void	Init(LPDIRECT3DDEVICE9 d3ddevice);
	void	Release(void);
	void	LoadTerrain(char *name);
	void	DrawTerrain(D3DXMATRIX *view,D3DXMATRIX *proj,D3DXMATRIX *world);

	DWORD	AddTerrTexture(char* FileName1);
	void	ResD3DXCreateCompiledEffectFromFile(LPDIRECT3DDEVICE9 pDevice,
		char* pSrcFile,
		const D3DXMACRO *pDefines,
		LPD3DXINCLUDE pInclude,
		DWORD Flags,
		LPD3DXEFFECTPOOL pPool,
		LPD3DXEFFECT *ppEffect,
		LPD3DXBUFFER *ppCompilationErrors,
		DWORD num,DWORD code
		);
//*********************************

	DWORD	NumX;
	DWORD	NumY;
	DWORD	ElementSize;
	LPDIRECT3DVERTEXBUFFER9		VertBuf;
	TERRAIN_ELEMENT	*Terrain;
	LPDIRECT3DDEVICE9			d3dDevice;
	LPDIRECT3DVERTEXDECLARATION9	WorldVDecl[64];
	D3D_EFFECTS D3D_FX[64];
	std::vector<TERRTEX*> TerrTextures;
	VIEW_FRUSTUM	ViewFrustum;

};



extern	TERRAIN_GRID	Terrain_Grid;
