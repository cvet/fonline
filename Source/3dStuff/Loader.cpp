#include "StdAfx.h"
#include "Loader.h"
#include "Text.h"
#include "Timer.h"

#include "Assimp/aiScene.h"
#include "Assimp/aiAnim.h"
#include "Assimp/aiPostProcess.h"
#include "Assimp/assimp.hpp"
#pragma comment (lib, "assimp.lib")

/************************************************************************/
/* Models                                                               */
/************************************************************************/

MeshHierarchy Loader3d::memAllocator;
PCharVec Loader3d::processedFiles;
FrameVec Loader3d::loadedModels;
PCharVec Loader3d::loadedAnimationsFNames;
AnimSetVec Loader3d::loadedAnimations;

FrameEx* Loader3d::LoadModel(IDirect3DDevice9* device, const char* fname, bool calc_tangent)
{
	for(FrameVecIt it=loadedModels.begin(),end=loadedModels.end();it!=end;++it)
	{
		FrameEx* frame=*it;
		if(!_stricmp(frame->exFileName,fname)) return frame;
	}

	for(uint i=0,j=(uint)processedFiles.size();i<j;i++)
		if(!_stricmp(processedFiles[i],fname)) return NULL;
	processedFiles.push_back(Str::Duplicate(fname));

	FileManager fm;
	if(!fm.LoadFile(fname,PT_DATA))
	{
		WriteLogF(_FUNC_," - 3d file not found, name<%s>.\n",fname);
		return NULL;
	}

	// Standart direct x loader
	const char* ext=FileManager::GetExtension(fname);
	if(ext && !_stricmp(ext,"x"))
	{
		FrameEx* frame_root=LoadX(device,fm,fname);
		if(frame_root)
		{
			frame_root->exFileName=Str::Duplicate(fname);
			loadedModels.push_back(frame_root);
		}
		else
		{
			WriteLogF(_FUNC_," - Can't load 3d X file, name<%s>.\n",fname);
		}
		return frame_root;
	}

	// Assimp loader
	static Assimp::Importer* importer=NULL;
	if(!importer)
	{
		// Check dll availability
		HMODULE dll=LoadLibrary("Assimp32.dll");
		if(!dll)
		{
			if(GameOpt.ClientPath!="") dll=LoadLibrary((GameOpt.ClientPath+"Assimp32.dll").c_str());

			if(!dll)
			{
				WriteLogF(_FUNC_," - Assimp32.dll not found.\n");
				return NULL;
			}
		}

		// Create importer instance
		importer=new(nothrow) Assimp::Importer();
		if(!importer) return NULL;
	}

	const aiScene* scene=importer->ReadFileFromMemory(fm.GetBuf(),fm.GetFsize(),
		(calc_tangent?aiProcess_CalcTangentSpace:0)|aiProcess_GenNormals|aiProcess_GenUVCoords|
		aiProcess_ConvertToLeftHanded|aiProcess_Triangulate|aiProcess_JoinIdenticalVertices|
		aiProcess_SortByPType|aiProcess_SplitLargeMeshes|aiProcess_LimitBoneWeights|
		aiProcess_ImproveCacheLocality);
	// Todo: optional aiProcess_ValidateDataStructure|aiProcess_FindInvalidData

	if(!scene)
	{
		WriteLogF(_FUNC_," - Can't load 3d file, name<%s>, error<%s>.\n",fname,importer->GetErrorString());
		return NULL;
	}

	// Extract frames
	FrameEx* frame_root=FillNode(device,scene->mRootNode,scene,calc_tangent);
	if(!frame_root)
	{
		WriteLogF(_FUNC_," - Conversion fail, name<%s>.\n",fname);
		importer->FreeScene();
		return NULL;
	}
	frame_root->exFileName=Str::Duplicate(fname);
	loadedModels.push_back(frame_root);

	// Extract animations
	vector<D3DXKEY_VECTOR3> scale;
	vector<D3DXKEY_QUATERNION> rot;
	vector<D3DXKEY_VECTOR3> pos;
	for(unsigned int i=0;i<scene->mNumAnimations;i++)
	{
		aiAnimation* anim=scene->mAnimations[i];

		ID3DXKeyframedAnimationSet* set;
		if(FAILED(D3DXCreateKeyframedAnimationSet(anim->mName.data,anim->mTicksPerSecond,D3DXPLAY_LOOP,anim->mNumChannels,0,NULL,&set)))
		{
			WriteLogF(_FUNC_," - Can't extract animation<%s> from file<%s>.\n",anim->mName.data,fname);
			continue;
		}

		for(unsigned int j=0;j<anim->mNumChannels;j++)
		{
			aiNodeAnim* na=anim->mChannels[j];

			if(scale.size()<na->mNumScalingKeys) scale.resize(na->mNumScalingKeys);
			for(unsigned int k=0;k<na->mNumScalingKeys;k++)
			{
				scale[k].Time=(float)na->mScalingKeys[k].mTime;
				scale[k].Value.x=(float)na->mScalingKeys[k].mValue.x;
				scale[k].Value.y=(float)na->mScalingKeys[k].mValue.y;
				scale[k].Value.z=(float)na->mScalingKeys[k].mValue.z;
			}
			if(rot.size()<na->mNumRotationKeys) rot.resize(na->mNumRotationKeys);
			for(unsigned int k=0;k<na->mNumRotationKeys;k++)
			{
				rot[k].Time=(float)na->mRotationKeys[k].mTime;
				rot[k].Value.x=(float)na->mRotationKeys[k].mValue.x;
				rot[k].Value.y=(float)na->mRotationKeys[k].mValue.y;
				rot[k].Value.z=(float)na->mRotationKeys[k].mValue.z;
				rot[k].Value.w=-(float)na->mRotationKeys[k].mValue.w;
			}
			if(pos.size()<na->mNumPositionKeys) pos.resize(na->mNumPositionKeys);
			for(unsigned int k=0;k<na->mNumPositionKeys;k++)
			{
				pos[k].Time=(float)na->mPositionKeys[k].mTime;
				pos[k].Value.x=(float)na->mPositionKeys[k].mValue.x;
				pos[k].Value.y=(float)na->mPositionKeys[k].mValue.y;
				pos[k].Value.z=(float)na->mPositionKeys[k].mValue.z;
			}

			DWORD index;
			set->RegisterAnimationSRTKeys(na->mNodeName.data,na->mNumScalingKeys,na->mNumRotationKeys,na->mNumPositionKeys,
				na->mNumScalingKeys?&scale[0]:NULL,na->mNumRotationKeys?&rot[0]:NULL,na->mNumPositionKeys?&pos[0]:NULL,&index);
		}

		ID3DXBuffer* buf;
		ID3DXCompressedAnimationSet* cset;
		if(FAILED(set->Compress(D3DXCOMPRESS_DEFAULT,0.0f,NULL,&buf))) continue;
		if(FAILED(D3DXCreateCompressedAnimationSet(anim->mName.data,anim->mTicksPerSecond,D3DXPLAY_LOOP,buf,0,NULL,&cset))) continue;
		buf->Release();
		set->Release();

		loadedAnimationsFNames.push_back(Str::Duplicate(fname));
		loadedAnimations.push_back(cset);
	}

	importer->FreeScene();
	return frame_root;
}

