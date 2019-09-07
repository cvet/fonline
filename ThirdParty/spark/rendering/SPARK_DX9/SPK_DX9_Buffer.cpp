//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2008-2011 - Julien Fryer - julienfryer@gmail.com				//
//                           foulon matthieu - stardeath@wanadoo.fr				//
//																				//
// This software is provided 'as-is', without any express or implied			//
// warranty.  In no event will the authors be held liable for any damages		//
// arising from the use of this software.										//
//																				//
// Permission is granted to anyone to use this software for any purpose,		//
// including commercial applications, and to alter it and redistribute it		//
// freely, subject to the following restrictions:								//
//																				//
// 1. The origin of this software must not be misrepresented; you must not		//
//    claim that you wrote the original software. If you use this software		//
//    in a product, an acknowledgment in the product documentation would be		//
//    appreciated but is not required.											//
// 2. Altered source versions must be plainly marked as such, and must not be	//
//    misrepresented as being the original software.							//
// 3. This notice may not be removed or altered from any source distribution.	//
//////////////////////////////////////////////////////////////////////////////////

#include <SPARK_Core.h>
#include "Rendering/DX9/SPK_DX9_Buffer.h"
#include "Rendering/DX9/SPK_DX9_Info.h"

namespace SPK
{
namespace DX9
{
	DX9Buffer::DX9Buffer(BufferInfo &info) :
		nbVertices(info.nbVertices),
		nbIndices(info.nbIndices),
		nbTexCoords(info.nbTexCoords),
		currentVertexIndex(0),
		currentColorIndex(0),
		currentTexCoordIndex(0),

		currentLock(NO_LOCK),
		vertexBuffer(NULL), colorBuffer(NULL), texCoordBuffer(NULL), indexBuffer(NULL),
		ptrVertexBuffer(NULL), ptrColorBuffer(NULL), ptrTexCoordBuffer(NULL), ptrIndexBuffer(NULL)
	{
		SPK_ASSERT(nbVertices > 0,"DX9Buffer::DX9Buffer(BufferInfo) - The number of vertices cannot be 0");

		DX9Info::getDevice()->CreateVertexBuffer(nbVertices*sizeof(D3DXVECTOR3), D3DUSAGE_DYNAMIC, D3DFVF_XYZ, D3DPOOL_DEFAULT, &vertexBuffer, NULL);
		DX9Info::getDevice()->CreateVertexBuffer(nbVertices*sizeof(D3DCOLOR), D3DUSAGE_DYNAMIC, D3DFVF_DIFFUSE, D3DPOOL_DEFAULT, &colorBuffer, NULL);

		// TODO : gérer les indices 32bit
		if( nbIndices > 0 )
		{
			DX9Info::getDevice()->CreateIndexBuffer(nbIndices*sizeof(short), 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &indexBuffer, 0);

			unsigned int offsetIndex = 0;

			lock(INDEX_LOCK);
			// initialisation de l'index buffer
			for(size_t i = 0; i < nbIndices/6; i++)
			{
//#ifdef _DX9QUADRENDERER_CLOCKWISE_
				*(ptrIndexBuffer++) = 0 + offsetIndex;
				*(ptrIndexBuffer++) = 1 + offsetIndex;
				*(ptrIndexBuffer++) = 2 + offsetIndex;
				*(ptrIndexBuffer++) = 0 + offsetIndex;
				*(ptrIndexBuffer++) = 2 + offsetIndex;
				*(ptrIndexBuffer++) = 3 + offsetIndex;
// TODO handle counter clockwise
//#else
//				*(ptrIndexBuffer++) = 0 + offsetIndex;
//				*(ptrIndexBuffer++) = 2 + offsetIndex;
//				*(ptrIndexBuffer++) = 1 + offsetIndex;
//				*(ptrIndexBuffer++) = 0 + offsetIndex;
//				*(ptrIndexBuffer++) = 3 + offsetIndex;
//				*(ptrIndexBuffer++) = 2 + offsetIndex;
//#endif
				offsetIndex += 4;
			}
			unlock();
		}

		// TODO : gérer autre chose que les textures 2D
		if(nbTexCoords > 0)
			DX9Info::getDevice()->CreateVertexBuffer(nbVertices*sizeof(D3DXVECTOR2), D3DUSAGE_DYNAMIC, D3DFVF_TEX1|D3DFVF_TEXCOORDSIZE1(nbTexCoords), D3DPOOL_DEFAULT, &texCoordBuffer, NULL);
	}

	DX9Buffer::~DX9Buffer()
	{
		SAFE_RELEASE( vertexBuffer );
		SAFE_RELEASE( colorBuffer );
		SAFE_RELEASE( texCoordBuffer );
		SAFE_RELEASE( indexBuffer );
	}

	void DX9Buffer::setNbTexCoords(size_t nb)
	{
		if (nbTexCoords != nb)
		{
			nbTexCoords = nb;
			SAFE_RELEASE( texCoordBuffer );
			if (nbTexCoords > 0)
				DX9Info::getDevice()->CreateVertexBuffer(nbVertices*nbTexCoords*sizeof(float), D3DUSAGE_DYNAMIC, D3DFVF_TEX1|D3DFVF_TEXCOORDSIZE1(nbTexCoords), D3DPOOL_DEFAULT, &texCoordBuffer, NULL);
			currentTexCoordIndex = 0;
		}
	}

	void DX9Buffer::render(D3DPRIMITIVETYPE primitive, size_t nbPrimitives)
	{
		DX9Info::getDevice()->SetVertexDeclaration(DX9Info::getDecl(nbTexCoords));

		DX9Info::getDevice()->SetStreamSource(0, vertexBuffer, 0, sizeof(D3DXVECTOR3));
		DX9Info::getDevice()->SetStreamSource(1, colorBuffer, 0, sizeof(D3DCOLOR));

		if( nbTexCoords > 0 )
			DX9Info::getDevice()->SetStreamSource(2, texCoordBuffer, 0, nbTexCoords*sizeof(float));

		// TODO : DrawPrimitive prend PrimitiveCount, pas nbVertices !!!
		if( nbIndices == 0 )
			DX9Info::getDevice()->DrawPrimitive(primitive, 0, nbPrimitives);
		else
		{
			DX9Info::getDevice()->SetIndices(indexBuffer);
			DX9Info::getDevice()->DrawIndexedPrimitive(primitive, 0, 0, nbPrimitives<<2, 0, nbPrimitives<<1);
		}
	}
}}
