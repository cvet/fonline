#ifndef __PROTO_MAP__
#define __PROTO_MAP__

#include "Common.h"
#include "Entity.h"
#include "MsgFiles.h"

#ifdef FONLINE_SERVER
# include "Critter.h"
# include "Item.h"
#endif

class ProtoMap: public ProtoEntity
{
public:
    CLASS_PROPERTY_ALIAS( string, FileDir );
    CLASS_PROPERTY_ALIAS( ushort, Width );
    CLASS_PROPERTY_ALIAS( ushort, Height );
    CLASS_PROPERTY_ALIAS( ushort, WorkHexX );
    CLASS_PROPERTY_ALIAS( ushort, WorkHexY );
    CLASS_PROPERTY_ALIAS( int, CurDayTime );
    CLASS_PROPERTY_ALIAS( hash, ScriptId );
    CLASS_PROPERTY_ALIAS( CScriptArray *, DayTime );    // 4 int
    CLASS_PROPERTY_ALIAS( CScriptArray *, DayColor );   // 12 uchar
    CLASS_PROPERTY_ALIAS( bool, IsNoLogOut );

    // Entities
    #ifdef FONLINE_EDITOR
    EntityVec AllEntities;
    uint      LastEntityId;
    #endif

    // Tiles
    struct Tile     // 16 bytes
    {
        hash   Name;
        ushort HexX, HexY;
        short  OffsX, OffsY;
        uchar  Layer;
        bool   IsRoof;
        #ifdef FONLINE_EDITOR
        bool   IsSelected;
        #endif

        Tile() { memzero( this, sizeof( Tile ) ); }
        Tile( hash name, ushort hx, ushort hy, char ox, char oy, uchar layer, bool is_roof ): Name( name ), HexX( hx ), HexY( hy ), OffsX( ox ), OffsY( oy ), Layer( layer ), IsRoof( is_roof ) {}
    };
    typedef vector< Tile > TileVec;
    TileVec Tiles;

private:
    #ifdef FONLINE_EDITOR
    void SaveTextFormat( IniParser& file );
    #endif
    bool LoadTextFormat( const char* buf );
    bool LoadOldTextFormat( const char* buf );
    bool OnAfterLoad( EntityVec& entities );

    #ifdef FONLINE_SERVER
public:
    UCharVec SceneryData;
    hash     HashTiles;
    hash     HashScen;

    CrVec    CrittersVec;
    ItemVec  HexItemsVec;
    ItemVec  ChildItemsVec;
    ItemVec  StaticItemsVec;
    ItemVec  TriggerItemsVec;
    uchar*   HexFlags;

private:
    bool BindScripts( EntityVec& entities );
    #endif

public:
    bool Load();

    #ifdef FONLINE_EDITOR
    void        GenNew();
    bool        Save( const string& custom_name );
    static bool IsMapFile( const string& fname );
    #endif

    #ifdef FONLINE_SERVER
    void  GetStaticItemTriggers( ushort hx, ushort hy, ItemVec& triggers );
    Item* GetStaticItem( ushort hx, ushort hy, hash pid );
    void  GetStaticItemsHex( ushort hx, ushort hy, ItemVec& items );
    void  GetStaticItemsHexEx( ushort hx, ushort hy, uint radius, hash pid, ItemVec& items );
    void  GetStaticItemsByPid( hash pid, ItemVec& items );
    #endif

    UIntVec  TextsLang;
    FOMsgVec Texts;

    ProtoMap( hash pid );
    ~ProtoMap();
};
typedef vector< ProtoMap* >    ProtoMapVec;
typedef map< hash, ProtoMap* > ProtoMapMap;

class ProtoLocation: public ProtoEntity
{
public:
    CLASS_PROPERTY_ALIAS( CScriptArray *, MapProtos );

    ProtoLocation( hash pid );

    UIntVec  TextsLang;
    FOMsgVec Texts;
};
typedef vector< ProtoLocation* >    ProtoLocVec;
typedef map< hash, ProtoLocation* > ProtoLocMap;

#endif // __PROTO_MAP__
