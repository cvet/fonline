// Based on http://www.codesampler.com/source/dx9_shadow_volume.zip

#include <vector>

class ShadowVolumeCreator
{
public:
	ShadowVolumeCreator()
	{
		svVolumes.resize(100);
		svVolumesSize=0;
	}

    void Reset()
	{
		svVolumesSize=0;
	}

    HRESULT BuildShadowVolume(const LPD3DXMESH mesh, const D3DXVECTOR3& look, const D3DMATRIX& world)
	{
		// Takes a mesh as input, and uses it to build a shadow volume. The
		// technique used considers each triangle of the mesh, and adds it's
		// edges to a temporary list. The edge list is maintained, such that
		// only silohuette edges are kept. Finally, the silohuette edges are
		// extruded to make the shadow volume vertex list.

		//DWORD fvf=mesh->GetFVF();
		// Note: the MeshVertex format depends on the FVF of the mesh
		struct MeshVertex { D3DXVECTOR3 xyz,n; FLOAT tu,tv;};
		if(mesh->GetNumBytesPerVertex()!=sizeof(MeshVertex)) return S_FALSE;

		// Lock the geometry buffers
		MeshVertex* vertices;
		WORD* indicies;
		mesh->LockVertexBuffer(0,(void**)&vertices);
		mesh->LockIndexBuffer(0,(void**)&indicies);
		DWORD faces_count=mesh->GetNumFaces();
		DWORD edges_count=0;

		// For each face
		for(DWORD i=0;i<faces_count;++i)
		{
			WORD face0=indicies[3*i+0];
			WORD face1=indicies[3*i+1];
			WORD face2=indicies[3*i+2];

			D3DXVECTOR3 v0=vertices[face0].xyz;
			D3DXVECTOR3 v1=vertices[face1].xyz;
			D3DXVECTOR3 v2=vertices[face2].xyz;

			// Transform vertices or transform light?
			D3DXVECTOR3 cross1(v2-v1);
			D3DXVECTOR3 cross2(v1-v0);
			D3DXVECTOR3 normal;
			D3DXVec3Cross(&normal,&cross1,&cross2);
			if(D3DXVec3Dot(&normal,&look)>=0.0f)
			{
				AddEdge(edges_count,face0,face1);
				AddEdge(edges_count,face1,face2);
				AddEdge(edges_count,face2,face0);
			}
		}

		svVolumesSize++;
		if(svVolumes.size()<svVolumesSize) svVolumes.push_back(LineList());

		LineList& ll=svVolumes[svVolumesSize-1];
		ll.world=world;
		ll.vertices.resize(edges_count*2);

		for(DWORD i=0;i<edges_count;i++)
		{
			ll.vertices[i*2+0].xyz=vertices[svEdges[i*2+0]].xyz;
			ll.vertices[i*2+0].color=D3DCOLOR_XRGB(255,0,0);
			ll.vertices[i*2+1].xyz=vertices[svEdges[i*2+1]].xyz;
			ll.vertices[i*2+1].color=D3DCOLOR_XRGB(255,0,0);

			/*ll.vertices[i*2+0].xyz=vertices[svEdges[i*2+0]].xyz;
			ll.vertices[i*2+0].color=D3DCOLOR_XRGB(255,0,0);
			ll.vertices[i*2+2].xyz=vertices[svEdges[i*2+1]].xyz;
			ll.vertices[i*2+2].color=D3DCOLOR_XRGB(255,0,0);
			ll.vertices[i*2+1].xyz=vertices[svEdges[i*2+0]].xyz-look*0.3f;
			ll.vertices[i*2+1].color=D3DCOLOR_XRGB(255,0,0);
			ll.vertices[i*2+3].xyz=vertices[svEdges[i*2+1]].xyz-look*0.3f;
			ll.vertices[i*2+3].color=D3DCOLOR_XRGB(255,0,0);*/
		}

		// Unlock the geometry buffers
		mesh->UnlockVertexBuffer();
		mesh->UnlockIndexBuffer();
		return S_OK;
	}

    HRESULT Render(const LPDIRECT3DDEVICE9 device)
	{
		device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
		for(DWORD i=0;i<svVolumesSize;i++)
		{
			LineList& ll=svVolumes[i];
			device->SetTransform(D3DTS_WORLD,&ll.world);
			device->DrawPrimitiveUP(D3DPT_LINELIST,ll.vertices.size()/2,&ll.vertices[0],sizeof(ll.vertices[0]));
			//device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,ll.vertices.size()/3,&ll.vertices[0],sizeof(ll.vertices[0]));
		}
		return S_OK;
	}

private:
	void AddEdge(DWORD& edges_count, WORD v0, WORD v1)
	{
		// Adds an edge to a list of silohuette edges of a shadow volume
		// Remove interior edges (which appear in the list twice)
		/*for(DWORD i=0;i<edges_count;++i)
		{
			if((svEdges[2*i+0]==v0 && svEdges[2*i+1]==v1)
			|| (svEdges[2*i+0]==v1 && svEdges[2*i+1]==v0))
			{
				if(edges_count>1)
				{
					svEdges[2*i+0]=svEdges[2*(edges_count-1)+0];
					svEdges[2*i+1]=svEdges[2*(edges_count-1)+1];
				}

				--edges_count;
				return;
			}
		}*/

		svEdges[edges_count*2+0]=v0;
		svEdges[edges_count*2+1]=v1;
		edges_count++;
	}

	struct LineVertex{ D3DXVECTOR3 xyz; DWORD color;};
typedef vector<LineVertex> LineVertexVec;
	struct LineList{ D3DMATRIX world; LineVertexVec vertices; };
typedef vector<LineList> LineListVec;

	LineListVec svVolumes;
	DWORD svVolumesSize;
	WORD svEdges[64000];
};

