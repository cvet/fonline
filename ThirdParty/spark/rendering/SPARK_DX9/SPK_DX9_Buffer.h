//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2008-2013 - Julien Fryer - julienfryer@gmail.com				//
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

#ifndef H_SPK_DX9_BUFFER
#define H_SPK_DX9_BUFFER

#include "Rendering/DX9/SPK_DX9_DEF.h"

namespace SPK
{
namespace DX9
{
	const unsigned int NO_LOCK					= 0;
	const unsigned int VERTEX_LOCK				= 1 << 0;
	const unsigned int COLOR_LOCK				= 1 << 1;
	const unsigned int TEXCOORD_LOCK			= 1 << 2;
	const unsigned int INDEX_LOCK				= 1 << 3;
	const unsigned int VERTEX_AND_COLOR_LOCK	= VERTEX_LOCK | COLOR_LOCK;

	const unsigned int ALL_LOCK					= 0xFFFFFFFF;
	const unsigned int ALL_WO_INDEX_LOCK		= ALL_LOCK & ~INDEX_LOCK;
	const unsigned int ALL_WO_TEXCOORD_LOCK		= ALL_LOCK & ~TEXCOORD_LOCK;

	struct BufferInfo
	{
		size_t nbVertices;
		size_t nbTexCoords;
		size_t nbIndices;
	};

	class SPK_DX9_PREFIX DX9Buffer : public RenderBuffer
	{
	public :

		DX9Buffer(BufferInfo &info);
		~DX9Buffer();

		void lock(unsigned int lock);
		void unlock();

		void copy(unsigned int dst, void *src, size_t size);

		void positionAtStart();

		void setNextVertex(const Vector3D& vertex);

		void setNextColor(const Color& color);

		void setNextTexCoord(float texCoord);
		void skipNextTexCoords(size_t nb);

		void setNbTexCoords(size_t nb);
		size_t getNbTexCoords();

		// WARNING : le draw en dx prend en compte le nombre de primitives pas le nombre de sommets !!!
		// WARNING : draw call takes primitive number not vertex number
		void render(D3DPRIMITIVETYPE primitive, size_t nbPrimitives);

	private :

		const size_t nbVertices;
		const size_t nbIndices;
		size_t nbTexCoords;

		unsigned int currentLock;

		LPDIRECT3DVERTEXBUFFER9 vertexBuffer;
		LPDIRECT3DVERTEXBUFFER9 colorBuffer;
		LPDIRECT3DVERTEXBUFFER9 texCoordBuffer;
		LPDIRECT3DINDEXBUFFER9  indexBuffer;

		size_t currentVertexIndex;
		size_t currentColorIndex;
		size_t currentTexCoordIndex;

		// pointeurs pour les locks
		D3DXVECTOR3 *ptrVertexBuffer;
		D3DCOLOR    *ptrColorBuffer;
		float       *ptrTexCoordBuffer;
		short       *ptrIndexBuffer;
	};

	inline void DX9Buffer::lock(unsigned int lock)
	{
		SPK_ASSERT(currentLock == NO_LOCK, "DX9Buffer::lock(unsigned int) - a lock is already set on this buffer");

#ifdef _DEBUG
		HRESULT hr = S_OK;
#endif

		currentLock = lock;
		if( lock & VERTEX_LOCK )
		{
#ifdef _DEBUG
			hr =
#endif
			vertexBuffer->Lock(0, 0, (void**)&ptrVertexBuffer, D3DLOCK_DISCARD);
#ifdef _DEBUG
			SPK_ASSERT(hr == S_OK, "DX9Buffer::lock(unsigned int) - vertex buffer can't be locked");
#endif
		}
		if( lock & COLOR_LOCK )
		{
#ifdef _DEBUG
			hr =
#endif
			colorBuffer->Lock(0, 0, (void**)&ptrColorBuffer, D3DLOCK_DISCARD);
#ifdef _DEBUG
			SPK_ASSERT(hr == S_OK, "DX9Buffer::lock(unsigned int) - color buffer can't be locked");
#endif
		}
		if( lock & TEXCOORD_LOCK && nbTexCoords > 0 )
		{
#ifdef _DEBUG
			hr =
#endif
			texCoordBuffer->Lock(0, 0, (void**)&ptrTexCoordBuffer, D3DLOCK_DISCARD);
#ifdef _DEBUG
			SPK_ASSERT(hr == S_OK, "DX9Buffer::lock(unsigned int) - texCoord buffer can't be locked");
#endif
		}
		if( lock & INDEX_LOCK && nbIndices > 0 )
		{
#ifdef _DEBUG
			hr =
#endif
			indexBuffer->Lock(0, 0, (void**)&ptrIndexBuffer, D3DLOCK_DISCARD);
#ifdef _DEBUG
			SPK_ASSERT(hr == S_OK, "DX9Buffer::lock(unsigned int) - index buffer can't be locked");
#endif
		}
	}

	inline void DX9Buffer::unlock()
	{
		if( currentLock & VERTEX_LOCK ) vertexBuffer->Unlock();
		if( currentLock & COLOR_LOCK ) colorBuffer->Unlock();
		if( currentLock & TEXCOORD_LOCK && nbTexCoords > 0 ) texCoordBuffer->Unlock();
		if( currentLock & INDEX_LOCK && nbIndices > 0 ) indexBuffer->Unlock();
		currentLock = NO_LOCK;

#ifdef _DEBUG
		ptrVertexBuffer   = NULL;
		ptrColorBuffer    = NULL;
		ptrTexCoordBuffer = NULL;
		ptrIndexBuffer    = NULL;

		positionAtStart();
#endif // _DEBUG
	}

	inline void DX9Buffer::copy(unsigned int dst, void *src, size_t size)
	{
		if( dst == VERTEX_LOCK )	{ std::memcpy(ptrVertexBuffer, src, size); return; }
		if( dst == COLOR_LOCK )		{ std::memcpy(ptrColorBuffer, src, size); return; }
		if( dst == TEXCOORD_LOCK )	{ std::memcpy(ptrTexCoordBuffer, src, size); return; }
		if( dst == INDEX_LOCK )		{ std::memcpy(ptrIndexBuffer, src, size); return; }

		SPK_LOG_WARNING("DX9Buffer::copy(unsigned int dst, void *src, size_t size) - no valid destination");
	}

	inline void DX9Buffer::positionAtStart()
	{
		currentVertexIndex		= 0;
		currentColorIndex		= 0;
		currentTexCoordIndex	= 0;
	}

	inline void DX9Buffer::setNextVertex(const Vector3D& vertex)
	{
		Assign(ptrVertexBuffer[currentVertexIndex++], vertex);
	}

	inline void DX9Buffer::setNextColor(const Color& color)
	{
		ptrColorBuffer[currentColorIndex++] = color.getARGB();
	}

	inline void DX9Buffer::setNextTexCoord(float texCoord)
	{
		ptrTexCoordBuffer[currentTexCoordIndex++] = texCoord;
	}

	inline void DX9Buffer::skipNextTexCoords(size_t nb)
	{
		currentTexCoordIndex += nb;
	}

	inline size_t DX9Buffer::getNbTexCoords()
	{
		return nbTexCoords;
	}
}}

#endif
