#pragma once

#include <bthsdpdef.h>

#define SDP_ATTRIB_SERVICE_NAME 0x100

union SdpTag 
{
	UCHAR Tag;
	struct  
	{
		UCHAR Size : 3;
		UCHAR Type : 5;
	};
};

typedef struct SDP_NODE : LIST_ENTRY {
	union {
		// UUID
		GUID uuid128;
		ULONG uuid32;
		USHORT uuid16;

		// 16 byte integers
		struct {
			UINT64 uint64L;
			UINT64 uint64H;
		};

		// 8 byte integers
		LONGLONG int64;
		ULONGLONG uint64;

		// 4 byte integers
		LONG int32;
		ULONG uint32;

		// 2 byte integers
		SHORT int16;
		USHORT uint16;

		// 1 bytes integers
		CHAR int8;
		UCHAR uint8;

		// Boolean
		BOOLEAN boolean;

		// string
		PCHAR string;

		// Sequence
		LIST_ENTRY sequence;

		// generic
		UCHAR Data[16];
	};
	ULONG Type;
	ULONG DataSize;
} * PSDP_NODE;

#include "elog.h"

BOOL GetProtocolValue(_In_ ELog& log, _In_ const UCHAR* pb, _In_ ULONG cb, _In_ UINT16 ProtocolUuid, _In_ ULONG DataSize, _Out_ PUSHORT value);