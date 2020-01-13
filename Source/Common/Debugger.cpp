#include "Debugger.h"
#include "Log.h"
#include "StringUtils.h"
#include "Timer.h"

#define MAX_BLOCKS (25)
#define MAX_ENTRY (2000)
#define MAX_PROCESS (20)

static double Ticks[MAX_BLOCKS][MAX_ENTRY][MAX_PROCESS];
static int Identifiers[MAX_BLOCKS][MAX_ENTRY][MAX_PROCESS];
static uint CurTick[MAX_BLOCKS][MAX_ENTRY];
static int CurEntry[MAX_BLOCKS];

int MemoryDebugLevel = 0;

void Debugger::BeginCycle()
{
    memzero(Identifiers, sizeof(Identifiers));
    for (int i = 0; i < MAX_BLOCKS; i++)
        CurEntry[i] = -1;
}

void Debugger::BeginBlock(int num_block)
{
    if (CurEntry[num_block] + 1 >= MAX_ENTRY)
        return;

    CurEntry[num_block]++;
    CurTick[num_block][CurEntry[num_block]] = 0;
    Ticks[num_block][CurEntry[num_block]][0] = Timer::AccurateTick();
}

void Debugger::ProcessBlock(int num_block, int identifier)
{
    if (CurEntry[num_block] + 1 >= MAX_ENTRY)
        return;

    CurTick[num_block][CurEntry[num_block]]++;
    Ticks[num_block][CurEntry[num_block]][CurTick[num_block][CurEntry[num_block]]] = Timer::AccurateTick();
    Identifiers[num_block][CurEntry[num_block]][CurTick[num_block][CurEntry[num_block]]] = identifier;
}

void Debugger::EndCycle(double lag_to_show)
{
    for (int num_block = 0; num_block < MAX_BLOCKS; num_block++)
    {
        for (int entry = 0; entry <= CurEntry[num_block]; entry++)
        {
            bool is_first = true;
            for (int i = 1, j = CurTick[num_block][entry]; i <= j; i++)
            {
                double diff = Ticks[num_block][entry][i] - Ticks[num_block][entry][i - 1];
                if (diff >= lag_to_show)
                {
                    if (is_first)
                    {
                        WriteLog("Lag in block {}...", num_block);
                        is_first = false;
                    }

                    WriteLog("<{},{}>", Identifiers[num_block][entry][i], diff);
                }
            }

            if (!is_first)
                WriteLog("\n");
        }
    }
}

void Debugger::ShowLags(int num_block, double lag_to_show)
{
    for (int entry = 0; entry <= CurEntry[num_block]; entry++)
    {
        double diff = Ticks[num_block][entry][CurTick[num_block][entry]] - Ticks[num_block][entry][0];
        if (diff >= lag_to_show)
        {
            WriteLog("Lag in block {}...", num_block);
            for (int i = 1, j = CurTick[num_block][entry]; i <= j; i++)
            {
                double diff_ = Ticks[num_block][entry][i] - Ticks[num_block][entry][i - 1];
                WriteLog("<{},{}>", Identifiers[num_block][entry][i], diff_);
            }
            WriteLog("\n");
        }
    }
}

#define MAX_MEM_NODES (18)
struct MemNode
{
    int64 AllocMem;
    int64 DeallocMem;
    int64 MinAlloc;
    int64 MaxAlloc;
} static MemNodes[MAX_MEM_NODES];

static const char* MemBlockNames[MAX_MEM_NODES] = {
    "Static       ",
    "Npc          ",
    "Clients      ",
    "Maps         ",
    "Map fields   ",
    "Proto maps   ",
    "Net buffers  ",
    "             ",
    "             ",
    "Items        ",
    "Dialogs      ",
    "             ",
    "             ",
    "Images       ",
    "             ",
    "             ",
    "             ",
    "Angel Script ",
};

static std::mutex MemLocker;

void Debugger::Memory(int block, int value)
{
    if (!value)
        return;

    SCOPE_LOCK(MemLocker);

    MemNode& node = MemNodes[block];
    if (value > 0)
    {
        node.AllocMem += value;
        if (!node.MinAlloc || value < node.MinAlloc)
            node.MinAlloc = value;
        if (value > node.MaxAlloc)
            node.MaxAlloc = value;
    }
    else
    {
        node.DeallocMem -= value;
    }
}

