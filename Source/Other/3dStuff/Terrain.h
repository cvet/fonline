#ifndef __TERRAIN__
#define __TERRAIN__

class TERRAIN_GRID;

class Terrain
{
public:
	Terrain(LPDIRECT3DDEVICE9 device, int hx, int hy, const char* terrain_name);
	bool Draw(LPDIRECT3DSURFACE surf_rt, LPDIRECT3DSURFACE surf_ds, float x, float y, float zoom);
	void PreRestore();
	void PostRestore();
	bool IsError(){return isError;}
	int GetHexX(){return hexX;}
	int GetHexY(){return hexY;}

private:
	bool isError;
	LPDIRECT3DDEVICE9 d3dDevice;
	TERRAIN_GRID* terrainGrid;
	int hexX, hexY;

	// Wrapped functions
	static void* FileOpen(const char* fname, int folder);
	static void FileClose(void*& file);
	static size_t FileRead(void* file, void* buffer, size_t size);
	static size_t FileGetSize(void* file);
	static int64 FileGetTimeWrite(void* file);
	static bool SaveData(const char* fname, int folder, void* buffer, size_t size);
	static void Log(const char* msg);
};

#endif // __TERRAIN__