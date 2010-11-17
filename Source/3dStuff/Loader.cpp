#include "StdAfx.h"
#include "Loader.h"
#include "Text.h"
#include "Timer.h"

/************************************************************************/
/* Models                                                               */
/************************************************************************/

MeshHierarchy Loader3d::memAllocator;

D3DXFRAME* Loader3d::Load(IDirect3DDevice9* device, const char* fname, int path_type, ID3DXAnimationController** anim)
{
	FileManager fm;
	if(!fm.LoadFile(fname,path_type))
	{
		WriteLog(__FUNCTION__" - 3d file not found, name<%s>.\n",fname);
		return NULL;
	}

	const char* ext=FileManager::GetExtension(fname);
	if(!ext)
	{
		WriteLog(__FUNCTION__" - Extension of 3d file not found, name<%s>.\n",fname);
		return NULL;
	}

	//double t=Timer::AccurateTick();

	D3DXFRAME* frame=NULL;
	if(!_stricmp(ext,"x")) frame=Load_X(fm,device,anim);
	else if(!_stricmp(ext,"3ds")) frame=Load_3ds(fm,device);
	else WriteLog(__FUNCTION__" - Unknown file format.\n");

	//WriteLog("Load 3d file<%s> time<%g>.\n",fname,Timer::AccurateTick()-t);

	if(!frame) WriteLog(__FUNCTION__" - Can't load 3d file, name<%s>.\n",fname);
	return frame;
}

void Loader3d::Free(D3DXFRAME* frame)
{
	if(frame) D3DXFrameDestroy(frame,&memAllocator);
}

/************************************************************************/
/* Direct X file format                                                 */
/************************************************************************/

D3DXFRAME* Loader3d::Load_X(FileManager& fm, IDirect3DDevice9* device, ID3DXAnimationController** anim)
{
	D3DXFRAME* frame_root=NULL;
	HRESULT hr=D3DXLoadMeshHierarchyFromXInMemory(fm.GetBuf(),fm.GetFsize(),D3DXMESH_MANAGED,device,&memAllocator,NULL,&frame_root,anim);
	if(hr!=D3D_OK)
	{
		WriteLog(__FUNCTION__" - Can't load X file hierarchy, error<%s>.\n",DXGetErrorString(hr));
		return NULL;
	}
	return frame_root;
}

/************************************************************************/
/* 3ds file format                                                      */
/************************************************************************/

