//---------------------------------------------------------------------------
#ifndef templateH
#define templateH

#include "map.h"
#include "objset.h"
#include "tileset.h"
#include "utilites.h"
#include <vector>
#include "mdi.h"

struct TILES;
struct MapChange;
class CMap;
class CTileSet;
class CObjSet;
class CHeader;

#define MAX_LM   0x05

enum ChangeType {LM_NONE = 0, LM_FLOOR, LM_ROOF, LM_OBJ};
enum RestoreType {RT_BACK = 0, RT_FORWARD = 1};

extern CLog        * pLog;
extern CUtilites   * pUtil;


struct MapChange
{
private:
    ChangeType    type;             // Тип изменения.
    CMap          *pMap;            // Карта, с которой связано изменение.

    CHeader       *pHeader;
    CTileSet      *pTileSet;        // Floor и Roof
    CObjSet       *pObjSet;         // ObjSet

    void ChangeFloor(void);
    void RestoreFloor(void);
    void ChangeRoof(void);
    void RestoreRoof(void);
    void ChangeObjSet(void);
    void RestoreObjSet(void);

public:
    String      ChangeHint;
    
    MapChange(CMap * map);
    void SetChange(ChangeType change);
    void Restore(void);
    ~MapChange(void);
};


class CMap
{
private:
    CHeader     * pHeader;
    CTileSet    * pTileSet[3];
    CObjSet     * pObjSet[3];
    BYTE          level;

    int           pos;
    int           count;
    MapChange   * Changes[MAX_LM];
    String      DirName;                // Директория
    String      FullFileName;           // Полное
    String      FileName;               // Имя файла
    String      Name;                   // Имя файла без расширения
    String      Ext;                    // расширение файла
public:
    int         WorldX;
    int         WorldY;
    bool        SaveState;

    int     GetPos(void)    const {return pos;};
    int     GetCount(void)  const {return count;};

    CMap();
    ~CMap();
    bool    bError;
    void    CreateNewMap(WORD width, WORD height);
    bool    LoadFromFile(String FileName);
    void    SaveToFile(String FileName);

    void    SetFullFileName(String name);
    String  GetFileName(void)       const {return FileName; };
    String  GetFullFileName(void)   const {return FullFileName;};
    String  GetDirName(void)        const {return DirName;};
    String  GetChangeHint(void)     const {if (pos == -1) return ""; else return Changes[pos]->ChangeHint;};

    DWORD   GetWidth(void)          const {return pHeader->GetWidth();};
    DWORD   GetHeight(void)         const {return pHeader->GetHeight();};

    CHeader    * GetHeader(void)    const {return pHeader;};
    CTileSet   * GetTileSet(void)   const {return pTileSet[level];};
    CObjSet    * GetObjSet(void)    const {return pObjSet[level];};

    void    SaveChange(ChangeType  ml);
    void    RestoreChange(RestoreType type);
};


//---------------------------------------------------------------------------
#endif
