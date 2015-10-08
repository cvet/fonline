#include "Common.h"
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

#define MAX_MEM_NODES    ( 18 )
struct MemNode
{
    int64 AllocMem;
    int64 DeallocMem;
    int64 MinAlloc;
    int64 MaxAlloc;
} static MemNodes[ MAX_MEM_NODES ];

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
    "Script arrays",
    "Script dict  ",
    "Angel Script ",
};

static Mutex* MemLocker = NULL;

void Debugger::Memory( int block, int value )
{
    if( !MemLocker )
        MemLocker = new Mutex();
    MemLocker->Lock();

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

    MemLocker->Unlock();
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
    MemLocker->Lock();

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

    MemLocker->Unlock();
}

const char* Debugger::GetMemoryStatistics()
{
    if( !MemLocker )
        MemLocker = new Mutex();
    MemLocker->Lock();

    #pragma MESSAGE("Exclude static var.")
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
            # else
            Str::Format( buf, "%s : %12lld %12lld %12lld %12lld %12lld\n", MemBlockNames[ i ], node.AllocMem - node.DeallocMem, node.AllocMem, node.DeallocMem, node.MinAlloc, node.MaxAlloc );
            # endif
            result += buf;
            all_alloc += node.AllocMem;
            all_dealloc += node.DeallocMem;
        }
        # ifdef FO_WINDOWS
        Str::Format( buf, "Whole memory  : %12I64d %12I64d %12I64d\n", all_alloc - all_dealloc, all_alloc, all_dealloc );
        # else
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
            # else
            Str::Format( buf, "%-50s : %12lld %12lld %12lld %12lld %12lld\n", node.Name, node.AllocMem - node.DeallocMem, node.AllocMem, node.DeallocMem, node.MinAlloc, node.MaxAlloc );
            # endif
            result += buf;
            all_alloc += node.AllocMem;
            all_dealloc += node.DeallocMem;
        }
        # ifdef FO_WINDOWS
        Str::Format( buf, "Whole memory                                       : %12I64d %12I64d %12I64d\n", all_alloc - all_dealloc, all_alloc, all_dealloc );
        # else
        Str::Format( buf, "Whole memory                                       : %12lld %12lld %12lld\n", all_alloc - all_dealloc, all_alloc, all_dealloc );
        # endif
        result += buf;
    }

    if( MemoryDebugLevel >= 3 )
    {
        result += "\n  Level 3\n";
        result += GetTraceMemory();
    }

    if( MemoryDebugLevel <= 0 )
        result += "\n  Disabled\n";
    #endif

    MemLocker->Unlock();
    return result.c_str();
}

// Memory tracing
struct StackInfo
{
    size_t Hash;
    char*  Name;
    size_t Heap;
    size_t Chunks;
    size_t Size;
};
static bool                         MemoryTrace;
typedef pair< StackInfo*, size_t > StackInfoSize;
static map< size_t, StackInfoSize > PtrStackInfoSize;
static map< size_t, StackInfo* >    StackHashStackInfo;
static Mutex                        MemoryAllocLocker;
static uint                         MemoryAllocRecursion;

#define ALLOCATE_PTR( ptr, size, param )                                   \
    if( ptr )                                                              \
    {                                                                      \
        StackInfo* si = GetStackInfo( param );                             \
        si->Size += size;                                                  \
        si->Chunks++;                                                      \
        PtrStackInfoSize.insert( PAIR( (size_t) ptr, PAIR( si, size ) ) ); \
    }
#define DEALLOCATE_PTR( ptr )                            \
    if( ptr )                                            \
    {                                                    \
        auto it = PtrStackInfoSize.find( (size_t) ptr ); \
        if( it != PtrStackInfoSize.end() )               \
        {                                                \
            StackInfo* si = it->second.first;            \
            si->Size -= it->second.second;               \
            si->Chunks--;                                \
            PtrStackInfoSize.erase( it );                \
        }                                                \
    }

#ifdef FO_WINDOWS
// Stack
# pragma warning( disable : 4748 )
# include <DbgHelp.h>
# pragma comment( lib, "Dbghelp.lib" )
# include "FileManager.h"

// Hooks
# include "NCodeHook/NCodeHookInstantiation.h"

static HANDLE        ProcessHandle;
static NCodeHookIA32 CodeHooker;

bool PatchWindowsAlloc();
void PatchWindowsDealloc();

