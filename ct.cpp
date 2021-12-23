#pragma once

#define DECLSPEC_DEPRECATED_DDK

#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NON_CONFORMING_SWPRINTFS
#define _NO_CRT_STDIO_INLINE
#define _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES 0

#define _NT_BEGIN namespace NT {
#define _NT_END }

#pragma warning(disable : 4005 5040)

_NT_BEGIN

struct _SECURITY_QUALITY_OF_SERVICE;
struct _CONTEXT;

_NT_END

#include <stdlib.h>
//#include <wchar.h>
#include <stdio.h>
#include <string.h>

#define RtlInitializeCorrelationVector _RtlInitializeCorrelationVector_
#define RtlIncrementCorrelationVector _RtlIncrementCorrelationVector_
#define RtlExtendCorrelationVector _RtlExtendCorrelationVector_
#define RtlValidateCorrelationVector _RtlValidateCorrelationVector_
#define RtlRaiseCustomSystemEventTrigger _RtlRaiseCustomSystemEventTrigger_
#define RtlCaptureContext _RtlCaptureContext_
#define RtlGetNonVolatileToken _RtlGetNonVolatileToken_
#define RtlFreeNonVolatileToken _RtlFreeNonVolatileToken_
#define RtlFlushNonVolatileMemory _RtlFlushNonVolatileMemory_
#define RtlDrainNonVolatileFlush _RtlDrainNonVolatileFlush_
#define RtlWriteNonVolatileMemory _RtlWriteNonVolatileMemory_
#define RtlFillNonVolatileMemory _RtlFillNonVolatileMemory_
#define RtlFlushNonVolatileMemoryRanges _RtlFlushNonVolatileMemoryRanges_
#define RtlCaptureContext2 _RtlCaptureContext2_

#define _INC_MMSYSTEM  /* Prevent inclusion of mmsystem.h in windows.h */

#include <WinSock2.h>
#include <intrin.h>

#undef RtlInitializeCorrelationVector
#undef RtlIncrementCorrelationVector
#undef RtlExtendCorrelationVector
#undef RtlValidateCorrelationVector
#undef RtlRaiseCustomSystemEventTrigger
#undef RtlCaptureContext
#undef RtlGetNonVolatileToken
#undef RtlFreeNonVolatileToken
#undef RtlFlushNonVolatileMemory
#undef RtlDrainNonVolatileFlush
#undef RtlWriteNonVolatileMemory
#undef RtlFillNonVolatileMemory
#undef RtlFlushNonVolatileMemoryRanges
#undef RtlCaptureContext2

#ifdef SECURITY_WIN32
#define InitSecurityInterfaceW _InitSecurityInterfaceW_
#include <Sspi.h>
#undef InitSecurityInterfaceW
#endif // SECURITY_WIN32

#undef _INC_MMSYSTEM

_NT_BEGIN

#define RtlCompareMemory ::RtlCompareMemory

#ifdef _RTL_RUN_ONCE_DEF
#undef _RTL_RUN_ONCE_DEF
#endif

#ifdef NOWINBASEINTERLOCK

#if !defined(_X86_)

#define InterlockedPopEntrySList(Head) ExpInterlockedPopEntrySList(Head)

#define InterlockedPushEntrySList(Head, Entry) ExpInterlockedPushEntrySList(Head, Entry)

#define InterlockedFlushSList(Head) ExpInterlockedFlushSList(Head)

#else // !defined(_X86_)

EXTERN_C_START

__declspec(dllimport)
PSLIST_ENTRY
__fastcall
InterlockedPopEntrySList (PSLIST_HEADER ListHead);

__declspec(dllimport)
PSLIST_ENTRY
__fastcall
InterlockedPushEntrySList (PSLIST_HEADER ListHead,PSLIST_ENTRY ListEntry);

EXTERN_C_END

#define InterlockedFlushSList(Head) \
	ExInterlockedFlushSList(Head)

#endif // !defined(_X86_)

#endif//NOWINBASEINTERLOCK

#define RtlOsDeploymentState _RtlOsDeploymentState_
#define CUSTOM_SYSTEM_EVENT_TRIGGER_INIT _CUSTOM_SYSTEM_EVENT_TRIGGER_INIT_

#include <ntifs.h>

#undef RtlOsDeploymentState
#undef CUSTOM_SYSTEM_EVENT_TRIGGER_INIT

_NT_END

#pragma warning(disable : 4005 5040)
