#include "StdAfx.h"
#include "Debugger.h"
#include "Mutex.h"

#define MAX_BLOCKS       ( 25 )
#define MAX_ENTRY        ( 2000 )
#define MAX_PROCESS      ( 20 )
static double Ticks[ MAX_BLOCKS ][ MAX_ENTRY ][ MAX_PROCESS ];
static int    Identifiers[ MAX_BLOCKS ][ MAX_ENTRY ][ MAX_PROCESS ];
static uint   CurTick[ MAX_BLOCKS ][ MAX_ENTRY ];
static int    CurEntry[ MAX_BLOCKS ];

void Debugger::BeginCycle()
{
    memzero( Identifiers, sizeof( Identifiers ) );
    for( int i = 0; i < MAX_BLOCKS; i++ )
        CurEntry[ i ] = -1;
}

void Debugger::BeginBlock( int num_block )
{
    if( CurEntry[ num_block ] + 1 >= MAX_ENTRY )
        return;
    CurEntry[ num_block ]++;
    CurTick[ num_block ][ CurEntry[ num_block ] ] = 0;
    Ticks[ num_block ][ CurEntry[ num_block ] ][ 0 ] = Timer::AccurateTick();
}

void Debugger::ProcessBlock( int num_block, int identifier )
{
    if( CurEntry[ num_block ] + 1 >= MAX_ENTRY )
        return;
    CurTick[ num_block ][ CurEntry[ num_block ] ]++;
    Ticks[ num_block ][ CurEntry[ num_block ] ][ CurTick[ num_block ][ CurEntry[ num_block ] ] ] = Timer::AccurateTick();
    Identifiers[ num_block ][ CurEntry[ num_block ] ][ CurTick[ num_block ][ CurEntry[ num_block ] ] ] = identifier;
}

void Debugger::EndCycle( double lag_to_show )
{
    for( int num_block = 0; num_block < MAX_BLOCKS; num_block++ )
    {
        for( int entry = 0; entry <= CurEntry[ num_block ]; entry++ )
        {
            bool is_first = true;
            for( int i = 1, j = CurTick[ num_block ][ entry ]; i <= j; i++ )
            {
                double diff = Ticks[ num_block ][ entry ][ i ] - Ticks[ num_block ][ entry ][ i - 1 ];
                if( diff >= lag_to_show )
                {
                    if( is_first )
                    {
                        WriteLog( "Lag in block %d...", num_block );
                        is_first = false;
                    }
                    WriteLog( "<%d,%g>", Identifiers[ num_block ][ entry ][ i ], diff );
                }
            }
            if( !is_first )
                WriteLog( "\n" );
        }
    }
}

void Debugger::ShowLags( int num_block, double lag_to_show )
{
    for( int entry = 0; entry <= CurEntry[ num_block ]; entry++ )
    {
        double diff = Ticks[ num_block ][ entry ][ CurTick[ num_block ][ entry ] ] - Ticks[ num_block ][ entry ][ 0 ];
        if( diff >= lag_to_show )
        {
            WriteLog( "Lag in block %d...", num_block );
            for( int i = 1, j = CurTick[ num_block ][ entry ]; i <= j; i++ )
            {
                double diff_ = Ticks[ num_block ][ entry ][ i ] - Ticks[ num_block ][ entry ][ i - 1 ];
                WriteLog( "<%d,%g>", Identifiers[ num_block ][ entry ][ i ], diff_ );
            }
            WriteLog( "\n" );
        }
    }
}

#define MAX_MEM_NODES    ( 16 )
struct MemNode
{
    int64 AllocMem;
    int64 DeallocMem;
    int64 MinAlloc;
    int64 MaxAlloc;
} static MemNodes[ MAX_MEM_NODES ] = { 0, 0, 0, 0 };

