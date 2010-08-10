#ifndef __DATA_MASK__
#define __DATA_MASK__

/*class CBitMask
{
public:
	void SetBit(DWORD x, DWORD y)
	{
		if(x>=width || y>=height) return;
		data[y*width_dw+x/32]|=1<<(x%32);
	}

	int GetBit(DWORD x, DWORD y)
	{
		if(x>=width || y>=height) return 0;
		return (data[y*width_dw+x/32]>>(x%32))&1;
	}

	void Fill(int fill)
	{
		memset(data,fill,width_dw*height*sizeof(DWORD));
	}

	CBitMask(DWORD width_bit, DWORD height_bit, DWORD* ptr, int fill)
	{
		if(!width_bit) width_bit=1;
		if(!height_bit) height_bit=1;
		width=width_bit;
		height=height_bit;
		width_dw=width/32;
		if(width%32) width_dw++;
		if(ptr)
		{
			isAlloc=false;
			data=ptr;
		}
		else
		{
			isAlloc=true;
			data=new DWORD[width_dw*height];
		}
		Fill(fill);
	}

	~CBitMask()
	{
		if(isAlloc) delete[] data;
		data=NULL;
	}

private:
	bool isAlloc;
	DWORD* data;
	DWORD width;
	DWORD height;
	DWORD width_dw;
};*/

class C2BitMask
{
public:
	void Set2Bit(DWORD x, DWORD y, int val)
	{
		if(x>=width || y>=height) return;
		BYTE& b=data[y*width_b+x/4];
		int bit=(x%4*2);
		UNSETFLAG(b,3<<bit);
		SETFLAG(b,(val&3)<<bit);
	}

	int Get2Bit(DWORD x, DWORD y)
	{
		if(x>=width || y>=height) return 0;
		return (data[y*width_b+x/4]>>(x%4*2))&3;
	}

	void Fill(int fill)
	{
		memset(data,fill,width_b*height);
	}

	void Create(DWORD width_2bit, DWORD height_2bit, BYTE* ptr)
	{
		if(!width_2bit) width_2bit=1;
		if(!height_2bit) height_2bit=1;
		width=width_2bit;
		height=height_2bit;
		width_b=width/4;
		if(width%4) width_b++;
		if(ptr)
		{
			isAlloc=false;
			data=ptr;
		}
		else
		{
			isAlloc=true;
			data=new BYTE[width_b*height];
			Fill(0);
		}
	}

	BYTE* GetData(){return data;}
	C2BitMask(){memset(this,0,sizeof(C2BitMask));}
	C2BitMask(DWORD width_2bit, DWORD height_2bit, BYTE* ptr){Create(width_2bit,height_2bit,ptr);}
	~C2BitMask(){if(isAlloc) delete[] data;	data=NULL;}

private:
	bool isAlloc;
	BYTE* data;
	DWORD width;
	DWORD height;
	DWORD width_b;
};

class C4BitMask
{
public:
	void Set4Bit(DWORD x, DWORD y, BYTE val)
	{
		if(x>=width || y>=height) return;
		BYTE& b=data[y*width_b+x/2];
		if(x&1) b=(b&0xF0)|(val&0xF);
		else b=(b&0xF)|(val<<4);
	}

	BYTE Get4Bit(DWORD x, DWORD y)
	{
		if(x>=width || y>=height) return 0;
		BYTE& b=data[y*width_b+x/2];
		if(x&1) return b&0xF;
		else return b>>4;
	}

	void Fill(int fill)
	{
		memset(data,fill,width_b*height);
	}

	C4BitMask(DWORD width_4bit, DWORD height_4bit, int fill)
	{
		if(!width_4bit) width_4bit=1;
		if(!height_4bit) height_4bit=1;
		width=width_4bit;
		height=height_4bit;
		width_b=width/2;
		if(width&1) width_b++;
		data=new BYTE[width_b*height];
		Fill(fill);
	}

	~C4BitMask()
	{
		delete[] data;
	}

private:
	BYTE* data;
	DWORD width;
	DWORD height;
	DWORD width_b;
};

class CByteMask
{
public:
	void SetByte(DWORD x, DWORD y, BYTE val)
	{
		if(x>=width || y>=height) return;
		data[y*width+x]=val;
	}

	BYTE GetByte(DWORD x, DWORD y)
	{
		if(x>=width || y>=height) return 0;
		return data[y*width+x];
	}

	void Fill(int fill)
	{
		memset(data,fill,width*height);
	}

	CByteMask(DWORD _width, DWORD _height, int fill)
	{
		if(!_width) _width=1;
		if(!_height) _height=1;
		width=_width;
		height=_height;
		data=new BYTE[width*height];
		Fill(fill);
	}

	~CByteMask()
	{
		delete[] data;
	}

private:
	BYTE* data;
	DWORD width;
	DWORD height;
};


/*template<int Size>
class CBitMask_
{
public:
	void SetBit(DWORD x, DWORD y, int val)
	{
		if(x>=width || y>=height) return;
		data[y*width_dw+x/32]|=1<<(x%32);
	}

	int GetBit(DWORD x, DWORD y)
	{
		if(x>=width || y>=height) return 0;
		return (data[y*width_dw+x/32]>>(x%32))&1;
	}

	void Fill(int fill)
	{
		memset(data,fill,width_dw*height*sizeof(DWORD));
	}

	CBitMask(DWORD width_bit, DWORD height_bit, int fill)
	{
		width=width_bit*Size;
		height=height_bit*Size;

		width_dw=width/32;
		if(width%32) width_dw++;

		data=new DWORD[width_dw*height];

		Fill(fill);
	}

	~CBitMask()
	{
		delete[] data;
	}

private:
	DWORD* data;
	DWORD width;
	DWORD height;
	DWORD width_dw;
};*/

#endif // __DATA_MASK__