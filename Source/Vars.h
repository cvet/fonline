#ifndef __VARS__
#define __VARS__

#include "Common.h"

#define VAR_NAME_LEN          ( 256 )
#define VAR_DESC_LEN          ( 2048 )
#define VAR_FNAME_VARS        "_vars.fos"
#define VAR_DESC_MARK         "**********"


// Types
#define VAR_GLOBAL            ( 0 )
#define VAR_LOCAL             ( 1 )
#define VAR_UNICUM            ( 2 )
#define VAR_LOCAL_LOCATION    ( 3 )
#define VAR_LOCAL_MAP         ( 4 )
#define VAR_LOCAL_ITEM        ( 5 )

// Flags
#define VAR_FLAG_RANDOM       ( 0x2 )
#define VAR_FLAG_NO_CHECK     ( 0x4 )

// Typedefs
class TemplateVar;
class GameVar;
typedef vector< TemplateVar* >  TempVarVec;
typedef map< uint, GameVar* >   VarsMap32;
typedef map< uint64, GameVar* > VarsMap64;
typedef vector< GameVar* >      VarsVec;


class TemplateVar
{
public:
    int       Type;
    ushort    TempId;
    string    Name;
    string    Desc;
    int       StartVal;
    int       MinVal;
    int       MaxVal;
    uint      Flags;

    VarsMap32 Vars;
    VarsMap64 VarsUnicum;

    bool IsNotUnicum() { return Type != VAR_UNICUM; }
    bool IsError()     { return !TempId || !Name.size() || ( IsNoBorders() && ( MinVal > MaxVal || StartVal < MinVal || StartVal > MaxVal ) ); }
    bool IsRandom()    { return FLAG( Flags, VAR_FLAG_RANDOM ); }
    bool IsNoBorders() { return FLAG( Flags, VAR_FLAG_NO_CHECK ); }
    TemplateVar(): Type( VAR_GLOBAL ), TempId( 0 ), StartVal( 0 ), MinVal( 0 ), MaxVal( 0 ), Flags( 0 ) {}
};

#ifdef FONLINE_SERVER
class Critter;

class GameVar
{
public:
    uint MasterId;
    uint SlaveId;
    int VarValue;
    TemplateVar* VarTemplate;
    ushort Type;
    short RefCount;
    SyncObject Sync;

    GameVar& operator+=( const int _right );
    GameVar& operator-=( const int _right );
    GameVar& operator*=( const int _right );
    GameVar& operator/=( const int _right );
    GameVar& operator=( const int _right );
    GameVar& operator+=( const GameVar& _right );
    GameVar& operator-=( const GameVar& _right );
    GameVar& operator*=( const GameVar& _right );
    GameVar& operator/=( const GameVar& _right );
    GameVar& operator=( const GameVar& _right );
    int      operator+( const int _right )             { return VarValue + _right; }
    int      operator-( const int _right )             { return VarValue - _right; }
    int      operator*( const int _right )             { return VarValue * _right; }
    int      operator/( const int _right )             { return VarValue / _right; }
    int      operator+( const GameVar& _right )        { return VarValue + _right.VarValue; }
    int      operator-( const GameVar& _right )        { return VarValue - _right.VarValue; }
    int      operator*( const GameVar& _right )        { return VarValue * _right.VarValue; }
    int      operator/( const GameVar& _right )        { return VarValue / _right.VarValue; }
    bool     operator>( const int _right ) const       { return VarValue > _right; }
    bool     operator<( const int _right ) const       { return VarValue < _right; }
    bool     operator==( const int _right ) const      { return VarValue == _right; }
    bool     operator>=( const int _right ) const      { return VarValue >= _right; }
    bool     operator<=( const int _right ) const      { return VarValue <= _right; }
    bool     operator!=( const int _right ) const      { return VarValue != _right; }
    bool     operator>( const GameVar& _right ) const  { return VarValue > _right.VarValue; }
    bool     operator<( const GameVar& _right ) const  { return VarValue < _right.VarValue; }
    bool     operator==( const GameVar& _right ) const { return VarValue == _right.VarValue; }
    bool     operator>=( const GameVar& _right ) const { return VarValue >= _right.VarValue; }
    bool     operator<=( const GameVar& _right ) const { return VarValue <= _right.VarValue; }
    bool     operator!=( const GameVar& _right ) const { return VarValue != _right.VarValue; }