const char* MemBlockNames[ MAX_MEM_NODES ] =
{
    "Static       ",
    "Npc          ",
    "Clients      ",
    "Maps         ",
    "Map fields   ",
    "Proto maps   ",
    "Net buffers  ",
    "Vars         ",
    "Npc planes   ",
    "Items        ",
    "Dialogs      ",
    "Save data    ",
    "Any data     ",
    "Images       ",
    "Script string",
    "Angel Script ",
};

static Mutex* MemLocker = NULL;

void Debugger::Memory( int block, int value )
{
    if( !MemLocker )
        MemLocker = new Mutex();
    SCOPE_LOCK( *MemLocker );

    if( value )
    {
        MemNode& node = MemNodes[ block ];
        if( value > 0 )
        {
            node.AllocMem += value;
            if( !node.MinAlloc || value < node.MinAlloc )
                node.MinAlloc = value;
            if( value > node.MaxAlloc )
                node.MaxAlloc = value;
        }
        else
        {
            node.DeallocMem -= value;
        }
    }
}

struct MemNodeStr
{
    char  Name[ 256 ];
    int64 AllocMem;
    int64 DeallocMem;
    int64 MinAlloc;
    int64 MaxAlloc;
    bool operator==( const char* str ) const { return Str::Compare( str, Name ); }
};
typedef vector< MemNodeStr > MemNodeStrVec;
static MemNodeStrVec MemNodesStr;

void Debugger::MemoryStr( const char* block, int value )
{
    if( !MemLocker )
        MemLocker = new Mutex();
    SCOPE_LOCK( *MemLocker );

    if( block && value )
    {
        MemNodeStr* node;
        auto        it = std::find( MemNodesStr.begin(), MemNodesStr.end(), block );
        if( it == MemNodesStr.end() )
        {
            MemNodeStr node_;
            Str::Copy( node_.Name, block );
            node_.AllocMem = 0;
            node_.DeallocMem = 0;
            node_.MinAlloc = 0;
            node_.MaxAlloc = 0;
            MemNodesStr.push_back( node_ );
            node = &MemNodesStr.back();
        }
        else
        {
            node = &( *it );
        }

        if( value > 0 )
        {
            node->AllocMem += value;
            if( !node->MinAlloc || value < node->MinAlloc )
                node->MinAlloc = value;
            if( value > node->MaxAlloc )
                node->MaxAlloc = value;
        }
        else
        {
            node->DeallocMem -= value;
        }
    }
}

