// windows.h - cppparse's declarations of the Win32 API that its own sources and its runtime use (x64, so the
// calling-convention keywords are not needed). The real kernel32 import library resolves them.
#ifndef CPPPARSE_WINDOWS_H
#define CPPPARSE_WINDOWS_H

#ifndef _WIN32
#error "windows.h declares the Win32 API: only for a 64-bit Windows target"
#endif

#include <cstddef>

typedef int                BOOL;
typedef unsigned char      BYTE;
typedef unsigned short     WORD;
typedef unsigned long      DWORD;
typedef long               LONG;
typedef unsigned long      ULONG;
typedef unsigned int       UINT;
typedef int                INT;
typedef __cppparse_int64   LONGLONG;
typedef __cppparse_uint64  ULONGLONG;
typedef __cppparse_uint64  SIZE_T;
typedef void*              LPVOID;
typedef const void*        LPCVOID;
typedef void*              HANDLE;
typedef HANDLE             HMODULE;
typedef HANDLE             HWND;
typedef char*              LPSTR;
typedef const char*        LPCSTR;
typedef wchar_t*           LPWSTR;
typedef const wchar_t*     LPCWSTR;
typedef DWORD*             LPDWORD;
typedef __cppparse_int64   LPARAM;
typedef __cppparse_uint64  WPARAM;
typedef __cppparse_int64   LRESULT;
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID parameter);

struct SECURITY_ATTRIBUTES { DWORD nLength; LPVOID lpSecurityDescriptor; BOOL bInheritHandle; };
typedef SECURITY_ATTRIBUTES* LPSECURITY_ATTRIBUTES;

struct CRITICAL_SECTION { void* DebugInfo; LONG LockCount; LONG RecursionCount; HANDLE OwningThread; HANDLE LockSemaphore; SIZE_T SpinCount; };
typedef CRITICAL_SECTION* LPCRITICAL_SECTION;

struct SYSTEM_INFO {
    WORD wProcessorArchitecture; WORD wReserved; DWORD dwPageSize; LPVOID lpMinimumApplicationAddress; LPVOID lpMaximumApplicationAddress;
    ULONGLONG dwActiveProcessorMask; DWORD dwNumberOfProcessors; DWORD dwProcessorType; DWORD dwAllocationGranularity; WORD wProcessorLevel; WORD wProcessorRevision;
};
typedef SYSTEM_INFO* LPSYSTEM_INFO;

union LARGE_INTEGER { struct { DWORD LowPart; LONG HighPart; } u; LONGLONG QuadPart; };

struct FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; };
struct WIN32_FIND_DATAA {
    DWORD dwFileAttributes; FILETIME ftCreationTime; FILETIME ftLastAccessTime; FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh; DWORD nFileSizeLow; DWORD dwReserved0; DWORD dwReserved1; char cFileName[260]; char cAlternateFileName[14];
};
typedef WIN32_FIND_DATAA* LPWIN32_FIND_DATAA;

// ---- the unwind interface (row P1 stage 2, docs/design-selfhost.md 4.3): what cppparse's runtime uses to walk and
// restore frames. The structures are the SDK's x64 ones (winnt.h), written flat: the anonymous unions become their
// first member (RUNTIME_FUNCTION's UnwindInfoAddress) or explicit members of the same offsets (CONTEXT's FltSave area
// as Header, Legacy, Xmm0..Xmm15 and 96 bytes beyond). scripts\target.ps1's layout test prints their sizes and
// offsets through cl with the SDK and through cppparse with this header, and the two must be equal.
typedef __cppparse_uint64  DWORD64;
typedef DWORD64*           PDWORD64;
typedef __cppparse_uint64  ULONG_PTR;
typedef void*              PVOID;
struct _UNWIND_HISTORY_TABLE;
struct _KNONVOLATILE_CONTEXT_POINTERS;

struct __cppparse_align(16) _M128A { ULONGLONG Low; LONGLONG High; };
typedef _M128A M128A;
typedef M128A* PM128A;

struct __cppparse_align(16) _CONTEXT {
    DWORD64 P1Home, P2Home, P3Home, P4Home, P5Home, P6Home;
    DWORD   ContextFlags;
    DWORD   MxCsr;
    WORD    SegCs, SegDs, SegEs, SegFs, SegGs, SegSs;
    DWORD   EFlags;
    DWORD64 Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    DWORD64 Rax, Rcx, Rdx, Rbx, Rsp, Rbp, Rsi, Rdi, R8, R9, R10, R11, R12, R13, R14, R15;
    DWORD64 Rip;
    M128A   Header[2];                       // the SDK's union of FltSave (XMM_SAVE_AREA32, 512 bytes) and these
    M128A   Legacy[8];
    M128A   Xmm0, Xmm1, Xmm2, Xmm3, Xmm4, Xmm5, Xmm6, Xmm7, Xmm8, Xmm9, Xmm10, Xmm11, Xmm12, Xmm13, Xmm14, Xmm15;
    M128A   FltSaveRest[6];                  // the rest of the 512 bytes
    M128A   VectorRegister[26];
    DWORD64 VectorControl;
    DWORD64 DebugControl, LastBranchToRip, LastBranchFromRip, LastExceptionToRip, LastExceptionFromRip;
};
typedef _CONTEXT CONTEXT;
typedef CONTEXT* PCONTEXT;