D3DXFRAME* Loader3d::Load_3ds(FileManager& fm, IDirect3DDevice9* device)
{
	class Material3ds
	{
	public:
		char Name[256];
		char TextureName[256];
		float Ambient[3];
		float Diffuse[3];
		float Specular[3];
	};
	vector<Material3ds> materials;

	class Mesh3ds
	{
	public:
		char Name[256];
		bool IsTriangularMesh;
		float Matrix[4][3];
		FloatVec Vertices;
		WordVec Faces;
		FloatVec TexCoords;
		class Subset
		{
		public:
			char Name[256];
			WordVec Faces;
		};
		vector<Subset> Subsets;
		Mesh3ds(){Name[0]=0;IsTriangularMesh=false;}
	};
	vector<Mesh3ds> meshes;

	float* cur_color=NULL;
	while(!fm.IsEOF())
	{
		WORD chunk=fm.GetLEWord();
		DWORD len=fm.GetLEDWord();

		switch(chunk)
		{
		case 0x4D4D: // MAIN3DS
			break;
		case 0x3D3D: // 3D EDITOR
			break;
		case 0x4000: // OBJECT BLOCK
			{
				meshes.push_back(Mesh3ds());
				Mesh3ds& mesh=meshes.back();
				fm.GetStr(mesh.Name);
			}
			break;
		case 0x4100: // TRIANGULAR MESH
			{
				meshes.back().IsTriangularMesh=true;
			}
			break;
		case 0x4110: // VERTICES LIST (x,y,z)
			{
				Mesh3ds& mesh=meshes.back();
				mesh.Vertices.resize(fm.GetLEWord()*3);
				for(size_t i=0,j=mesh.Vertices.size()/3;i<j;i++)
				{
					mesh.Vertices[i*3+0]=fm.GetLEFloat();
					mesh.Vertices[i*3+1]=fm.GetLEFloat();
					mesh.Vertices[i*3+2]=fm.GetLEFloat();
				}
			}
			break;
		case 0x4120: // FACES DESCRIPTION (a,b,c,flag)
			{
				Mesh3ds& mesh=meshes.back();
				mesh.Faces.resize(fm.GetLEWord()*3);
				for(size_t i=0,j=mesh.Faces.size()/3;i<j;i++)
				{
					mesh.Faces[i*3+0]=fm.GetLEWord();
					mesh.Faces[i*3+1]=fm.GetLEWord();
					mesh.Faces[i*3+2]=fm.GetLEWord();
					fm.GetLEWord();
				}
			}
			break;
		case 0x4130: // MSH_MAT_GROUP
			{
				Mesh3ds& mesh=meshes.back();
				mesh.Subsets.push_back(Mesh3ds::Subset());
				Mesh3ds::Subset& ss=mesh.Subsets.back();
				fm.GetStr(ss.Name);
				ss.Faces.resize(fm.GetLEWord());
				fm.CopyMem(&ss.Faces[0],ss.Faces.size()*2);
			}
			break;
		case 0x4140: // MAPPING COORDINATES LIST (u,v)
			{
				Mesh3ds& mesh=meshes.back();
				mesh.TexCoords.resize(fm.GetLEWord()*2);
				for(size_t i=0,j=mesh.TexCoords.size()/2;i<j;i++)
				{
					mesh.TexCoords[i*2+0]=fm.GetLEFloat();
					mesh.TexCoords[i*2+1]=1.0f-fm.GetLEFloat();
				}
			}
			break;
		case 0x4160: // MESH_MATRIX
			{
				Mesh3ds& mesh=meshes.back();
				fm.CopyMem(mesh.Matrix,48);
			}
			break;
		case 0xAFFF: // MAT_ENTRY
			materials.push_back(Material3ds());
			ZeroMemory(&materials.back(),sizeof(Material3ds));
			break;
		case 0xA200: // MAT_TEXMAP
			break;
		case 0xA000: // MAT_NAME
			fm.GetStr(materials.back().Name);
			break;
		case 0xA300: // MAT_MAPNAME
			fm.GetStr(materials.back().TextureName);
			break;
		case 0xA010: // MAT_AMBIENT
			cur_color=materials.back().Ambient;
			break;
		case 0xA020: // MAT_DIFFUSE
			cur_color=materials.back().Diffuse;
			break;
		case 0xA030: // MAT_SPECULAR
			cur_color=materials.back().Specular;
			break;
		case 0x0011: // COLOR_24
		case 0x0012: // LIN_COLOR_24
			if(cur_color) for(int i=0;i<3;i++) cur_color[i]=(float)fm.GetByte()/255.0f;
			else fm.GoForward(3);
			cur_color=NULL;
			break;
		case 0x0010: // COLOR_F
		case 0x0013: // LIN_COLOR_F
			if(cur_color) for(int i=0;i<3;i++) cur_color[i]=fm.GetLEFloat();
			else fm.GoForward(12);
			cur_color=NULL;
			break;
		case 0x0100: // MASTER_SCALE
		case 0xA040: // MAT_SHININESS
		case 0xA050: // MAT_TRANSPARENCY
		default: // Unknown chunk, skip
			fm.GoForward(len-6);
			break;
		}
	}

	if(meshes.empty())
	{
		WriteLog(__FUNCTION__" - Not found any meshes in 3ds file.\n");
		return NULL;
	}

	// Fill materials
	static vector<D3DXMATERIAL> dxmaterials;
	for(size_t i=0,j=dxmaterials.size();i<j;i++) SAFEDELA(dxmaterials[i].pTextureFilename);
	dxmaterials.resize(materials.size());
	for(size_t i=0,j=materials.size();i<j;i++)
	{
		Material3ds& mtrl=materials[i];
		D3DXMATERIAL& dxmtrl=dxmaterials[i];
		ZeroMemory(&dxmtrl,sizeof(dxmtrl));
		dxmtrl.pTextureFilename=StringDuplicate(mtrl.TextureName);
		dxmtrl.MatD3D.Ambient.r=mtrl.Ambient[0];
		dxmtrl.MatD3D.Ambient.g=mtrl.Ambient[1];
		dxmtrl.MatD3D.Ambient.b=mtrl.Ambient[2];
		dxmtrl.MatD3D.Ambient.a=1.0f;
		dxmtrl.MatD3D.Diffuse.r=mtrl.Diffuse[0];
		dxmtrl.MatD3D.Diffuse.g=mtrl.Diffuse[1];
		dxmtrl.MatD3D.Diffuse.b=mtrl.Diffuse[2];
		dxmtrl.MatD3D.Diffuse.a=1.0f;
		dxmtrl.MatD3D.Specular.r=mtrl.Specular[0];
		dxmtrl.MatD3D.Specular.g=mtrl.Specular[1];
		dxmtrl.MatD3D.Specular.b=mtrl.Specular[2];
		dxmtrl.MatD3D.Specular.a=1.0f;
	}

	// Create frames
	D3DXFRAME* frame_root=NULL;
	for(size_t mi=0;mi<meshes.size();mi++)
	{
		Mesh3ds& mesh=meshes[mi];

		if(mesh.IsTriangularMesh)
		{
			// Create frame
			D3DXFRAME* frame;
			D3D_HR(memAllocator.CreateFrame(mesh.Name,&frame));
			if(!frame_root) frame_root=frame;

			// Model already positioned
			D3DXMatrixIdentity(&frame->TransformationMatrix);

			// Create mesh
			D3DXMESHDATA dxmesh;
			dxmesh.Type=D3DXMESHTYPE_MESH;
			D3D_HR(D3DXCreateMeshFVF(mesh.Faces.size()/3,mesh.Vertices.size()/3,D3DXMESH_MANAGED,D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1,device,&dxmesh.pMesh));

			// Fill data
			struct
			{
				float x,y,z;
				float nx,ny,nz;
				float u,v;
			} *vb;
			WORD* ib;
			DWORD* att;
			D3D_HR(dxmesh.pMesh->LockVertexBuffer(0,(void**)&vb));
			D3D_HR(dxmesh.pMesh->LockIndexBuffer(0,(void**)&ib));
			D3D_HR(dxmesh.pMesh->LockAttributeBuffer(0,&att));

			for(DWORD i=0;i<mesh.Vertices.size()/3;i++)
			{
				vb[i].x=mesh.Vertices[i*3+0];
				vb[i].y=mesh.Vertices[i*3+2];
				vb[i].z=mesh.Vertices[i*3+1];
				vb[i].u=(i*2+0<mesh.TexCoords.size()?mesh.TexCoords[i*2+0]:0.0f);
				vb[i].v=(i*2+1<mesh.TexCoords.size()?mesh.TexCoords[i*2+1]:0.0f);
			}

			for(DWORD i=0;i<mesh.Faces.size()/3;i++)
			{
				ib[i*3+0]=mesh.Faces[i*3+2];
				ib[i*3+1]=mesh.Faces[i*3+1];
				ib[i*3+2]=mesh.Faces[i*3+0];

				// Compute normals
				int v1=ib[i*3+0];
				int v2=ib[i*3+1];
				int v3=ib[i*3+2];
				D3DXVECTOR3 a(mesh.Vertices[v1*3+0],mesh.Vertices[v1*3+2],mesh.Vertices[v1*3+1]);
				D3DXVECTOR3 b(mesh.Vertices[v2*3+0],mesh.Vertices[v2*3+2],mesh.Vertices[v2*3+1]);
				D3DXVECTOR3 c(mesh.Vertices[v3*3+0],mesh.Vertices[v3*3+2],mesh.Vertices[v3*3+1]);
				D3DXVECTOR3 p=b-a;
				D3DXVECTOR3 q=c-a;
				D3DXVECTOR3 n;
				D3DXVec3Cross(&n,&p,&q);
				D3DXVec3Normalize(&n,&n);
				vb[v1].nx=n.x; vb[v1].ny=n.y; vb[v1].nz=n.z;
				vb[v2].nx=n.x; vb[v2].ny=n.y; vb[v2].nz=n.z;
				vb[v3].nx=n.x; vb[v3].ny=n.y; vb[v3].nz=n.z;
			}

			ZeroMemory(att,sizeof(DWORD)*mesh.Faces.size()/3);
			for(size_t i=0,j=mesh.Subsets.size();i<j;i++)
			{
				DWORD mat_ind=0;
				for(size_t k=0,l=materials.size();k<l;k++)
				{
					if(!strcmp(mesh.Subsets[i].Name,materials[k].Name))
					{
						mat_ind=k;
						break;
					}
				}

				for(size_t n=0,m=mesh.Subsets[i].Faces.size();n<m;n++)
					att[mesh.Subsets[i].Faces[n]]=mat_ind;
			}

			D3D_HR(dxmesh.pMesh->UnlockVertexBuffer());
			D3D_HR(dxmesh.pMesh->UnlockIndexBuffer());
			D3D_HR(dxmesh.pMesh->UnlockAttributeBuffer());

			// Normals smoothing
			DwordVec adjacency;
			adjacency.resize(mesh.Faces.size());
			dxmesh.pMesh->GenerateAdjacency(0.0000125f,&adjacency[0]);
			D3D_HR(D3DXComputeNormals(dxmesh.pMesh,&adjacency[0]));

			// Create container
			D3D_HR(memAllocator.CreateMeshContainer(mesh.Name,&dxmesh,&dxmaterials[0],NULL,dxmaterials.size(),&adjacency[0],NULL,&frame->pMeshContainer));
		}
	}

	return frame_root;
};