D3DXMATRIX ConvertMatrix(const aiMatrix4x4& mat)
{
	D3DXMATRIX m;
	aiVector3D scale;
	aiQuaternion rot;
	aiVector3D pos;
	mat.Decompose(scale,rot,pos);

	D3DXMATRIX ms,mr,mp;
	D3DXMatrixIdentity(&m);
	D3DXMatrixScaling(&ms,scale.x,scale.y,scale.z);
	D3DXMatrixRotationQuaternion(&mr,&D3DXQUATERNION(rot.x,rot.y,rot.z,rot.w));
	D3DXMatrixTranslation(&mp,pos.x,pos.y,pos.z);
	return ms*mr*mp;
}

FrameEx* Loader3d::FillNode(IDirect3DDevice9* device, const aiNode* node, const aiScene* scene, bool with_tangent)
{
	// Create frame
	FrameEx* frame;
	D3D_HR(memAllocator.CreateFrame(node->mName.data,(D3DXFRAME**)&frame));

	frame->TransformationMatrix=ConvertMatrix(node->mTransformation);

	// Merge meshes, because Assimp split subsets
	if(node->mNumMeshes)
	{
		// Calculate whole data
		uint faces_count=0;
		uint vertices_count=0;
		vector<aiBone*> all_bones;
		vector<D3DXMATERIAL> materials;
		for(unsigned int m=0;m<node->mNumMeshes;m++)
		{
			aiMesh* mesh=scene->mMeshes[node->mMeshes[m]];

			// Faces and vertices
			faces_count+=mesh->mNumFaces;
			vertices_count+=mesh->mNumVertices;

			// Shared bones
			for(unsigned int i=0;i<mesh->mNumBones;i++)
			{
				aiBone* bone=mesh->mBones[i];
				bool bone_aviable=false;
				for(uint b=0,bb=(uint)all_bones.size();b<bb;b++)
				{
					if(!strcmp(all_bones[b]->mName.data,bone->mName.data))
					{
						bone_aviable=true;
						break;
					}
				}
				if(!bone_aviable) all_bones.push_back(bone);
			}

			// Material
			D3DXMATERIAL material;
			ZeroMemory(&material,sizeof(material));
			aiMaterial* mtrl=scene->mMaterials[mesh->mMaterialIndex];

			aiString path;
			if(mtrl->GetTextureCount(aiTextureType_DIFFUSE))
			{
				mtrl->GetTexture(aiTextureType_DIFFUSE,0,&path);
				material.pTextureFilename=Str::Duplicate(path.data);
			}

			mtrl->Get(AI_MATKEY_COLOR_DIFFUSE,material.MatD3D.Diffuse);
			mtrl->Get(AI_MATKEY_COLOR_AMBIENT,material.MatD3D.Ambient);
			mtrl->Get(AI_MATKEY_COLOR_SPECULAR,material.MatD3D.Specular);
			mtrl->Get(AI_MATKEY_COLOR_EMISSIVE,material.MatD3D.Emissive);
			mtrl->Get(AI_MATKEY_SHININESS,material.MatD3D.Power);

			materials.push_back(material);
		}

		// Mesh declarations
		D3DVERTEXELEMENT9 delc_simple[]=
		{
			{0, 0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
			{0,12,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_NORMAL  ,0},
			{0,24,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
			D3DDECL_END()
		};
		D3DVERTEXELEMENT9 delc_tangent[]=
		{
			{0, 0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
			{0,12,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_NORMAL  ,0},
			{0,24,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
			{0,32,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TANGENT ,0},
			{0,44,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_BINORMAL,0},
			D3DDECL_END()
		};
		D3DVERTEXELEMENT9* declaration=(with_tangent?delc_tangent:delc_simple);

		// Create mesh
		D3DXMESHDATA dxmesh;
		dxmesh.Type=D3DXMESHTYPE_MESH;
		D3D_HR(D3DXCreateMesh(faces_count,vertices_count,D3DXMESH_MANAGED,declaration,device,&dxmesh.pMesh));

		// Skinning
		ID3DXSkinInfo* skin_info=NULL;

		// Fill data
		struct Vertex
		{
			float x,y,z;
			float nx,ny,nz;
			float u,v;
			float tx,ty,tz;
			float bx,by,bz;
		} *vb;
		int vsize=(with_tangent?56:32);
		WORD* ib;
		DWORD* att;
		D3D_HR(dxmesh.pMesh->LockVertexBuffer(0,(void**)&vb));
		D3D_HR(dxmesh.pMesh->LockIndexBuffer(0,(void**)&ib));
		D3D_HR(dxmesh.pMesh->LockAttributeBuffer(0,&att));

		uint cur_vertices=0;
		uint cur_faces=0;
		for(unsigned int m=0;m<node->mNumMeshes;m++)
		{
			aiMesh* mesh=scene->mMeshes[node->mMeshes[m]];

			for(unsigned int i=0;i<mesh->mNumVertices;i++)
			{
				Vertex& v=*vb;
				vb=(Vertex*)(((char*)vb)+vsize);
				ZeroMemory(&v,vsize);

				// Vertices data
				v.x=mesh->mVertices[i].x;
				v.y=mesh->mVertices[i].y;
				v.z=mesh->mVertices[i].z;

				if(mesh->mTextureCoords && mesh->mTextureCoords[0])
				{
					v.u=mesh->mTextureCoords[0][i].x;
					v.v=mesh->mTextureCoords[0][i].y;
				}
				if(mesh->mNormals)
				{
					v.nx=mesh->mNormals[i].x;
					v.ny=mesh->mNormals[i].y;
					v.nz=mesh->mNormals[i].z;
				}
				if(with_tangent && mesh->mTangents)
				{
					v.tx=mesh->mTangents[i].x;
					v.ty=mesh->mTangents[i].y;
					v.tz=mesh->mTangents[i].z;
					v.bx=mesh->mBitangents[i].x;
					v.by=mesh->mBitangents[i].y;
					v.bz=mesh->mBitangents[i].z;
				}
			}

			// Indices
			for(unsigned int i=0;i<mesh->mNumFaces;i++)
			{
				aiFace& face=mesh->mFaces[i];
				for(unsigned int j=0;j<face.mNumIndices;j++)
				{
					*ib=face.mIndices[j]+cur_vertices;
					ib++;
				}

				*(att+cur_faces+i)=m;
			}

			cur_vertices+=mesh->mNumVertices;
			cur_faces+=mesh->mNumFaces;
		}

		// Skin info
		if(all_bones.size())
		{
			D3D_HR(D3DXCreateSkinInfo(vertices_count,declaration,(uint)all_bones.size(),&skin_info));

			vector<vector<DWORD>> vertices(all_bones.size());
			vector<FloatVec> weights(all_bones.size());

			for(uint b=0,bb=(uint)all_bones.size();b<bb;b++)
			{
				aiBone* bone=all_bones[b];

				// Bone options
				skin_info->SetBoneName(b,bone->mName.data);
				skin_info->SetBoneOffsetMatrix(b,&ConvertMatrix(bone->mOffsetMatrix));

				// Reserve memory
				vertices.reserve(vertices_count);
				weights.reserve(vertices_count);
			}

			cur_vertices=0;
			for(unsigned int m=0;m<node->mNumMeshes;m++)
			{
				aiMesh* mesh=scene->mMeshes[node->mMeshes[m]];

				for(unsigned int i=0;i<mesh->mNumBones;i++)
				{
					aiBone* bone=mesh->mBones[i];

					// Get bone id
					uint bone_id=0;
					for(uint b=0,bb=(uint)all_bones.size();b<bb;b++)
					{
						if(!strcmp(all_bones[b]->mName.data,bone->mName.data))
						{
							bone_id=b;
							break;
						}
					}

					// Fill weights
					if(bone->mNumWeights)
					{
						for(unsigned int j=0;j<bone->mNumWeights;j++)
						{
							aiVertexWeight& vw=bone->mWeights[j];
							vertices[bone_id].push_back(vw.mVertexId+cur_vertices);
							weights[bone_id].push_back(vw.mWeight);
						}
					}
				}

				cur_vertices+=mesh->mNumVertices;
			}

			// Set influences
			for(uint b=0,bb=(uint)all_bones.size();b<bb;b++)
			{
				if(vertices[b].size())
					skin_info->SetBoneInfluence(b,(uint)vertices[b].size(),&vertices[b][0],&weights[b][0]);
			}
		}

		D3D_HR(dxmesh.pMesh->UnlockVertexBuffer());
		D3D_HR(dxmesh.pMesh->UnlockIndexBuffer());
		D3D_HR(dxmesh.pMesh->UnlockAttributeBuffer());
		
		// Create container
		D3DXMESHCONTAINER* mesh_container;
		D3D_HR(memAllocator.CreateMeshContainer(node->mName.data,&dxmesh,&materials[0],NULL,(uint)materials.size(),NULL,skin_info,&mesh_container));
		dxmesh.pMesh->Release();
		if(skin_info) skin_info->Release();

		frame->pMeshContainer=mesh_container;
	}

	// Childs
	FrameEx* frame_last=NULL;
	for(unsigned int i=0;i<node->mNumChildren;i++)
	{
		aiNode* node_child=node->mChildren[i];
		FrameEx* frame_child=FillNode(device,node_child,scene,with_tangent);
		if(!i) frame->pFrameFirstChild=frame_child;
		else frame_last->pFrameSibling=frame_child;
		frame_last=frame_child;
	}

	return frame;
}

AnimSet* Loader3d::LoadAnimation(IDirect3DDevice9* device, const char* anim_fname, const char* anim_name)
{
	// Find in already loaded
	for(uint i=0,j=(uint)loadedAnimations.size();i<j;i++)
	{
		if(!_stricmp(loadedAnimationsFNames[i],anim_fname) &&
			!_stricmp(loadedAnimations[i]->GetName(),anim_name)) return loadedAnimations[i];
	}

	// Check maybe file already processed and nothing founded
	for(uint i=0,j=(uint)processedFiles.size();i<j;i++)
		if(!_stricmp(processedFiles[i],anim_fname)) return NULL;

	// File not processed, load and recheck animations
	if(LoadModel(device,anim_fname,false)) return LoadAnimation(device,anim_fname,anim_name);

	return NULL;
}

void Loader3d::Free(D3DXFRAME* frame)
{
	// Free frame
	if(frame) D3DXFrameDestroy(frame,&memAllocator);
}

FrameEx* Loader3d::LoadX(IDirect3DDevice9* device, FileManager& fm, const char* fname)
{
	// Load model
	ID3DXAnimationController* anim=NULL;
	FrameEx* frame_root=NULL;
	HRESULT hr=D3DXLoadMeshHierarchyFromXInMemory(fm.GetBuf(),fm.GetFsize(),D3DXMESH_MANAGED,device,&memAllocator,NULL,
		(D3DXFRAME**)&frame_root,&anim);
	if(hr!=D3D_OK)
	{
		WriteLogF(_FUNC_," - Can't load X file hierarchy, error<%s>.\n",DXGetErrorString(hr));
		return NULL;
	}

	// If animation aviable than store it
	if(anim)
	{
		for(uint i=0,j=anim->GetNumAnimationSets();i<j;i++)
		{
			AnimSet* set;
			anim->GetAnimationSet(i,&set); // AddRef
			loadedAnimationsFNames.push_back(Str::Duplicate(fname));
			loadedAnimations.push_back(set);
		}
		anim->Release();
	}

	return frame_root;
}

bool Loader3d::IsExtensionSupported(const char* ext)
{
	static const char* arr[]=
	{
		"fo3d","x","3ds","obj","dae","blend","ase","ply","dxf","lwo","lxo","stl","ac","ms3d",
		"scn","smd","vta","mdl","md2","md3","pk3","mdc","md5","bvh","csm","b3d","q3d","cob",
		"q3s","mesh","xml","irrmesh","irr","nff","nff","off","raw","ter","mdl","hmp","ndo"
	};

	for(int i=0,j=sizeof(arr)/sizeof(arr[0]);i<j;i++)
		if(!_stricmp(ext,arr[i])) return true;
	return false;
}

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
		Str::Append(path,texture_name);
		fm.LoadFile(path,model_path_type);
	}

	// Create texture
	IDirect3DTexture9* texture=NULL;
	if(!fm.IsLoaded() || FAILED(D3DXCreateTextureFromFileInMemory(device,fm.GetBuf(),fm.GetFsize(),&texture))) return NULL;

	TextureEx* texture_ex=new TextureEx();
	texture_ex->Name=Str::Duplicate(texture_name);
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
			Str::Append(path,pFileName);
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
		EffectEx* effect_ex=*it;
		if(!_stricmp(effect_ex->Name,effect_name) && effect_ex->Defaults==effect_inst->pDefaults) return effect_ex;
	}

	// First try load from effects folder
	FileManager fm;
	if(!fm.LoadFile(effect_name,PT_EFFECTS) && model_path)
	{
		// After try load from file folder
		char path[MAX_FOPATH];
		FileManager::ExtractPath(model_path,path);
		Str::Append(path,effect_name);
		fm.LoadFile(path,model_path_type);
	}
	if(!fm.IsLoaded()) return NULL;

	// Find already compiled effect in cache
	char cache_name[MAX_FOPATH];
	sprintf(cache_name,"%s.fxc",effect_name);
	FileManager fm_cache;
	if(fm_cache.LoadFile(cache_name,PT_CACHE))
	{
		uint64 last_write,last_write_cache;
		fm.GetTime(NULL,NULL,&last_write);
		fm_cache.GetTime(NULL,NULL,&last_write_cache);
		if(last_write>last_write_cache) fm_cache.UnloadFile();
	}

	// Load and cache effect
	if(!fm_cache.IsLoaded())
	{
		char* buf=(char*)fm.GetBuf();
		uint size=fm.GetFsize();

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
				WriteLogF(_FUNC_," - Unable to compile effect, effect<%s>, errors<\n%s>.\n",effect_name,errors?errors->GetBufferPointer():"nullptr\n");
			}
		}
		else
		{
			WriteLogF(_FUNC_," - Unable to create effect compiler, effect<%s>, errors<%s\n%s>, legacy compiler errors<%s\n%s>.\n",effect_name,
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
		WriteLogF(_FUNC_," - Unable to create effect, effect<%s>, errors<\n%s>.\n",effect_name,errors?errors->GetBufferPointer():"nullptr");
		SAFEREL(effect);
		SAFEREL(errors);
		return NULL;
	}
	SAFEREL(errors);

	EffectEx* effect_ex=new EffectEx();
	effect_ex->Name=Str::Duplicate(effect_name);
	effect_ex->Effect=effect;
	effect_ex->EffectFlags=D3DXFX_DONOTSAVESTATE;
	effect_ex->Defaults=NULL;
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
	effect_ex->ViewProjMatrix=effect->GetParameterByName(NULL,"ViewProjMatrix");

	if(!effect_ex->TechniqueSimple)
	{
		WriteLogF(_FUNC_," - Technique 'Simple' not founded, effect<%s>.\n",effect_name);
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
	effect_ex->IsTextures=false;
	for(int i=0;i<EFFECT_TEXTURES;i++)
	{
		char tex_name[32];
		sprintf(tex_name,"Texture%d",i);
		effect_ex->Textures[i]=effect->GetParameterByName(NULL,tex_name);
		if(effect_ex->Textures[i]) effect_ex->IsTextures=true;
	}
	effect_ex->IsScriptValues=false;
	for(int i=0;i<EFFECT_SCRIPT_VALUES;i++)
	{
		char val_name[32];
		sprintf(val_name,"EffectValue%d",i);
		effect_ex->ScriptValues[i]=effect->GetParameterByName(NULL,val_name);
		if(effect_ex->ScriptValues[i]) effect_ex->IsScriptValues=true;
	}
	effect_ex->AnimPosProc=effect->GetParameterByName(NULL,"AnimPosProc");
	effect_ex->AnimPosTime=effect->GetParameterByName(NULL,"AnimPosTime");
	effect_ex->IsAnimPos=(effect_ex->AnimPosProc || effect_ex->AnimPosTime);
	effect_ex->IsNeedProcess=(effect_ex->PassIndex || effect_ex->IsTime || effect_ex->IsRandomPass || effect_ex->IsRandomEffect ||
		effect_ex->IsTextures || effect_ex->IsScriptValues || effect_ex->IsAnimPos);

	if(effect_inst->NumDefaults)
	{
		effect->BeginParameterBlock();
		for(uint d=0;d<effect_inst->NumDefaults;d++)
		{
			D3DXEFFECTDEFAULT& def=effect_inst->pDefaults[d];
			D3DXHANDLE param=effect->GetParameterByName(NULL,def.pParamName);
			if(!param) continue;
			switch(def.Type)
			{
			case D3DXEDT_STRING: // pValue points to a null terminated ASCII string 
				effect->SetString(param,(LPCSTR)def.pValue); break;
			case D3DXEDT_FLOATS: // pValue points to an array of floats - number of floats is NumBytes / sizeof(float)
				effect->SetFloatArray(param,(FLOAT*)def.pValue,def.NumBytes/sizeof(FLOAT)); break;
			case D3DXEDT_DWORD:  // pValue points to a uint
				effect->SetInt(param,*(uint*)def.pValue); break;
			default: break;
			}
		}
		effect_ex->EffectParams=effect->EndParameterBlock();
		effect_ex->Defaults=effect_inst->pDefaults;
	}

	loadedEffects.push_back(effect_ex);
	return loadedEffects.back();
}

void Loader3d::EffectProcessVariables(EffectEx* effect_ex, int pass,  float anim_proc /* = 0.0f */, float anim_time /* = 0.0f */, TextureEx** textures /* = NULL */)
{
	// Process effect
	if(pass==-1)
	{
		if(effect_ex->IsTime)
		{
			double tick=Timer::AccurateTick();
			if(effect_ex->Time)
			{
				effect_ex->TimeCurrent+=(float)(tick-effect_ex->TimeLastTick);
				effect_ex->TimeLastTick=tick;
				if(effect_ex->TimeCurrent>=120.0f) effect_ex->TimeCurrent=fmod(effect_ex->TimeCurrent,120.0f);

				effect_ex->Effect->SetFloat(effect_ex->Time,effect_ex->TimeCurrent);
			}
			if(effect_ex->TimeGame)
			{
				if(!Timer::IsGamePaused())
				{
					effect_ex->TimeGameCurrent+=(float)(tick-effect_ex->TimeGameLastTick);
					effect_ex->TimeGameLastTick=tick;
					if(effect_ex->TimeGameCurrent>=120.0f) effect_ex->TimeGameCurrent=fmod(effect_ex->TimeGameCurrent,120.0f);
				}
				else
				{
					effect_ex->TimeGameLastTick=tick;
				}

				effect_ex->Effect->SetFloat(effect_ex->TimeGame,effect_ex->TimeGameCurrent);
			}
		}

		if(effect_ex->IsRandomEffect)
		{
			if(effect_ex->Random1Effect) effect_ex->Effect->SetFloat(effect_ex->Random1Effect,(float)((double)Random(0,2000000000)/2000000000.0));
			if(effect_ex->Random2Effect) effect_ex->Effect->SetFloat(effect_ex->Random2Effect,(float)((double)Random(0,2000000000)/2000000000.0));
			if(effect_ex->Random3Effect) effect_ex->Effect->SetFloat(effect_ex->Random3Effect,(float)((double)Random(0,2000000000)/2000000000.0));
			if(effect_ex->Random4Effect) effect_ex->Effect->SetFloat(effect_ex->Random4Effect,(float)((double)Random(0,2000000000)/2000000000.0));
		}

		if(effect_ex->IsTextures)
		{
			for(int i=0;i<EFFECT_TEXTURES;i++)
				if(effect_ex->Textures[i])
					effect_ex->Effect->SetTexture(effect_ex->Textures[i],textures && textures[i]?textures[i]->Texture:NULL);
		}

		if(effect_ex->IsScriptValues)
		{
			for(int i=0;i<EFFECT_SCRIPT_VALUES;i++)
				if(effect_ex->ScriptValues[i])
					effect_ex->Effect->SetFloat(effect_ex->ScriptValues[i],GameOpt.EffectValues[i]);
		}

		if(effect_ex->IsAnimPos)
		{
			if(effect_ex->AnimPosProc) effect_ex->Effect->SetFloat(effect_ex->AnimPosProc,anim_proc);
			if(effect_ex->AnimPosTime) effect_ex->Effect->SetFloat(effect_ex->AnimPosTime,anim_time);
		}
	}
	// Process pass
	else
	{
		if(effect_ex->PassIndex) effect_ex->Effect->SetFloat(effect_ex->Random1Pass,(float)pass);

		if(effect_ex->IsRandomPass)
		{
			if(effect_ex->Random1Pass) effect_ex->Effect->SetFloat(effect_ex->Random1Pass,(float)((double)Random(0,2000000000)/2000000000.0));
			if(effect_ex->Random2Pass) effect_ex->Effect->SetFloat(effect_ex->Random2Pass,(float)((double)Random(0,2000000000)/2000000000.0));
			if(effect_ex->Random3Pass) effect_ex->Effect->SetFloat(effect_ex->Random3Pass,(float)((double)Random(0,2000000000)/2000000000.0));
			if(effect_ex->Random4Pass) effect_ex->Effect->SetFloat(effect_ex->Random4Pass,(float)((double)Random(0,2000000000)/2000000000.0));
		}
	}
}

bool Loader3d::EffectsPreRestore()
{
	for(EffectExVecIt it=loadedEffects.begin(),end=loadedEffects.end();it!=end;++it)
	{
		EffectEx* effect_ex=*it;
		D3D_HR(effect_ex->Effect->OnLostDevice());
	}
	return true;
}

bool Loader3d::EffectsPostRestore()
{
	for(EffectExVecIt it=loadedEffects.begin(),end=loadedEffects.end();it!=end;++it)
	{
		EffectEx* effect_ex=*it;
		D3D_HR(effect_ex->Effect->OnResetDevice());
	}
	return true;
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