struct MemNodeStr
{
    char Name[256];
    int64 AllocMem;
    int64 DeallocMem;
    int64 MinAlloc;
    int64 MaxAlloc;
    bool operator==(const char* str) const { return Str::Compare(str, Name); }
};
using MemNodeStrVec = vector<MemNodeStr>;
static MemNodeStrVec MemNodesStr;

void Debugger::MemoryStr(const char* block, int value)
{
    if (!block || !value)
        return;

    SCOPE_LOCK(MemLocker);

    MemNodeStr* node;
    auto it = std::find(MemNodesStr.begin(), MemNodesStr.end(), block);
    if (it == MemNodesStr.end())
    {
        MemNodeStr node_;
        Str::Copy(node_.Name, block);
        node_.AllocMem = 0;
        node_.DeallocMem = 0;
        node_.MinAlloc = 0;
        node_.MaxAlloc = 0;
        MemNodesStr.push_back(node_);
        node = &MemNodesStr.back();
    }
    else
    {
        node = &(*it);
    }

    if (value > 0)
    {
        node->AllocMem += value;
        if (!node->MinAlloc || value < node->MinAlloc)
            node->MinAlloc = value;
        if (value > node->MaxAlloc)
            node->MaxAlloc = value;
    }
    else
    {
        node->DeallocMem -= value;
    }
}

const char* Debugger::GetMemoryStatistics()
{
    SCOPE_LOCK(MemLocker);

    // Todo: Exclude static var
    static string result;
    result = "Memory statistics:\n";

#ifdef FONLINE_CLIENT
    int64 all_alloc = 0, all_dealloc = 0;
    result +=
        "\n  Level 2                                                  Memory        Alloc         Free    Min block    Max block\n";
    for (auto it = MemNodesStr.begin(), end = MemNodesStr.end(); it != end; ++it)
    {
        MemNodeStr& node = *it;
#ifdef FO_WINDOWS
        string buf = _str("{:<50} : {:<12} {:<12} {:<12} {:<12} {:<12}\n", node.Name, node.AllocMem - node.DeallocMem,
            node.AllocMem, node.DeallocMem, node.MinAlloc, node.MaxAlloc);
#else
        string buf = _str("{:<50} : {:<12} {:<12} {:<12} {:<12} {:<12}\n", node.Name, node.AllocMem - node.DeallocMem,
            node.AllocMem, node.DeallocMem, node.MinAlloc, node.MaxAlloc);
#endif
        result += buf;
        all_alloc += node.AllocMem;
        all_dealloc += node.DeallocMem;
    }
#ifdef FO_WINDOWS
    string buf = _str("Whole memory                                       : {:<12} {:<12} {:<12}\n",
        all_alloc - all_dealloc, all_alloc, all_dealloc);
#else
    string buf = _str("Whole memory                                       : {:<12} {:<12} {:<12}\n",
        all_alloc - all_dealloc, all_alloc, all_dealloc);
#endif
    result += buf;
#endif

#if defined(FONLINE_SERVER) || defined(FONLINE_EDITOR)
    int64 all_alloc = 0, all_dealloc = 0;

    if (MemoryDebugLevel > 0)
    {
        result += "\n  Level 1            Memory        Alloc         Free    Min block    Max block\n";
        for (int i = 0; i < MAX_MEM_NODES; i++)
        {
            MemNode& node = MemNodes[i];
#ifdef FO_WINDOWS
            string buf = _str("{:<50} : {:<12} {:<12} {:<12} {:<12} {:<12}\n", MemBlockNames[i],
                node.AllocMem - node.DeallocMem, node.AllocMem, node.DeallocMem, node.MinAlloc, node.MaxAlloc);
#else
            string buf = _str("{:<50} : {:<12} {:<12} {:<12} {:<12} {:<12}\n", MemBlockNames[i],
                node.AllocMem - node.DeallocMem, node.AllocMem, node.DeallocMem, node.MinAlloc, node.MaxAlloc);
#endif
            result += buf;
            all_alloc += node.AllocMem;
            all_dealloc += node.DeallocMem;
        }
#ifdef FO_WINDOWS
        string buf = _str("Whole memory  : {:<12} {:<12} {:<12}\n", all_alloc - all_dealloc, all_alloc, all_dealloc);
#else
        string buf = _str("Whole memory  : {:<12} {:<12} {:<12}\n", all_alloc - all_dealloc, all_alloc, all_dealloc);
#endif
        result += buf;
    }

    if (MemoryDebugLevel > 1)
    {
        all_alloc = all_dealloc = 0;
        result +=
            "\n  Level 2                                                  Memory        Alloc         Free    Min block    Max block\n";
        for (auto it = MemNodesStr.begin(), end = MemNodesStr.end(); it != end; ++it)
        {
            MemNodeStr& node = *it;
#ifdef FO_WINDOWS
            string buf = _str("{:<50} : {:<12} {:<12} {:<12} {:<12} {:<12}\n", node.Name,
                node.AllocMem - node.DeallocMem, node.AllocMem, node.DeallocMem, node.MinAlloc, node.MaxAlloc);
#else
            string buf = _str("{:<50} : {:<12} {:<12} {:<12} {:<12} {:<12}\n", node.Name,
                node.AllocMem - node.DeallocMem, node.AllocMem, node.DeallocMem, node.MinAlloc, node.MaxAlloc);
#endif
            result += buf;
            all_alloc += node.AllocMem;
            all_dealloc += node.DeallocMem;
        }
#ifdef FO_WINDOWS
        string buf = _str("Whole memory                                       : {:<12} {:<12} {:<12}\n",
            all_alloc - all_dealloc, all_alloc, all_dealloc);
#else
        string buf = _str("Whole memory                                       : {:<12} {:<12} {:<12}\n",
            all_alloc - all_dealloc, all_alloc, all_dealloc);
#endif
        result += buf;
    }

    if (MemoryDebugLevel > 2)
    {
        result += "\n  Level 3\n";
        result += GetTraceMemory();
    }

    if (MemoryDebugLevel <= 0)
        result += "\n  Disabled\n";
#endif

    return result.c_str();
}

