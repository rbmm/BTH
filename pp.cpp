#include "stdafx.h"

_NT_BEGIN

#include "sdp.h"

struct SDP_NODE_ARRAY 
{
	LONG n;
	SDP_NODE nodes[];

	void* operator new(size_t s, ULONG n)
	{
		if (PVOID pv = LocalAlloc(0, s += n * sizeof(SDP_NODE)))
		{
			RtlZeroMemory(pv, s);
			reinterpret_cast<SDP_NODE_ARRAY*>(pv)->n = n;
			return pv;
		}

		return 0;
	}

	SDP_NODE_ARRAY() {}
	SDP_NODE_ARRAY(LONG n) : n(n) {}

	void operator delete(PVOID pv)
	{
		LocalFree(pv);
	}

	SDP_NODE* pop()
	{
		LONG i = --n;
		return 0 > i ? 0 : &nodes[i];
	}

	SDP_NODE* getParent()
	{
		LONG i = n;
		return 0 > i ? 0 : &nodes[i];
	}

	ULONG NeedCount()
	{
		return -n;
	}

	ULONG SdpValidateStream(const UCHAR* pb, ULONG cb, PCSTR prefix);
};

ULONG SDP_NODE_ARRAY::SdpValidateStream(const UCHAR* pb, ULONG cb, PCSTR prefix)
{
	if (!cb)
	{
		return 0;
	}

	const UCHAR* _pb = pb;

	PSDP_NODE pParentNode = getParent();

	do 
	{
		PSDP_NODE pNode = pop();

		if (pNode)
		{
			InsertTailList(&pParentNode->sequence, pNode);
		}
		else
		{
			SDP_NODE stub;
			pNode = &stub;
		}

		SdpTag s;

		s.Tag = *pb++;
		--cb;
		ULONG size = s.Size;
		ULONG Type = s.Type;

		union {
			ULONG Len;
			UINT64 uint64;
			UINT32 uint32;
			UINT16 uint16;
			UINT8 uint8;
			GUID uuid128;
			UCHAR Data[16];
		};

		union {
			ULONG LenBytes;
			ULONG DataSize;
		};

		if (4 < size)
		{
			if (cb < (LenBytes = 1 << (size - 5)))
			{
				return 0;
			}

			Len = 0;
			do 
			{
				Len = (Len << 8)|*pb++;
			} while (--cb, --LenBytes);

			if (cb < Len)
			{
				return 0;
			}
			pNode->DataSize = Len;
		}
		else if (Type != SDP_TYPE_NIL)
		{
			if (cb < (DataSize = 1 << size))
			{
				return 0;
			}

			pNode->DataSize = DataSize;

			memcpy(Data, pb, DataSize);

			pb += DataSize, cb -= DataSize;

			switch (DataSize)
			{
			case 1:
				pNode->uint8 = uint8;
				break;
			case 2:
				pNode->uint16 = _byteswap_ushort(uint16);
				break;
			case 4:
				pNode->uint32 = _byteswap_ulong(uint32);
				break;
			case 8:
				pNode->uint64 = _byteswap_uint64(uint64);
				break;
			case 16:
				pNode->uuid128.Data1 = _byteswap_ulong(uuid128.Data1);
				pNode->uuid128.Data2 = _byteswap_ushort(uuid128.Data2);
				pNode->uuid128.Data3 = _byteswap_ushort(uuid128.Data3);
				memcpy(pNode->uuid128.Data4, uuid128.Data4, sizeof(GUID::Data4));
				break;
			}
		}

		pNode->Type = Type;

		switch (Type)
		{
		case SDP_TYPE_BOOLEAN:
			DbgPrint("%s(%s)\n", prefix, uint8 ? "true" : "false");
			[[fallthrough]];
		case SDP_TYPE_NIL:
			if (size)
			{
				return 0;
			}
			continue;

		case SDP_TYPE_UINT:
		case SDP_TYPE_INT:
			if (size > 4)
			{
				return 0;
			}

			DbgPrint("%s(UINT%u:%x)\n", prefix, DataSize << 3, pNode->uint32);
			continue;

		case SDP_TYPE_UUID:
			switch (size)
			{
			default: return 0;
			case 1:
			case 2:
				break;
			case 4:
				GUID& guid = pNode->uuid128;
				DbgPrint("%s(UUID128:%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x)\n", 
					prefix, guid.Data1, guid.Data2, guid.Data3, 
					guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
					guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
				continue;
			}

			DbgPrint("%s(UUID%u:%x)\n", prefix, DataSize << 3, pNode->uint32);
			continue;

		case SDP_TYPE_STRING:
		case SDP_TYPE_URL:
			if (size < 5)
			{
				return 0;
			}
			pNode->string = (PSTR)pb;
			DbgPrint("%s(\"%.*s\")\n", prefix, Len, pb);
			pb += Len, cb -= Len;
			continue;

		case SDP_TYPE_SEQUENCE:
		case SDP_TYPE_ALTERNATIVE:
			if (size < 5)
			{
				return 0;
			}

			InitializeListHead(&pNode->sequence);

			DbgPrint("%s[ // %X\n", prefix, Len);
			if (!(Len = SdpValidateStream(pb, Len, prefix - 1)))
			{
				return 0;
			}
			DbgPrint("%s]\n", prefix);
			pb += Len, cb -= Len;
			continue;

		default: return 0;
		}

	} while (cb);

	return RtlPointerToOffset(_pb, pb);
}

PVOID SdpValidateStream(const UCHAR* pb, ULONG cb, PSDP_NODE* ppRootNode)
{
#pragma warning(suppress: 4815)
	SDP_NODE_ARRAY sna(-1);

	char prefix[32];
	memset(prefix, '\t', _countof(prefix));
	prefix[_countof(prefix) - 1] = 0;

	if (sna.SdpValidateStream(pb, cb, prefix + _countof(prefix) - 1))
	{
		if (SDP_NODE_ARRAY* psna = new(sna.NeedCount()) SDP_NODE_ARRAY)
		{
			PSDP_NODE pNode = psna->pop();
			InitializeListHead(&pNode->sequence);
			pNode->Type = SDP_TYPE_SEQUENCE;
			pNode->DataSize = cb;

			if (psna->SdpValidateStream(pb, cb, prefix + _countof(prefix) - 1))
			{
				*ppRootNode = pNode;
				return psna;
			}

			LocalFree(psna);
		}
	}

	return 0;
}

void Dump(PSDP_NODE pNode, PCSTR prefix)
{
	ULONG DataSize = pNode->DataSize;

	switch (pNode->Type)
	{
	default: __debugbreak();
		return;

	case SDP_TYPE_BOOLEAN:
		DbgPrint("%s(%s)\n", prefix, pNode->boolean ? "true" : "false");
		[[fallthrough]];
	case SDP_TYPE_NIL:
		return;

	case SDP_TYPE_UINT:
	case SDP_TYPE_INT:

		DbgPrint("%s(UINT%u:%x)\n", prefix, DataSize << 3, pNode->uint32);
		return;

	case SDP_TYPE_UUID:
		switch (DataSize)
		{
		default: __debugbreak();
		case 2:
		case 4:
			break;
		case 16:
			GUID& guid = pNode->uuid128;
			DbgPrint("%s(UUID128:%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x)\n", 
				prefix, guid.Data1, guid.Data2, guid.Data3, 
				guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
				guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
			return;
		}

		DbgPrint("%s(UUID%u:%x)\n", prefix, DataSize << 3, pNode->uint32);
		return;

	case SDP_TYPE_STRING:
	case SDP_TYPE_URL:
		DbgPrint("%s(\"%.*s\")\n", prefix, DataSize, pNode->string);
		return;

	case SDP_TYPE_SEQUENCE:
	case SDP_TYPE_ALTERNATIVE:
		DbgPrint("%s[ // %X\n", prefix--, DataSize);
		PLIST_ENTRY head = &pNode->sequence, entry = head;

		while ((entry = entry->Flink) != head)
		{
			Dump(static_cast<PSDP_NODE>(entry), prefix);
		}
		DbgPrint("%s]\n", 1 + prefix);
		return;
	}
}

void Dump(PSDP_NODE pNode)
{
	char prefix[32];
	memset(prefix, '\t', _countof(prefix));
	prefix[_countof(prefix) - 1] = 0;

	Dump(pNode, prefix + _countof(prefix) - 1);
}

_NT_END