    int          GetValue()       { return VarValue; }
    int          GetMin()         { return VarTemplate->MinVal; }
    int          GetMax()         { return VarTemplate->MaxVal; }
    bool         IsRandom()       { return VarTemplate->IsRandom(); }
    TemplateVar* GetTemplateVar() { return VarTemplate; }
    uint64       GetUid()         { return ( ( (uint64) SlaveId ) << 32 ) | ( (uint64) MasterId ); }
    uint         GetMasterId()    { return MasterId; }
    uint         GetSlaveId()     { return SlaveId; }

    void AddRef()  { RefCount++; }
    void Release() { if( !--RefCount ) delete this; }

    GameVar( uint master_id, uint slave_id, TemplateVar* var_template, int val ): MasterId( master_id ), SlaveId( slave_id ), VarTemplate( var_template ),
                                                                                  Type( var_template->Type ), VarValue( val ), RefCount( 1 ) { MEMORY_PROCESS( MEMORY_VAR, sizeof( GameVar ) ); }
    ~GameVar() { MEMORY_PROCESS( MEMORY_VAR, -(int) sizeof( GameVar ) ); }
private: GameVar() {}
};

// Wrapper
int  GameVarAddInt( GameVar& var, const int _right );
int  GameVarSubInt( GameVar& var, const int _right );
int  GameVarMulInt( GameVar& var, const int _right );
int  GameVarDivInt( GameVar& var, const int _right );
int  GameVarAddGameVar( GameVar& var, GameVar& _right );
int  GameVarSubGameVar( GameVar& var, GameVar& _right );
int  GameVarMulGameVar( GameVar& var, GameVar& _right );
int  GameVarDivGameVar( GameVar& var, GameVar& _right );
bool GameVarEqualInt( const GameVar& var, const int _right );
int  GameVarCmpInt( const GameVar& var, const int _right );
bool GameVarEqualGameVar( const GameVar& var, const GameVar& _right );
int  GameVarCmpGameVar( const GameVar& var, const GameVar& _right );
#endif // FONLINE_SERVER

class VarManager
{
private:
    TempVarVec   tempVars;
    StrUShortMap varsNames;
    Mutex        varsLocker;

    bool LoadTemplateVars( const char* str, TempVarVec& vars );   // Return count error

public:
    bool Init();
    void Finish();
    void Clear();

    bool         UpdateVarsTemplate();
    bool         AddTemplateVar( TemplateVar* var );
    void         EraseTemplateVar( ushort temp_id );
    TemplateVar* GetTemplateVar( ushort temp_id );
    ushort       GetTemplateVarId( const char* var_name );
    bool         IsTemplateVarAviable( const char* var_name );
    void         SaveTemplateVars();
    TempVarVec&  GetTemplateVars() { return tempVars; }

    #ifdef FONLINE_SERVER
public:
    void     SaveVarsDataFile( void ( * save_func )( void*, size_t ) );
    bool     LoadVarsDataFile( void* f, int version );
    bool     CheckVar( const char* var_name, uint master_id, uint slave_id, char oper, int val );
    bool     CheckVar( ushort temp_id, uint master_id, uint slave_id, char oper, int val );
    GameVar* ChangeVar( const char* var_name, uint master_id, uint slave_id, char oper, int val );
    GameVar* ChangeVar( ushort temp_id, uint master_id, uint slave_id, char oper, int val );
    GameVar* GetVar( const char* var_name, uint master_id, uint slave_id,  bool create );
    GameVar* GetVar( ushort temp_id, uint master_id, uint slave_id,  bool create );
    void     SwapVars( uint id1, uint id2 );
    uint     ClearUnusedVars( UIntSet& ids1, UIntSet& ids2, UIntSet& ids_locs, UIntSet& ids_maps, UIntSet& ids_items );
    uint     GetVarsCount() { return varsCount; }

private:
    uint varsCount;

    bool     CheckVar( GameVar* var, char oper, int val );
    void     ChangeVar( GameVar* var, char oper, int val );
    GameVar* CreateVar( uint master_id, TemplateVar* tvar );
    GameVar* CreateVarUnicum( uint64 id, uint master_id, uint slave_id, TemplateVar* tvar );
    #endif // FONLINE_SERVER
};

extern VarManager VarMngr;

#endif // __VARS__
