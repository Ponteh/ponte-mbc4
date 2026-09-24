#include "RealtimeAudit.h"
#include <cstdlib>
#include <cstring>
#include <new>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <malloc.h>
#endif
namespace realtimeAudit {
thread_local bool active=false;
thread_local Counts counts;
}
void* operator new(std::size_t n) {
    if(realtimeAudit::active)++realtimeAudit::counts.allocations;
    if(void* p=std::malloc(n?n:1))return p; throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if(p && realtimeAudit::active)++realtimeAudit::counts.frees;std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }
#if defined(_WIN32)
void* operator new(std::size_t n,std::align_val_t a) {
    if(realtimeAudit::active)++realtimeAudit::counts.allocations;
    if(void* p=_aligned_malloc(n?n:1,static_cast<std::size_t>(a)))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n,std::align_val_t a) { return ::operator new(n,a); }
void operator delete(void* p,std::align_val_t) noexcept { if(p && realtimeAudit::active)++realtimeAudit::counts.frees;_aligned_free(p); }
void operator delete[](void* p,std::align_val_t a) noexcept { ::operator delete(p,a); }
void operator delete(void* p,std::size_t,std::align_val_t a) noexcept { ::operator delete(p,a); }
void operator delete[](void* p,std::size_t,std::align_val_t a) noexcept { ::operator delete(p,a); }
namespace realtimeAudit {
// Patch only imports of THIS diagnostic executable, outside measured callbacks.
// This does not observe calls wholly inside an OS/CRT DLL or the DAW.
namespace {
decltype(&EnterCriticalSection) originalEnter=::EnterCriticalSection;
decltype(&TryEnterCriticalSection) originalTry=::TryEnterCriticalSection;
decltype(&AcquireSRWLockExclusive) originalExclusive=::AcquireSRWLockExclusive;
decltype(&AcquireSRWLockShared) originalShared=::AcquireSRWLockShared;
decltype(&WaitForSingleObject) originalWait=::WaitForSingleObject;
decltype(&WriteFile) originalWrite=::WriteFile;
decltype(&ReadFile) originalRead=::ReadFile;
void* (__cdecl* originalMalloc)(size_t)=std::malloc;
void* (__cdecl* originalCalloc)(size_t,size_t)=std::calloc;
void* (__cdecl* originalRealloc)(void*,size_t)=std::realloc;
using MtxFunction=int (__cdecl*)(void*);
MtxFunction originalMtx=nullptr,originalTryMtx=nullptr;
void WINAPI enter(LPCRITICAL_SECTION p) { if(active)++counts.locks;originalEnter(p); }
BOOL WINAPI tryEnter(LPCRITICAL_SECTION p) { if(active)++counts.locks;return originalTry(p); }
void WINAPI exclusive(PSRWLOCK p) { if(active)++counts.locks;originalExclusive(p); }
void WINAPI shared(PSRWLOCK p) { if(active)++counts.locks;originalShared(p); }
DWORD WINAPI wait(HANDLE h,DWORD t) { if(active)++counts.waits;return originalWait(h,t); }
BOOL WINAPI write(HANDLE h,LPCVOID p,DWORD n,LPDWORD w,LPOVERLAPPED o) { if(active)++counts.io;return originalWrite(h,p,n,w,o); }
BOOL WINAPI read(HANDLE h,LPVOID p,DWORD n,LPDWORD w,LPOVERLAPPED o) { if(active)++counts.io;return originalRead(h,p,n,w,o); }
void* __cdecl alloc(size_t n) { if(active)++counts.allocations;return originalMalloc(n); }
void* __cdecl callocHook(size_t n,size_t z) { if(active)++counts.allocations;return originalCalloc(n,z); }
void* __cdecl reallocHook(void* p,size_t n) { if(active)++counts.allocations;return originalRealloc(p,n); }
int __cdecl mtx(void* p) { if(active)++counts.locks;return originalMtx(p); }
int __cdecl tryMtx(void* p) { if(active)++counts.locks;return originalTryMtx(p); }
template<typename T> void* hook(void** slot,T replacement,T& saved) {
    if(*slot==reinterpret_cast<void*>(replacement))return nullptr;
    DWORD old=0;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old))return nullptr;
    saved=reinterpret_cast<T>(*slot);*slot=reinterpret_cast<void*>(replacement);
    DWORD ignored=0;VirtualProtect(slot,sizeof(void*),old,&ignored);return *slot;
}
}
int installImportHooks() {
    static int installed=-1;if(installed>=0)return installed;installed=0;
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    auto rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if(!rva)return 0;
    for(auto* d=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+rva);d->Name;++d) {
        if(!d->OriginalFirstThunk)continue;
        auto* names=reinterpret_cast<IMAGE_THUNK_DATA*>(base+d->OriginalFirstThunk);
        auto* slots=reinterpret_cast<IMAGE_THUNK_DATA*>(base+d->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots) {
            if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))continue;
            auto* n=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
            auto* name=reinterpret_cast<const char*>(n->Name);
            auto** slot=reinterpret_cast<void**>(&slots->u1.Function);
#define INSTALL(label,fn,orig) if(std::strcmp(name,label)==0 && hook(slot,&fn,orig))++installed
            INSTALL("EnterCriticalSection",enter,originalEnter);
            INSTALL("TryEnterCriticalSection",tryEnter,originalTry);
            INSTALL("AcquireSRWLockExclusive",exclusive,originalExclusive);
            INSTALL("AcquireSRWLockShared",shared,originalShared);
            INSTALL("WaitForSingleObject",wait,originalWait);
            INSTALL("WriteFile",write,originalWrite);INSTALL("ReadFile",read,originalRead);
            INSTALL("malloc",alloc,originalMalloc);INSTALL("calloc",callocHook,originalCalloc);
            INSTALL("realloc",reallocHook,originalRealloc);
            INSTALL("_Mtx_lock",mtx,originalMtx);INSTALL("_Mtx_trylock",tryMtx,originalTryMtx);
#undef INSTALL
        }
    }
    return installed;
}
bool selfTest() {
    CRITICAL_SECTION cs;InitializeCriticalSection(&cs);
    auto event=CreateEventW(nullptr,TRUE,TRUE,nullptr);
    auto file=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    { Guard g;auto* p=::operator new(32);::operator delete(p);EnterCriticalSection(&cs);LeaveCriticalSection(&cs);WaitForSingleObject(event,0);DWORD written=0;WriteFile(file,"x",1,&written,nullptr); }
    const bool ok=counts.allocations>0 && counts.frees>0 && counts.locks>0 && counts.waits>0 && counts.io>0;
    DeleteCriticalSection(&cs);CloseHandle(event);CloseHandle(file);return ok;
}
}
#else
namespace realtimeAudit { int installImportHooks(){return 0;} bool selfTest(){return false;} }
#endif