StackInfo* GetStackInfo( HANDLE heap )
{
    void*  blocks[ 63 ];
    ULONG  hash;
    USHORT frames = CaptureStackBackTrace( 2, 60, (void**) blocks, &hash );

    auto   it = StackHashStackInfo.find( (size_t) hash );
    if( it != StackHashStackInfo.end() )
        return it->second;

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

    StackInfo* si = new StackInfo();
    si->Hash = hash;
    si->Name = _strdup( str.c_str() );
    si->Heap = (size_t) heap;
    si->Chunks = 0;
    si->Size = 0;
    StackHashStackInfo.insert( PAIR( (size_t) hash, si ) );
    return si;
}

template< class T >
class Patch
{
public:
    Patch< T >(): Call( NULL ), PatchFunc( NULL ) {}
    ~Patch< T >() { Uninstall(); }
    bool Install( void* orig, void* patch )
    {
        if( orig && patch )
            Call = CodeHooker.createHook( (T) orig, (T) patch );
        PatchFunc = (T) patch;
        return Call != NULL;
    }
    void Uninstall()
    {
        if( PatchFunc )
            CodeHooker.removeHook( PatchFunc );
        PatchFunc = NULL;
        Call = NULL;
    }
    T Call;
    T PatchFunc;
};

# define DECLARE_PATCH( ret, name, args )      \
    Patch< decltype(& name ) > Patch_ ## name; \
    ret WINAPI Hooked_ ## name ## args