/************************************************************************/
/* Textures                                                             */
/************************************************************************/
TextureExVec Loader3d::loadedTextures;

TextureEx* Loader3d::LoadTexture(IDirect3DDevice9* device, const char* texture_name, const char* model_path, int model_path_type)
{
	if(!texture_name || !texture_name[0]) return NULL;

	// Try find already loaded texture
	for(TextureExVecIt it=loadedTextures.begin(),end=loadedTextures.end();it!=end;++it)
	{
		TextureEx* texture=*it;
		if(!_stricmp(texture->Name,texture_name)) return texture;
	}

	// First try load from textures folder
	FileManager fm;
	if(!fm.LoadFile(texture_name,PT_TEXTURES) && model_path)
	{
		// After try load from file folder
		char path[MAX_FOPATH];
		FileManager::ExtractPath(model_path,path);
		StringAppend(path,texture_name);
		fm.LoadFile(path,model_path_type);
	}

	// Create texture
	IDirect3DTexture9* texture=NULL;
	if(!fm.IsLoaded() || FAILED(D3DXCreateTextureFromFileInMemory(device,fm.GetBuf(),fm.GetFsize(),&texture))) return NULL;

	TextureEx* texture_ex=new TextureEx();
	texture_ex->Name=StringDuplicate(texture_name);
	texture_ex->Texture=texture;
	loadedTextures.push_back(texture_ex);
	return loadedTextures.back();
}

