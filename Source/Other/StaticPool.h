#ifndef __STATIC_POOL__H__
#define __STATIC_POOL__H__

/********************************************************************
	created:	13:12:2008   18:40;
	author:		Anton Tsvetinskiy aka Cvet
	purpose:	
*********************************************************************/

#include <Log.h>

template<class Type, int Size>
class StaticPool
{
public:
	StaticPool()
	{
		allocSize=0;
		for(int i=0;i<Size;i++) validData[i]=false;
	}

	Type* Alloc()
	{
		for(int i=0;i<Size;i++)
		{
			if(!validData[i])
			{
				validData[i]=true;
				allocSize++;
				Type* ptr=(Item*)(staticData+sizeof(Type)*i);
				//ptr->Type::;
				return ptr;
			}
		}

		WriteLog(__FUNCTION__" - Not enaugh pool memory.\n");
		return NULL;
	}

	void Free(Type* data)
	{
		if(((BYTE*)data-staticData)%sizeof(Type))
		{
			WriteLog(__FUNCTION__" - Bad divide.\n");
			return;
		}

		int i=((BYTE*)data-staticData)/sizeof(Type);
		if(i<0)
		{
			WriteLog(__FUNCTION__" - Index is negative.\n");
			return;
		}
		if(i>=Size)
		{
			WriteLog(__FUNCTION__" - Index is greather than pool size.\n");
			return;
		}

		if(validData[i])
		{
			validData[i]=false;
			allocSize--;
		}
		else
		{
			WriteLog(__FUNCTION__" - Data already free.\n");
		}
	}

	int GetSize()
	{
		return allocSize;
	}

private:
	int allocSize;
	bool validData[Size];
	BYTE staticData[Size*sizeof(Type)];
};

#endif //__STATIC_POOL__H__