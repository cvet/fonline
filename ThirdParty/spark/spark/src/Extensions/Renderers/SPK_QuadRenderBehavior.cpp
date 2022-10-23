//
// SPARK particle engine
//
// Copyright (C) 2008-2011 - Julien Fryer - julienfryer@gmail.com
// Copyright (C) 2017 - Frederic Martin - fredakilla@gmail.com
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <SPARK_Core.h>
#include "Extensions/Renderers/SPK_QuadRenderBehavior.h"

namespace SPK
{
	QuadRenderBehavior::QuadRenderBehavior(float scaleX,float scaleY) :
        scaleX(scaleX),
		scaleY(scaleY),		
		textureAtlasNbX(1),
		textureAtlasNbY(1),
		textureAtlasW(1.0f),
		textureAtlasH(1.0f)
	{}

	void QuadRenderBehavior::setAtlasDimensions(size_t nbX,size_t nbY)
	{
		textureAtlasNbX = nbX;
		textureAtlasNbY = nbY;
		textureAtlasW = 1.0f / nbX;
		textureAtlasH = 1.0f / nbY;
	}
}
