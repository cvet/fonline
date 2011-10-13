#ifndef __MESH_HIERARHY__
#define __MESH_HIERARHY__

#include "3dMeshStructures.h"

class MeshHierarchy: public ID3DXAllocateHierarchy
{
public:
    // Callback to create a D3DXFRAME extended object and initialize it
    STDMETHOD( CreateFrame ) ( LPCSTR Name, LPD3DXFRAME * retNewFrame );

    // Callback to create a D3DXMESHCONTAINER extended object and initialize it
    STDMETHOD( CreateMeshContainer ) ( LPCSTR Name, CONST D3DXMESHDATA * meshData,
                                       CONST D3DXMATERIAL * materials, CONST EffectInstanceType * effectInstances,
                                       DWORD numMaterials, CONST DWORD * adjacency, LPD3DXSKININFO skinInfo,
                                       LPD3DXMESHCONTAINER * retNewMeshContainer );

    // Callback to release a D3DXFRAME extended object
    STDMETHOD( DestroyFrame ) ( LPD3DXFRAME frameToFree );

    // Callback to release a D3DXMESHCONTAINER extended object
    STDMETHOD( DestroyMeshContainer ) ( LPD3DXMESHCONTAINER meshContainerToFree );
};

#endif // __MESH_HIERARHY__