# define INSTALL_PATCH( name )                                                                 \
    if( !Patch_ ## name.Install( GetProcAddress( hkernel32, # name ), &( Hooked_ ## name ) ) ) \
        return false
# define UNINSTALL_PATCH( name ) \
    Patch_ ## name.Uninstall()

DECLARE_PATCH( HANDLE, HeapCreate, ( DWORD flOptions, SIZE_T dwInitialSize, SIZE_T dwMaximumSize ) )
{
    return Patch_HeapCreate.Call( flOptions, dwInitialSize, dwMaximumSize );
}

DECLARE_PATCH( BOOL, HeapDestroy, (HANDLE hHeap) )
{
    return Patch_HeapDestroy.Call( hHeap );
}

DECLARE_PATCH( LPVOID, HeapAlloc, ( HANDLE hHeap, DWORD dwFlags, DWORD_PTR dwBytes ) )
{
    if( MemoryTrace )
    {
        SCOPE_LOCK( MemoryAllocLocker );
        MemoryAllocRecursion++;
        void* ptr = Patch_HeapAlloc.Call( hHeap, dwFlags | HEAP_NO_SERIALIZE, dwBytes );
        MemoryAllocRecursion--;
        if( !MemoryAllocRecursion )
        {
            MemoryAllocRecursion++;
            ALLOCATE_PTR( ptr, dwBytes, hHeap );
            MemoryAllocRecursion--;
        }
        return ptr;
    }
    return Patch_HeapAlloc.Call( hHeap, dwFlags, dwBytes );
}

DECLARE_PATCH( LPVOID, HeapReAlloc, ( HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes ) )
{
    if( MemoryTrace )
    {
        SCOPE_LOCK( MemoryAllocLocker );
        MemoryAllocRecursion++;
        void* ptr = Patch_HeapReAlloc.Call( hHeap, dwFlags | HEAP_NO_SERIALIZE, lpMem, dwBytes );
        MemoryAllocRecursion--;
        if( !MemoryAllocRecursion )
        {
            MemoryAllocRecursion++;
            DEALLOCATE_PTR( lpMem );
            ALLOCATE_PTR( ptr, dwBytes, hHeap );
            MemoryAllocRecursion--;
        }
        return ptr;
    }
    return Patch_HeapReAlloc.Call( hHeap, dwFlags, lpMem, dwBytes );
}

DECLARE_PATCH( BOOL, HeapFree, ( HANDLE hHeap, DWORD dwFlags, LPVOID lpMem ) )
{
    if( MemoryTrace )
    {
        SCOPE_LOCK( MemoryAllocLocker );
        MemoryAllocRecursion++;
        BOOL result = Patch_HeapFree.Call( hHeap, dwFlags | HEAP_NO_SERIALIZE, lpMem );
        MemoryAllocRecursion--;
        if( !MemoryAllocRecursion )
        {
            MemoryAllocRecursion++;
            DEALLOCATE_PTR( lpMem );
            MemoryAllocRecursion--;
        }
        return result;
    }
    return Patch_HeapFree.Call( hHeap, dwFlags, lpMem );
}

bool PatchWindowsAlloc()
{
    HMODULE hkernel32 = GetModuleHandle( "kernel32" );
    if( !hkernel32 )
        return false;
    INSTALL_PATCH( HeapCreate );
    INSTALL_PATCH( HeapDestroy );
    INSTALL_PATCH( HeapAlloc );
    INSTALL_PATCH( HeapReAlloc );
    INSTALL_PATCH( HeapFree );
    return true;
}

void PatchWindowsDealloc()
{
    UNINSTALL_PATCH( HeapCreate );
    UNINSTALL_PATCH( HeapDestroy );
    UNINSTALL_PATCH( HeapAlloc );
    UNINSTALL_PATCH( HeapReAlloc );
    UNINSTALL_PATCH( HeapFree );
}

#else

StackInfo* GetStackInfo( const void* caller )
{
    auto it = StackHashStackInfo.find( (size_t) caller );
    if( it != StackHashStackInfo.end() )
        return it->second;

    string str;
    str.reserve( 16384 );

    char buf[ 64 ];
    sprintf( buf, "\t%p\n\n", caller );
    str += buf;

    StackInfo* si = new StackInfo();
    si->Hash = (size_t) caller;
    si->Name = strdup( str.c_str() );
    si->Heap = 0;
    si->Chunks = 0;
    si->Size = 0;
    StackHashStackInfo.insert( PAIR( (size_t) caller, si ) );
    return si;
}

#endif

#ifdef FO_LINUX

# include <malloc.h>

Mutex HookLocker;

void* malloc_hook( size_t size, const void* caller );
void* realloc_hook( void* ptr, size_t size, const void* caller );
void  free_hook( void* ptr, const void* caller );

void* malloc_hook( size_t size, const void* caller )
{
    SCOPE_LOCK( HookLocker );

    __malloc_hook = NULL;
    void* ptr = malloc( size );
    if( MemoryTrace )
        ALLOCATE_PTR( ptr, size, caller );
    __malloc_hook = malloc_hook;
    return ptr;
}

void* realloc_hook( void* ptr, size_t size, const void* caller )
{
    SCOPE_LOCK( HookLocker );

    __realloc_hook = NULL;
    if( MemoryTrace )
        DEALLOCATE_PTR( ptr );
    ptr = realloc( ptr, size );
    if( MemoryTrace )
        ALLOCATE_PTR( ptr, size, caller );
    __realloc_hook = realloc_hook;
    return ptr;
}

void free_hook( void* ptr, const void* caller )
{
    SCOPE_LOCK( HookLocker );

    __free_hook = NULL;
    free( ptr );
    if( MemoryTrace )
        DEALLOCATE_PTR( ptr );
    __free_hook = free_hook;
}

#endif

void Debugger::StartTraceMemory()
{
    #ifdef FO_WINDOWS
    // Stack symbols unnaming
    ProcessHandle = OpenProcess( PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId() );
    SymInitialize( ProcessHandle, NULL, TRUE );
    SymSetOptions( SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS );

    // Allocators
    PatchWindowsAlloc();
    #endif

    #ifdef FO_LINUX
    __malloc_hook = malloc_hook;
    __realloc_hook = realloc_hook;
    __free_hook = free_hook;
    #endif

    // Begin catching
    MemoryTrace = true;
}

struct ChunkSorter
{
    static bool Compare( StackInfo* a, StackInfo* b )
    {
        return a->Chunks > b->Chunks;
    }
};
string Debugger::GetTraceMemory()
{
    SCOPE_LOCK( MemoryAllocLocker );
    MemoryAllocRecursion++;

    // Sort by chunks count
    int64                whole_chunks = 0, whole_size = 0;
    vector< StackInfo* > blocks_chunks;
    for( auto it = StackHashStackInfo.begin(), end = StackHashStackInfo.end(); it != end; ++it )
    {
        StackInfo* si = it->second;
        if( si->Size )
        {
            blocks_chunks.push_back( si );
            whole_chunks += (int64) si->Chunks;
            whole_size += (int64) si->Size;
        }
    }
    std::sort( blocks_chunks.begin(), blocks_chunks.end(), ChunkSorter::Compare );

    // Print
    string str;
    str.reserve( 1000000 );
    char   buf[ MAX_FOTEXT ];
    str += "Unfreed memory:\n";
    sprintf( buf, "  Whole blocks %lld\n", (int64) blocks_chunks.size() );
    str += buf;
    sprintf( buf, "  Whole size   %lld\n", (int64) whole_size );
    str += buf;
    sprintf( buf, "  Whole chunks %lld\n", (int64) whole_chunks );
    str += buf;
    str += "\n";
    uint cur = 0;
    for( auto it = blocks_chunks.begin(), end = blocks_chunks.end(); it != end; ++it )
    {
        StackInfo* si = *it;
        sprintf( buf, "%06u) Size %-10zu Chunks %-10zu Heap %zu\n", cur++, si->Size, si->Chunks, si->Heap );
        str += buf;
        str += si->Name;
    }

    MemoryAllocRecursion--;
    return str;
}