const char* Debugger::GetMemoryStatistics()
{
    if( !MemLocker )
        MemLocker = new Mutex();
    SCOPE_LOCK( *MemLocker );

    static string result;
    result = "Memory statistics:\n";

    #ifdef FONLINE_SERVER
    char  buf[ 512 ];
    int64 all_alloc = 0, all_dealloc = 0;

    if( MemoryDebugLevel >= 1 )
    {
        result += "\n  Level 1            Memory        Alloc         Free    Min block    Max block\n";
        for( int i = 0; i < MAX_MEM_NODES; i++ )
        {
            MemNode& node = MemNodes[ i ];
            # ifdef FO_WINDOWS
            Str::Format( buf, "%s : %12I64d %12I64d %12I64d %12I64d %12I64d\n", MemBlockNames[ i ], node.AllocMem - node.DeallocMem, node.AllocMem, node.DeallocMem, node.MinAlloc, node.MaxAlloc );
            # else // FO_LINUX
            Str::Format( buf, "%s : %12lld %12lld %12lld %12lld %12lld\n", MemBlockNames[ i ], node.AllocMem - node.DeallocMem, node.AllocMem, node.DeallocMem, node.MinAlloc, node.MaxAlloc );
            # endif
            result += buf;
            all_alloc += node.AllocMem;
            all_dealloc += node.DeallocMem;
        }
        # ifdef FO_WINDOWS
        Str::Format( buf, "Whole memory  : %12I64d %12I64d %12I64d\n", all_alloc - all_dealloc, all_alloc, all_dealloc );
        # else // FO_LINUX
        Str::Format( buf, "Whole memory  : %12lld %12lld %12lld\n", all_alloc - all_dealloc, all_alloc, all_dealloc );
        # endif
        result += buf;
    }

    if( MemoryDebugLevel >= 2 )
    {
        all_alloc = all_dealloc = 0;
        result += "\n  Level 2                                                  Memory        Alloc         Free    Min block    Max block\n";
        for( auto it = MemNodesStr.begin(), end = MemNodesStr.end(); it != end; ++it )
        {
            MemNodeStr& node = *it;
            # ifdef FO_WINDOWS
            Str::Format( buf, "%-50s : %12I64d %12I64d %12I64d %12I64d %12I64d\n", node.Name, node.AllocMem - node.DeallocMem, node.AllocMem, node.DeallocMem, node.MinAlloc, node.MaxAlloc );
            # else // FO_LINUX
            Str::Format( buf, "%-50s : %12lld %12lld %12lld %12lld %12lld\n", node.Name, node.AllocMem - node.DeallocMem, node.AllocMem, node.DeallocMem, node.MinAlloc, node.MaxAlloc );
            # endif
            result += buf;
            all_alloc += node.AllocMem;
            all_dealloc += node.DeallocMem;
        }
        # ifdef FO_WINDOWS
        Str::Format( buf, "Whole memory                                       : %12I64d %12I64d %12I64d\n", all_alloc - all_dealloc, all_alloc, all_dealloc );
        # else // FO_LINUX
        Str::Format( buf, "Whole memory                                       : %12lld %12lld %12lld\n", all_alloc - all_dealloc, all_alloc, all_dealloc );
        # endif
        result += buf;
    }

    if( MemoryDebugLevel >= 3 )
    {
        result += "\n  Level 3\n";
        # ifdef TRACE_MEMORY
        result += GetTraceMemory();
        # else
        result += "Not available\n";
        # endif
    }

    if( MemoryDebugLevel <= 0 )
        result += "\n  Disabled\n";
    #endif

    return result.c_str();
}

// Memory tracing
#ifdef TRACE_MEMORY
# pragma warning( disable : 4748 )
# include <DbgHelp.h>
# pragma comment( lib, "Dbghelp.lib" )
# include "FileManager.h"
# undef malloc
# undef calloc
# undef realloc
# undef free

static bool                                  MemoryTrace = false;
static bool                                  MemoryTraceExt = false;
static map< size_t, pair< size_t, size_t > > MemoryBlocks;
static map< size_t, const char* >            StackWalkHashName;
static Mutex                                 MemoryAllocLocker;
static HANDLE                                ProcessHandle;