struct _EXCEPTION_RECORD {
    DWORD                      ExceptionCode;
    DWORD                      ExceptionFlags;
    struct _EXCEPTION_RECORD*  ExceptionRecord;
    PVOID                      ExceptionAddress;
    DWORD                      NumberParameters;
    ULONG_PTR                  ExceptionInformation[15];
};
typedef _EXCEPTION_RECORD EXCEPTION_RECORD;
typedef EXCEPTION_RECORD* PEXCEPTION_RECORD;

struct _IMAGE_RUNTIME_FUNCTION_ENTRY { DWORD BeginAddress; DWORD EndAddress; DWORD UnwindInfoAddress; };
typedef _IMAGE_RUNTIME_FUNCTION_ENTRY RUNTIME_FUNCTION;
typedef RUNTIME_FUNCTION* PRUNTIME_FUNCTION;

enum EXCEPTION_DISPOSITION { ExceptionContinueExecution, ExceptionContinueSearch, ExceptionNestedException, ExceptionCollidedUnwind };
typedef EXCEPTION_DISPOSITION (*PEXCEPTION_ROUTINE)(struct _EXCEPTION_RECORD* record, PVOID establisherFrame, struct _CONTEXT* context, PVOID dispatcherContext);

struct _DISPATCHER_CONTEXT {
    DWORD64                        ControlPc;
    DWORD64                        ImageBase;
    PRUNTIME_FUNCTION              FunctionEntry;
    DWORD64                        EstablisherFrame;
    DWORD64                        TargetIp;
    PCONTEXT                       ContextRecord;
    PEXCEPTION_ROUTINE             LanguageHandler;
    PVOID                          HandlerData;
    struct _UNWIND_HISTORY_TABLE*  HistoryTable;
    DWORD                          ScopeIndex;
    DWORD                          Fill0;
};
typedef _DISPATCHER_CONTEXT DISPATCHER_CONTEXT;

#define UNW_FLAG_NHANDLER 0x0
#define UNW_FLAG_EHANDLER 0x1
#define UNW_FLAG_UHANDLER 0x2

extern "C" {
    HANDLE CreateThread(LPSECURITY_ATTRIBUTES attributes, SIZE_T stackSize, LPTHREAD_START_ROUTINE start, LPVOID parameter, DWORD flags, LPDWORD threadId);
    DWORD  WaitForSingleObject(HANDLE handle, DWORD milliseconds);
    DWORD  WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL waitAll, DWORD milliseconds);
    BOOL   CloseHandle(HANDLE handle);
    HANDLE CreateEventA(LPSECURITY_ATTRIBUTES attributes, BOOL manualReset, BOOL initialState, LPCSTR name);
    BOOL   SetEvent(HANDLE event);
    BOOL   ResetEvent(HANDLE event);
    HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES attributes, DWORD disposition, DWORD flags, HANDLE templateFile);
    HANDLE CreateFileW(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES attributes, DWORD disposition, DWORD flags, HANDLE templateFile);
    HANDLE FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA data);
    BOOL   FindNextFileA(HANDLE find, LPWIN32_FIND_DATAA data);
    BOOL   FindClose(HANDLE find);
    DWORD  GetFullPathNameA(LPCSTR name, DWORD size, LPSTR buffer, LPSTR* filePart);
    DWORD  GetModuleFileNameA(HMODULE module, LPSTR buffer, DWORD size);
    DWORD  GetLastError(void);
    DWORD  GetCurrentThreadId(void);
    DWORD  GetTickCount(void);
    void   Sleep(DWORD milliseconds);
    void   GetSystemInfo(LPSYSTEM_INFO info);
    void   InitializeCriticalSection(LPCRITICAL_SECTION section);
    void   EnterCriticalSection(LPCRITICAL_SECTION section);
    void   LeaveCriticalSection(LPCRITICAL_SECTION section);
    void   DeleteCriticalSection(LPCRITICAL_SECTION section);
    LONG   cppparse_interlocked_increment(LONG volatile* value);   // compiler intrinsics in MSVC: supplied by cppparse's runtime
    LONG   cppparse_interlocked_decrement(LONG volatile* value);
    BOOL   QueryPerformanceCounter(LARGE_INTEGER* counter);
    BOOL   QueryPerformanceFrequency(LARGE_INTEGER* frequency);
    // the unwind interface (row P1 stage 2): KERNEL32.dll exports all five on x64
    void               RtlCaptureContext(PCONTEXT context);
    PRUNTIME_FUNCTION  RtlLookupFunctionEntry(DWORD64 controlPc, PDWORD64 imageBase, struct _UNWIND_HISTORY_TABLE* history);
    PEXCEPTION_ROUTINE RtlVirtualUnwind(DWORD handlerType, DWORD64 imageBase, DWORD64 controlPc, PRUNTIME_FUNCTION functionEntry, PCONTEXT context, PVOID* handlerData, PDWORD64 establisherFrame, struct _KNONVOLATILE_CONTEXT_POINTERS* contextPointers);
    void               RtlRestoreContext(PCONTEXT context, struct _EXCEPTION_RECORD* record);
    BOOL               SwitchToThread(void);
}

#define TRUE  1
#define FALSE 0
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define WAIT_FAILED 0xFFFFFFFF
#define INVALID_HANDLE_VALUE ((HANDLE)(__cppparse_int64)-1)
#define MAX_PATH 260
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define CreateFile  CreateFileA
#define CreateEvent CreateEventA
#define InterlockedIncrement cppparse_interlocked_increment
#define InterlockedDecrement cppparse_interlocked_decrement
#define WINAPI
#define CALLBACK
#define APIENTRY

#endif
