#include "StdAfx.h"
#include "Loader.h"
#include "Text.h"
#include "Timer.h"

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
	static vector<Material3ds> materials;
	materials.clear();

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
	static vector<Mesh3ds> meshes;
	meshes.clear();

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
				vb[i].u=mesh.TexCoords[i*2+0];
				vb[i].v=mesh.TexCoords[i*2+1];
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
			DWORD* adjacency=new DWORD[mesh.Faces.size()];
			dxmesh.pMesh->GenerateAdjacency(0.0000125f,adjacency);
			D3D_HR(D3DXComputeNormals(dxmesh.pMesh,adjacency));

			// Create container
			D3D_HR(memAllocator.CreateMeshContainer(mesh.Name,&dxmesh,&dxmaterials[0],NULL,dxmaterials.size(),adjacency,NULL,&frame->pMeshContainer));
			delete[] adjacency;
		}
	}

	return frame_root;
};

/************************************************************************/
/*                                                                      */
/************************************************************************/