size_t GetStackInfo()
{
    void*  blocks[ 63 ];
    ULONG  hash;
    USHORT frames = CaptureStackBackTrace( 1, 60, (void**) blocks, &hash );

    auto   it = StackWalkHashName.find( (size_t) hash );
    if( it != StackWalkHashName.end() )
        return (size_t) hash;

    string str;
    str.reserve( 16384 );

    # define STACKWALK_MAX_NAMELEN    ( 2048 )
    char         symbol_buffer[ sizeof( SYMBOL_INFO ) + STACKWALK_MAX_NAMELEN ];
    SYMBOL_INFO* symbol = (SYMBOL_INFO*) symbol_buffer;
    memset( symbol, 0, sizeof( SYMBOL_INFO ) + STACKWALK_MAX_NAMELEN );
    symbol->SizeOfStruct = sizeof( SYMBOL_INFO );
    symbol->MaxNameLen = STACKWALK_MAX_NAMELEN;

    for( int frame_num = 0; frame_num < (int) frames - 1; frame_num++ )
    {
        DWORD64 offset = (DWORD64) blocks[ frame_num ];
        if( !offset )
            continue;

        struct
        {
            CHAR    name[ STACKWALK_MAX_NAMELEN ];
            CHAR    undName[ STACKWALK_MAX_NAMELEN ];
            CHAR    undFullName[ STACKWALK_MAX_NAMELEN ];
            DWORD64 offsetFromSmybol;
            DWORD   offsetFromLine;
            DWORD   lineNumber;
            CHAR    lineFileName[ STACKWALK_MAX_NAMELEN ];
            CHAR    moduleName[ STACKWALK_MAX_NAMELEN ];
            DWORD64 baseOfImage;
            CHAR    loadedImageName[ STACKWALK_MAX_NAMELEN ];
        } callstack;
        memzero( &callstack, sizeof( callstack ) );

        if( SymFromAddr( ProcessHandle, offset, &callstack.offsetFromSmybol, symbol ) )
        {
            strncpy_s( callstack.name, symbol->Name, STACKWALK_MAX_NAMELEN - 1 );
            callstack.name[ STACKWALK_MAX_NAMELEN - 1 ] = 0;
            UnDecorateSymbolName( symbol->Name, callstack.undName, STACKWALK_MAX_NAMELEN, UNDNAME_NAME_ONLY );
            UnDecorateSymbolName( symbol->Name, callstack.undFullName, STACKWALK_MAX_NAMELEN, UNDNAME_COMPLETE );
        }

        IMAGEHLP_LINE64 line;
        memset( &line, 0, sizeof( line ) );
        line.SizeOfStruct = sizeof( line );
        if( SymGetLineFromAddr64( ProcessHandle, offset, &callstack.offsetFromLine, &line ) )
        {
            callstack.lineNumber = line.LineNumber;
            strcpy( callstack.lineFileName, line.FileName );
            FileManager::ExtractFileName( callstack.lineFileName, callstack.lineFileName );
        }

        IMAGEHLP_MODULE64 module;
        memset( &module, 0, sizeof( module ) );
        module.SizeOfStruct = sizeof( module );
        if( SymGetModuleInfo64( ProcessHandle, offset, &module ) )
        {
            strcpy( callstack.moduleName, module.ModuleName );
            callstack.baseOfImage = module.BaseOfImage;
            strcpy( callstack.loadedImageName, module.LoadedImageName );
        }

        if( callstack.name[ 0 ] == 0 )
            strcpy( callstack.name, "(function-name not available)" );
        if( callstack.undName[ 0 ] != 0 )
            strcpy( callstack.name, callstack.undName );
        if( callstack.undFullName[ 0 ] != 0 )
            strcpy( callstack.name, callstack.undFullName );
        if( callstack.moduleName[ 0 ] == 0 )
            strcpy( callstack.moduleName, "???" );

        char buf[ 64 ];
        str += "\t";
        str += callstack.moduleName;
        str += ", ";
        str += callstack.name;
        str += " + ";
        str += _itoa( (int) callstack.offsetFromSmybol, buf, 10 );
        if( callstack.lineFileName[ 0 ] )
        {
            str += ", ";
            str += callstack.lineFileName;
            str += " (";
            str += _itoa( callstack.lineNumber, buf, 10 );
            str += ")\n";
        }
        else
        {
            str += "\n";
        }
    }
    str += "\n";

    StackWalkHashName.insert( PAIR( (size_t) hash, _strdup( str.c_str() ) ) );
    return (size_t) hash;
}

# define ALLOCATE_MEMORY                                                           \
    if( MemoryTrace && ptr )                                                       \
    {                                                                              \
        SCOPE_LOCK( MemoryAllocLocker );                                           \
        if( !MemoryTraceExt )                                                      \
        {                                                                          \
            MemoryTraceExt = true;                                                 \
            size_t stack_hash = GetStackInfo();                                    \
            MemoryBlocks.insert( PAIR( (size_t) ptr, PAIR( stack_hash, size ) ) ); \
            MemoryTraceExt = false;                                                \
        }                                                                          \
    }