void Loader3d::FreeTexture(TextureEx* texture)
{
	if(texture)
	{
		for(TextureExVecIt it=loadedTextures.begin(),end=loadedTextures.end();it!=end;++it)
		{
			TextureEx* texture_=*it;
			if(texture_==texture)
			{
				loadedTextures.erase(it);
				break;
			}
		}

		SAFEDELA(texture->Name);
		SAFEREL(texture->Texture);
		SAFEDEL(texture);
	}
	else
	{
		TextureExVec textures=loadedTextures;
		for(TextureExVecIt it=textures.begin(),end=textures.end();it!=end;++it) FreeTexture(*it);
	}
}

/************************************************************************/
/* Effects                                                              */
/************************************************************************/
class IncludeParser : public ID3DXInclude
{
public:
	char* RootPath;
	int RootPathType;

	STDMETHOD(Open)(THIS_ D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
	{
		FileManager fm;
		if(!fm.LoadFile(pFileName,PT_EFFECTS))
		{
			if(!RootPath) return S_FALSE;
			char path[MAX_FOPATH];
			FileManager::ExtractPath(RootPath,path);
			StringAppend(path,pFileName);
			if(!fm.LoadFile(path,RootPathType)) return S_FALSE;
		}
		*pBytes=fm.GetFsize();
		*ppData=fm.ReleaseBuffer();
		return S_OK;
	}

	STDMETHOD(Close)(THIS_ LPCVOID pData)
	{
		if(pData) delete[] pData;
		return S_OK;
	}
};

EffectExVec Loader3d::loadedEffects;
IncludeParser includeParser;

EffectEx* Loader3d::LoadEffect(IDirect3DDevice9* device, const char* effect_name)
{
	D3DXEFFECTINSTANCE effect_inst;
	ZeroMemory(&effect_inst,sizeof(effect_inst));
	effect_inst.pEffectFilename=(char*)effect_name;
	return LoadEffect(device,&effect_inst,NULL,0);
}

EffectEx* Loader3d::LoadEffect(IDirect3DDevice9* device, D3DXEFFECTINSTANCE* effect_inst, const char* model_path, int model_path_type)
{
	if(!effect_inst || !effect_inst->pEffectFilename || !effect_inst->pEffectFilename[0]) return NULL;
	const char* effect_name=effect_inst->pEffectFilename;

	// Try find already loaded texture
	for(EffectExVecIt it=loadedEffects.begin(),end=loadedEffects.end();it!=end;++it)
	{
		EffectEx* effect=*it;
		if(!_stricmp(effect->Name,effect_name)) return effect;
	}

	// First try load from effects folder
	FileManager fm;
	if(!fm.LoadFile(effect_name,PT_EFFECTS) && model_path)
	{
		// After try load from file folder
		char path[MAX_FOPATH];
		FileManager::ExtractPath(model_path,path);
		StringAppend(path,effect_name);
		fm.LoadFile(path,model_path_type);
	}
	if(!fm.IsLoaded()) return NULL;

	// Find already compiled effect in cache
	char cache_name[MAX_FOPATH];
	sprintf(cache_name,"%s.fxc",effect_name);
	FileManager fm_cache;
	if(fm_cache.LoadFile(cache_name,PT_CACHE))
	{
		FILETIME last_write,last_write_cache;
		fm.GetTime(NULL,NULL,&last_write);
		fm_cache.GetTime(NULL,NULL,&last_write_cache);
		if(CompareFileTime(&last_write,&last_write_cache)>0) fm_cache.UnloadFile();
	}

	// Load and cache effect
	if(!fm_cache.IsLoaded())
	{
		char* buf=(char*)fm.GetBuf();
		DWORD size=fm.GetFsize();

		LPD3DXEFFECTCOMPILER ef_comp=NULL;
		LPD3DXBUFFER ef_buf=NULL;
		LPD3DXBUFFER errors=NULL;
		LPD3DXBUFFER errors31=NULL;
		HRESULT hr=0,hr31=0;

		includeParser.RootPath=(char*)model_path;
		includeParser.RootPathType=model_path_type;
		hr=D3DXCreateEffectCompiler(buf,size,NULL,&includeParser,0,&ef_comp,&errors);
		if(FAILED(hr)) hr31=D3DXCreateEffectCompiler(buf,size,NULL,&includeParser,D3DXSHADER_USE_LEGACY_D3DX9_31_DLL,&ef_comp,&errors31);

		if(SUCCEEDED(hr) || SUCCEEDED(hr31))
		{
			SAFEREL(errors);
			if(SUCCEEDED(ef_comp->CompileEffect(0,&ef_buf,&errors)))
			{
				fm_cache.SetData(ef_buf->GetBufferPointer(),ef_buf->GetBufferSize());
				fm_cache.SaveOutBufToFile(cache_name,PT_CACHE);
				fm_cache.SwitchToRead();
			}
			else
			{
				WriteLog(__FUNCTION__" - Unable to compile effect, effect<%s>, errors<\n%s>.\n",effect_name,errors?errors->GetBufferPointer():"nullptr\n");
			}
		}
		else
		{
			WriteLog(__FUNCTION__" - Unable to create effect compiler, effect<%s>, errors<%s\n%s>, legacy compiler errors<%s\n%s>.\n",effect_name,
				DXGetErrorString(hr),errors?errors->GetBufferPointer():"",DXGetErrorString(hr31),errors31?errors31->GetBufferPointer():"");
		}

		SAFEREL(ef_comp);
		SAFEREL(ef_buf);
		SAFEREL(errors);
		SAFEREL(errors31);

		if(!fm_cache.IsLoaded()) return NULL;
	}

	// Create effect
	ID3DXEffect* effect=NULL;
	LPD3DXBUFFER errors=NULL;
	if(FAILED(D3DXCreateEffect(device,fm_cache.GetBuf(),fm_cache.GetFsize(),NULL,NULL,/*D3DXSHADER_SKIPVALIDATION|*/D3DXFX_NOT_CLONEABLE,NULL,&effect,&errors)))
	{
		WriteLog(__FUNCTION__" - Unable to create effect, effect<%s>, errors<\n%s>.\n",effect_name,errors?errors->GetBufferPointer():"nullptr");
		SAFEREL(effect);
		SAFEREL(errors);
		return NULL;
	}
	SAFEREL(errors);

	EffectEx* effect_ex=new EffectEx();
	effect_ex->Name=StringDuplicate(effect_name);
	effect_ex->Effect=effect;
	effect_ex->EffectFlags=D3DXFX_DONOTSAVESTATE;
	effect_ex->EffectParams=NULL;

	effect_ex->TechniqueSkinned=effect->GetTechniqueByName("Skinned");
	effect_ex->TechniqueSkinnedWithShadow=effect->GetTechniqueByName("SkinnedWithShadow");
	effect_ex->TechniqueSimple=effect->GetTechniqueByName("Simple");
	effect_ex->TechniqueSimpleWithShadow=effect->GetTechniqueByName("SimpleWithShadow");
	effect_ex->BonesInfluences=effect->GetParameterByName(NULL,"BonesInfluences");
	effect_ex->GroundPosition=effect->GetParameterByName(NULL,"GroundPosition");
	effect_ex->LightDir=effect->GetParameterByName(NULL,"LightDir");
	effect_ex->LightDiffuse=effect->GetParameterByName(NULL,"LightDiffuse");
	effect_ex->MaterialAmbient=effect->GetParameterByName(NULL,"MaterialAmbient");
	effect_ex->MaterialDiffuse=effect->GetParameterByName(NULL,"MaterialDiffuse");
	effect_ex->WorldMatrices=effect->GetParameterByName(NULL,"WorldMatrices");
	effect_ex->ProjectionMatrix=effect->GetParameterByName(NULL,"ProjectionMatrix");

	if(!effect_ex->TechniqueSimple)
	{
		WriteLog(__FUNCTION__" - Technique 'Simple' not founded, effect<%s>.\n",effect_name);
		delete effect_ex;
		SAFEREL(effect);
		return NULL;
	}

	if(!effect_ex->TechniqueSimpleWithShadow) effect_ex->TechniqueSimpleWithShadow=effect_ex->TechniqueSimple;
	if(!effect_ex->TechniqueSkinned) effect_ex->TechniqueSkinned=effect_ex->TechniqueSimple;
	if(!effect_ex->TechniqueSkinnedWithShadow) effect_ex->TechniqueSkinnedWithShadow=effect_ex->TechniqueSkinned;

	effect_ex->PassIndex=effect->GetParameterByName(NULL,"PassIndex");
	effect_ex->Time=effect->GetParameterByName(NULL,"Time");
	effect_ex->TimeCurrent=0.0f;
	effect_ex->TimeLastTick=Timer::AccurateTick();
	effect_ex->TimeGame=effect->GetParameterByName(NULL,"TimeGame");
	effect_ex->TimeGameCurrent=0.0f;
	effect_ex->TimeGameLastTick=Timer::AccurateTick();
	effect_ex->IsTime=(effect_ex->Time || effect_ex->TimeGame);
	effect_ex->Random1Pass=effect->GetParameterByName(NULL,"Random1Pass");
	effect_ex->Random2Pass=effect->GetParameterByName(NULL,"Random2Pass");
	effect_ex->Random3Pass=effect->GetParameterByName(NULL,"Random3Pass");
	effect_ex->Random4Pass=effect->GetParameterByName(NULL,"Random4Pass");
	effect_ex->IsRandomPass=(effect_ex->Random1Pass || effect_ex->Random2Pass || effect_ex->Random3Pass || effect_ex->Random4Pass);
	effect_ex->Random1Effect=effect->GetParameterByName(NULL,"Random1Effect");
	effect_ex->Random2Effect=effect->GetParameterByName(NULL,"Random2Effect");
	effect_ex->Random3Effect=effect->GetParameterByName(NULL,"Random3Effect");
	effect_ex->Random4Effect=effect->GetParameterByName(NULL,"Random4Effect");
	effect_ex->IsRandomEffect=(effect_ex->Random1Effect || effect_ex->Random2Effect || effect_ex->Random3Effect || effect_ex->Random4Effect);
	effect_ex->IsNeedProcess=(effect_ex->PassIndex || effect_ex->IsTime || effect_ex->IsRandomPass || effect_ex->IsRandomEffect);

	if(effect_inst->NumDefaults)
	{
		effect->BeginParameterBlock();
		for(DWORD d=0;d<effect_inst->NumDefaults;d++)
		{
			D3DXEFFECTDEFAULT& def=effect_inst->pDefaults[d];
			D3DXHANDLE param=NULL;
			effect->GetParameterByName(param,def.pParamName);
			switch(def.Type)
			{
			case D3DXEDT_STRING: // pValue points to a null terminated ASCII string 
				effect->SetString(param,(LPCSTR)def.pValue); break;
			case D3DXEDT_FLOATS: // pValue points to an array of floats - number of floats is NumBytes / sizeof(float)
				effect->SetFloatArray(param,(FLOAT*)def.pValue,def.NumBytes/sizeof(FLOAT)); break;
			case D3DXEDT_DWORD:  // pValue points to a DWORD
				effect->SetInt(param,*(DWORD*)def.pValue); break;
			default: break;
			}
		}
		effect_ex->EffectParams=effect->EndParameterBlock();
	}

	loadedEffects.push_back(effect_ex);
	return loadedEffects.back();
}

void Loader3d::EffectProcessVariables(EffectEx* effect, int pass)
{
	// Process effect
	if(pass==-1)
	{
		if(effect->IsTime)
		{
			double tick=Timer::AccurateTick();
			if(effect->Time)
			{
				effect->TimeCurrent+=tick-effect->TimeLastTick;
				effect->TimeLastTick=tick;
				while(effect->TimeCurrent>=120.0f) effect->TimeCurrent-=120.0f;

				effect->Effect->SetFloat(effect->Time,effect->TimeCurrent);
			}
			if(effect->TimeGame)
			{
				if(!Timer::IsGamePaused())
				{
					effect->TimeGameCurrent+=tick-effect->TimeGameLastTick;
					effect->TimeGameLastTick=tick;
					while(effect->TimeGameCurrent>=120.0f) effect->TimeGameCurrent-=120.0f;
				}
				else
				{
					effect->TimeGameLastTick=tick;
				}

				effect->Effect->SetFloat(effect->TimeGame,effect->TimeGameCurrent);
			}
		}

		if(effect->IsRandomEffect)
		{
			if(effect->Random1Effect) effect->Effect->SetFloat(effect->Random1Effect,(double)Random(0,2000000000)/2000000000.0);
			if(effect->Random2Effect) effect->Effect->SetFloat(effect->Random2Effect,(double)Random(0,2000000000)/2000000000.0);
			if(effect->Random3Effect) effect->Effect->SetFloat(effect->Random3Effect,(double)Random(0,2000000000)/2000000000.0);
			if(effect->Random4Effect) effect->Effect->SetFloat(effect->Random4Effect,(double)Random(0,2000000000)/2000000000.0);
		}
	}
	// Process pass
	else
	{
		if(effect->PassIndex) effect->Effect->SetFloat(effect->Random1Pass,(float)pass);

		if(effect->IsRandomPass)
		{
			if(effect->Random1Pass) effect->Effect->SetFloat(effect->Random1Pass,(double)Random(0,2000000000)/2000000000.0);
			if(effect->Random2Pass) effect->Effect->SetFloat(effect->Random2Pass,(double)Random(0,2000000000)/2000000000.0);
			if(effect->Random3Pass) effect->Effect->SetFloat(effect->Random3Pass,(double)Random(0,2000000000)/2000000000.0);
			if(effect->Random4Pass) effect->Effect->SetFloat(effect->Random4Pass,(double)Random(0,2000000000)/2000000000.0);
		}
	}
}

/*
Todo:
	if(Name) delete Name;
	if(Effect)
	{
		if(EffectParams) Effect->DeleteParameterBlock(EffectParams);
		Effect->Release();
		Effect=NULL;
	}
*/

/************************************************************************/
/*                                                                      */
/************************************************************************/