#ifdef FO_WINDOWS
#include "FileSystem.h"
#include "WinApi_Include.h"

#pragma warning(disable : 4748)
#pragma warning(disable : 4091)
#pragma warning(disable : 4996)
#include <DbgHelp.h>

// Hooks
#include "NCodeHookInstantiation.h"

// Memory tracing
struct StackInfo
{
    size_t Hash;
    char* Name;
    size_t Heap;
    size_t Chunks;
    size_t Size;
};
using StackInfoSize = pair<StackInfo*, size_t>;

static bool MemoryTrace;
static map<size_t, StackInfoSize> PtrStackInfoSize;
static map<size_t, StackInfo*> StackHashStackInfo;
static std::mutex MemoryAllocLocker;
static uint MemoryAllocRecursion;

#define ALLOCATE_PTR(ptr, size, param) \
    if (ptr) \
    { \
        StackInfo* si = GetStackInfo(param); \
        si->Size += size; \
        si->Chunks++; \
        PtrStackInfoSize.insert(std::make_pair((size_t)ptr, std::make_pair(si, size))); \
    }
#define DEALLOCATE_PTR(ptr) \
    if (ptr) \
    { \
        auto it = PtrStackInfoSize.find((size_t)ptr); \
        if (it != PtrStackInfoSize.end()) \
        { \
            StackInfo* si = it->second.first; \
            si->Size -= it->second.second; \
            si->Chunks--; \
            PtrStackInfoSize.erase(it); \
        } \
    }

#define STACKWALK_MAX_NAMELEN (2048)

static HANDLE ProcessHandle;
static NCodeHookIA32 CodeHooker;

static bool PatchWindowsAlloc();
static void PatchWindowsDealloc();

