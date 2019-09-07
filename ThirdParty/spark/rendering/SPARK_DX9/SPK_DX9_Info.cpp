//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2009-2011 - foulon matthieu - stardeath@wanadoo.fr				//
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

#include "Rendering/DX9/SPK_DX9_Info.h"

namespace SPK
{
namespace DX9
{
	LPDIRECT3DDEVICE9 DX9Info::device = NULL;
	LPDIRECT3DVERTEXDECLARATION9 DX9Info::vertexDeclaration[] = {NULL, NULL, NULL, NULL};

	LPDIRECT3DDEVICE9 DX9Info::getDevice()
	{
		return DX9Info::device;
	}

	LPDIRECT3DVERTEXDECLARATION9 DX9Info::getDecl(unsigned int nbTexCoords)
	{
		return vertexDeclaration[nbTexCoords];
	}

	void DX9Info::setDevice(LPDIRECT3DDEVICE9 device)
	{
		DX9Info::device = device;

		for( unsigned int i = 0; i < 4; ++i )
			device->CreateVertexDeclaration(Decl[i], &vertexDeclaration[i]);
	}

	void DX9Info::ReleaseResourcesOnDeviceFailure()
	{
		for( unsigned int i = 0; i < 4; ++i )
			SAFE_RELEASE( vertexDeclaration[i] );
	}
}}
