#ifndef __DEBUGGER__
#define __DEBUGGER__

#ifdef FONLINE_SERVER
# define MEMORY_DEBUG
#endif

#ifdef MEMORY_DEBUG
# define MEMORY_PROCESS( block, memory ) \
    if( MemoryDebugLevel >= 1 )          \
        Debugger::Memory( block, memory )
# define MEMORY_PROCESS_STR( block, memory ) \
    if( MemoryDebugLevel >= 2 )              \
        Debugger::MemoryStr( block, memory )
#else
# define MEMORY_PROCESS( block, memory )
# define MEMORY_PROCESS_STR( block, memory )
#endif

#define MEMORY_STATIC           ( 0 )
#define MEMORY_NPC              ( 1 )
#define MEMORY_CLIENT           ( 2 )
#define MEMORY_MAP              ( 3 )
#define MEMORY_MAP_FIELD        ( 4 )
#define MEMORY_PROTO_MAP        ( 5 )
#define MEMORY_NET_BUFFER       ( 6 )
#define MEMORY_VAR              ( 7 )
#define MEMORY_NPC_PLANE        ( 8 )
#define MEMORY_ITEM             ( 9 )
#define MEMORY_DIALOG           ( 10 )
#define MEMORY_SAVE_DATA        ( 11 )
#define MEMORY_ANY_DATA         ( 12 )
#define MEMORY_IMAGE            ( 13 )
#define MEMORY_SCRIPT_STRING    ( 14 )
#define MEMORY_SCRIPT_ARRAY     ( 15 )
#define MEMORY_SCRIPT_DICT      ( 16 )
#define MEMORY_ANGEL_SCRIPT     ( 17 )

namespace Debugger
{
    void BeginCycle();
    void BeginBlock( int num_block );
    void ProcessBlock( int num_block, int identifier );
    void EndCycle( double lag_to_show );
    void ShowLags( int num_block, double lag_to_show );

    void        Memory( int block, int value );
    void        MemoryStr( const char* block, int value );
    const char* GetMemoryStatistics();

    void   StartTraceMemory();
    string GetTraceMemory();
};

#endif // __DEBUGGER__
