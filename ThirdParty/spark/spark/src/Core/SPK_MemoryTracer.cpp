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

#include "Core/SPK_DEF.h"


#ifdef SPK_OVERRIDE_NEW
#if _win32
    #define ALIGN16_MALLOC(__data, __size)      __data = _aligned_malloc(__size, 16)
    #define ALIGN16_FREE(__data)                _aligned_free(__data)
#else
    #define ALIGN16_MALLOC(__data, __size)      posix_memalign((void**)&__data,16,__size)
    #define ALIGN16_FREE(__data)                free(__data)
#endif
void * operator new(size_t size)
{
    void * p;
    ALIGN16_MALLOC(p, size);
    if (p == NULL) throw std::bad_alloc();
    return p;
}

void operator delete(void *p)
{
    ALIGN16_FREE(p);
}

void * operator new[](size_t size)
{
    void * p;
    ALIGN16_MALLOC(p, size);
    if (p == NULL) throw std::bad_alloc();
    return p;
}

void operator delete[](void *p)
{
    ALIGN16_FREE(p);
}
#endif