# define DEALLOCATE_MEMORY                      \
    if( MemoryTrace && ptr )                    \
    {                                           \
        SCOPE_LOCK( MemoryAllocLocker );        \
        if( !MemoryTraceExt )                   \
        {                                       \
            MemoryTraceExt = true;              \
            MemoryBlocks.erase( (size_t) ptr ); \
            MemoryTraceExt = false;             \
        }                                       \
    }

void* operator new( size_t size )
{
    void* ptr = malloc( size );
    ALLOCATE_MEMORY;
    return ptr;
}

void* operator new[]( size_t size )
{
    void* ptr = malloc( size );
    ALLOCATE_MEMORY;
    return ptr;
}

void operator delete( void* ptr )
{
    DEALLOCATE_MEMORY;
    free( ptr );
}

void operator delete[]( void* ptr )
{
    DEALLOCATE_MEMORY;
    free( ptr );
}

void* Malloc( size_t size )
{
    void* ptr = malloc( size );
    ALLOCATE_MEMORY;
    return ptr;
}

void* Calloc( size_t count, size_t size )
{
    void* ptr = calloc( count, size );
    ALLOCATE_MEMORY;
    return ptr;
}

void* Realloc( void* ptr, size_t size )
{
    DEALLOCATE_MEMORY;
    ptr = realloc( ptr, size );
    ALLOCATE_MEMORY;
    return ptr;
}

void Free( void* ptr )
{
    DEALLOCATE_MEMORY;
    free( ptr );
}

void Debugger::StartTraceMemory()
{
    ProcessHandle = OpenProcess( PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId() );
    SymInitialize( ProcessHandle, NULL, TRUE );
    SymSetOptions( SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS );
    MemoryTrace = true;
}

string Debugger::GetTraceMemory()
{
    SCOPE_LOCK( MemoryAllocLocker );
    MemoryTraceExt = true;

    // RetAddr, size, chunks
    map< size_t, pair< size_t, size_t > > blocks;
    int64                                 whole_size = 0, whole_chunks = 0;
    for( auto it = MemoryBlocks.begin(), end = MemoryBlocks.end(); it != end; ++it )
    {
        size_t ptr = ( *it ).first;
        size_t stack_hash = ( *it ).second.first;
        size_t size = ( *it ).second.second;
        auto   it_block = blocks.find( stack_hash );
        if( it_block == blocks.end() )
            blocks.insert( PAIR( stack_hash, PAIR( size, 1 ) ) );
        else
            ( *it_block ).second.first += size, ( *it_block ).second.second++;
        whole_size += size;
        whole_chunks++;
    }

    // Sort by chunks count
    multimap< size_t, pair< size_t, size_t >, greater< size_t > > blocks_chunks;
    for( auto it = blocks.begin(), end = blocks.end(); it != end; ++it )
    {
        size_t stack_hash = ( *it ).first;
        size_t size = ( *it ).second.first;
        size_t chunks = ( *it ).second.second;
        blocks_chunks.insert( PAIR( chunks, PAIR( stack_hash, size ) ) );
    }

    // Print
    string str;
    str.reserve( 1000000 );
    char   buf[ MAX_FOTEXT ];
    str += "Unfreed memory:\n";
    sprintf( buf, "  Whole blocks %u\n", blocks.size() );
    str += buf;
    sprintf( buf, "  Whole size   %I64d\n", whole_size );
    str += buf;
    sprintf( buf, "  Whole chunks %I64d\n", whole_chunks );
    str += buf;
    str += "\n";
    uint cur = 0;
    for( auto it = blocks_chunks.begin(), end = blocks_chunks.end(); it != end; ++it )
    {
        size_t chunks = ( *it ).first;
        size_t stack_hash = ( *it ).second.first;
        size_t size = ( *it ).second.second;
        sprintf( buf, "%06u) Size %-10u Chunks %-10u\n", cur++, size, chunks );
        str += buf;
        str += StackWalkHashName[ stack_hash ];
    }

    MemoryTraceExt = false;
    return str;
}
#endif