static StackInfo* GetStackInfo(HANDLE heap)
{
    void* blocks[63];
    ULONG hash;
    USHORT frames = CaptureStackBackTrace(2, 60, (void**)blocks, &hash);

    auto it = StackHashStackInfo.find((size_t)hash);
    if (it != StackHashStackInfo.end())
        return it->second;

    string str;
    str.reserve(16384);

    char symbol_buffer[sizeof(SYMBOL_INFO) + STACKWALK_MAX_NAMELEN];
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbol_buffer;
    memset(symbol, 0, sizeof(SYMBOL_INFO) + STACKWALK_MAX_NAMELEN);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = STACKWALK_MAX_NAMELEN;

    for (int frame_num = 0; frame_num < (int)frames - 1; frame_num++)
    {
        DWORD64 offset = (DWORD64)blocks[frame_num];
        if (!offset)
            continue;

        struct
        {
            CHAR name[STACKWALK_MAX_NAMELEN];
            CHAR undName[STACKWALK_MAX_NAMELEN];
            CHAR undFullName[STACKWALK_MAX_NAMELEN];
            DWORD64 offsetFromSmybol;
            DWORD offsetFromLine;
            DWORD lineNumber;
            CHAR lineFileName[STACKWALK_MAX_NAMELEN];
            CHAR moduleName[STACKWALK_MAX_NAMELEN];
            DWORD64 baseOfImage;
            CHAR loadedImageName[STACKWALK_MAX_NAMELEN];
        } callstack;
        memzero(&callstack, sizeof(callstack));

        if (SymFromAddr(ProcessHandle, offset, &callstack.offsetFromSmybol, symbol))
        {
            strncpy_s(callstack.name, symbol->Name, STACKWALK_MAX_NAMELEN - 1);
            callstack.name[STACKWALK_MAX_NAMELEN - 1] = 0;
            UnDecorateSymbolName(symbol->Name, callstack.undName, STACKWALK_MAX_NAMELEN, UNDNAME_NAME_ONLY);
            UnDecorateSymbolName(symbol->Name, callstack.undFullName, STACKWALK_MAX_NAMELEN, UNDNAME_COMPLETE);
        }

        IMAGEHLP_LINE64 line;
        memset(&line, 0, sizeof(line));
        line.SizeOfStruct = sizeof(line);
        if (SymGetLineFromAddr64(ProcessHandle, offset, &callstack.offsetFromLine, &line))
        {
            callstack.lineNumber = line.LineNumber;
            strcpy_s(callstack.lineFileName, _str(line.FileName).extractFileName().c_str());
        }

        IMAGEHLP_MODULE64 module;
        memset(&module, 0, sizeof(module));
        module.SizeOfStruct = sizeof(module);
        if (SymGetModuleInfo64(ProcessHandle, offset, &module))
        {
            strcpy(callstack.moduleName, module.ModuleName);
            callstack.baseOfImage = module.BaseOfImage;
            strcpy(callstack.loadedImageName, module.LoadedImageName);
        }

        if (callstack.name[0] == 0)
            strcpy(callstack.name, "(function-name not available)");
        if (callstack.undName[0] != 0)
            strcpy(callstack.name, callstack.undName);
        if (callstack.undFullName[0] != 0)
            strcpy(callstack.name, callstack.undFullName);
        if (callstack.moduleName[0] == 0)
            strcpy(callstack.moduleName, "???");

        char buf[64];
        str += "\t";
        str += callstack.moduleName;
        str += ", ";
        str += callstack.name;
        str += " + ";
        str += _itoa((int)callstack.offsetFromSmybol, buf, 10);
        if (callstack.lineFileName[0])
        {
            str += ", ";
            str += callstack.lineFileName;
            str += " (";
            str += _itoa(callstack.lineNumber, buf, 10);
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
    si->Name = _strdup(str.c_str());
    si->Heap = (size_t)heap;
    si->Chunks = 0;
    si->Size = 0;
    StackHashStackInfo.insert(std::make_pair((size_t)hash, si));
    return si;
}

template<class T>
class Patch
{
public:
    Patch<T>() : Call(nullptr), PatchFunc(nullptr) {}
    ~Patch<T>() { Uninstall(); }

    bool Install(void* orig, void* patch)
    {
        if (orig && patch)
            Call = CodeHooker.createHook((T)orig, (T)patch);
        PatchFunc = (T)patch;
        return Call != nullptr;
    }

    void Uninstall()
    {
        if (PatchFunc)
            CodeHooker.removeHook(PatchFunc);
        PatchFunc = nullptr;
        Call = nullptr;
    }

    T Call;
    T PatchFunc;
};

#define DECLARE_PATCH(ret, name, args) \
    static Patch<decltype(&name)> Patch_##name; \
    static ret WINAPI Hooked_##name##args
#define INSTALL_PATCH(name) \
    if (!Patch_##name.Install(GetProcAddress(hkernel32, #name), &(Hooked_##name))) \
    return false
#define UNINSTALL_PATCH(name) Patch_##name.Uninstall()

DECLARE_PATCH(HANDLE, HeapCreate, (DWORD flOptions, SIZE_T dwInitialSize, SIZE_T dwMaximumSize))
{
    return Patch_HeapCreate.Call(flOptions, dwInitialSize, dwMaximumSize);
}

DECLARE_PATCH(BOOL, HeapDestroy, (HANDLE hHeap))
{
    return Patch_HeapDestroy.Call(hHeap);
}

DECLARE_PATCH(LPVOID, HeapAlloc, (HANDLE hHeap, DWORD dwFlags, DWORD_PTR dwBytes))
{
    if (MemoryTrace)
    {
        SCOPE_LOCK(MemoryAllocLocker);

        MemoryAllocRecursion++;
        void* ptr = Patch_HeapAlloc.Call(hHeap, dwFlags | HEAP_NO_SERIALIZE, dwBytes);
        MemoryAllocRecursion--;
        if (!MemoryAllocRecursion)
        {
            MemoryAllocRecursion++;
            ALLOCATE_PTR(ptr, dwBytes, hHeap);
            MemoryAllocRecursion--;
        }
        return ptr;
    }
    return Patch_HeapAlloc.Call(hHeap, dwFlags, dwBytes);
}

DECLARE_PATCH(LPVOID, HeapReAlloc, (HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes))
{
    if (MemoryTrace)
    {
        SCOPE_LOCK(MemoryAllocLocker);

        MemoryAllocRecursion++;
        void* ptr = Patch_HeapReAlloc.Call(hHeap, dwFlags | HEAP_NO_SERIALIZE, lpMem, dwBytes);
        MemoryAllocRecursion--;
        if (!MemoryAllocRecursion)
        {
            MemoryAllocRecursion++;
            DEALLOCATE_PTR(lpMem);
            ALLOCATE_PTR(ptr, dwBytes, hHeap);
            MemoryAllocRecursion--;
        }
        return ptr;
    }
    return Patch_HeapReAlloc.Call(hHeap, dwFlags, lpMem, dwBytes);
}

DECLARE_PATCH(BOOL, HeapFree, (HANDLE hHeap, DWORD dwFlags, LPVOID lpMem))
{
    if (MemoryTrace)
    {
        SCOPE_LOCK(MemoryAllocLocker);

        MemoryAllocRecursion++;
        BOOL result = Patch_HeapFree.Call(hHeap, dwFlags | HEAP_NO_SERIALIZE, lpMem);
        MemoryAllocRecursion--;
        if (!MemoryAllocRecursion)
        {
            MemoryAllocRecursion++;
            DEALLOCATE_PTR(lpMem);
            MemoryAllocRecursion--;
        }
        return result;
    }
    return Patch_HeapFree.Call(hHeap, dwFlags, lpMem);
}

static bool PatchWindowsAlloc()
{
    HMODULE hkernel32 = GetModuleHandleW(L"kernel32");
    if (!hkernel32)
        return false;

    INSTALL_PATCH(HeapCreate);
    INSTALL_PATCH(HeapDestroy);
    INSTALL_PATCH(HeapAlloc);
    INSTALL_PATCH(HeapReAlloc);
    INSTALL_PATCH(HeapFree);
    return true;
}

static void PatchWindowsDealloc()
{
    UNINSTALL_PATCH(HeapCreate);
    UNINSTALL_PATCH(HeapDestroy);
    UNINSTALL_PATCH(HeapAlloc);
    UNINSTALL_PATCH(HeapReAlloc);
    UNINSTALL_PATCH(HeapFree);
}
#endif

void Debugger::StartTraceMemory()
{
#ifdef FO_WINDOWS
    // Stack symbols unnaming
    ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
    SymInitialize(ProcessHandle, nullptr, TRUE);
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);

    // Allocators
    PatchWindowsAlloc();

    // Begin catching
    MemoryTrace = true;
#endif
}

string Debugger::GetTraceMemory()
{
#ifdef FO_WINDOWS
    SCOPE_LOCK(MemoryAllocLocker);

    MemoryAllocRecursion++;

    // Sort by chunks count
    int64 whole_chunks = 0, whole_size = 0;
    vector<StackInfo*> blocks_chunks;
    for (auto it = StackHashStackInfo.begin(), end = StackHashStackInfo.end(); it != end; ++it)
    {
        StackInfo* si = it->second;
        if (si->Size)
        {
            blocks_chunks.push_back(si);
            whole_chunks += (int64)si->Chunks;
            whole_size += (int64)si->Size;
        }
    }
    std::sort(blocks_chunks.begin(), blocks_chunks.end(), [](auto* a, auto* b) { return a->Chunks > b->Chunks; });

    // Print
    string str;
    str += "Unfreed memory:\n";
    str += _str("  Whole blocks {}\n", (int64)blocks_chunks.size());
    str += _str("  Whole size   {}\n", (int64)whole_size);
    str += _str("  Whole chunks {}\n", (int64)whole_chunks);
    str += "\n";
    uint cur = 0;
    for (StackInfo* si : blocks_chunks)
        str += _str("{:06}) Size {:<10} Chunks {:<10} Heap {}\n{}", cur++, si->Size, si->Chunks, si->Heap, si->Name);

    MemoryAllocRecursion--;
    return str;
#else

    return "";
#endif
}
