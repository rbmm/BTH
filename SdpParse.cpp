#include "stdafx.h"

_NT_BEGIN

#include "sdp.h"

//C_ASSERT(sizeof(SDP_NODE)==0x28);

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

	ULONG SdpValidateStream(const UCHAR* pb, ULONG cb);
};

ULONG SDP_NODE_ARRAY::SdpValidateStream(const UCHAR* pb, ULONG cb)
{
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
			struct {
				UINT64 uint64L;
				UINT64 uint64H;
			};
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
				if (Type == SDP_TYPE_UUID)
				{
					pNode->uuid128.Data1 = _byteswap_ulong(uuid128.Data1);
					pNode->uuid128.Data2 = _byteswap_ushort(uuid128.Data2);
					pNode->uuid128.Data3 = _byteswap_ushort(uuid128.Data3);
					pNode->uint64H = uint64H;
				}
				else
				{
					pNode->uint64L = _byteswap_uint64(uint64H);
					pNode->uint64H = _byteswap_uint64(uint64L);
				}
				break;
			}
		}

		pNode->Type = Type;

		switch (Type)
		{
		case SDP_TYPE_BOOLEAN:
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
			continue;

		case SDP_TYPE_UUID:
			switch (size)
			{
			case 1:
			case 2:
			case 4:
				continue;
			}
			return 0;

		case SDP_TYPE_STRING:
		case SDP_TYPE_URL:
			if (size < 5)
			{
				return 0;
			}
			pNode->string = (PSTR)pb;
			pb += Len, cb -= Len;
			continue;

		case SDP_TYPE_SEQUENCE:
		case SDP_TYPE_ALTERNATIVE:
			if (size < 5)
			{
				return 0;
			}

			InitializeListHead(&pNode->sequence);

			if (Len && !(Len = SdpValidateStream(pb, Len)))
			{
				return 0;
			}
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

	if (sna.SdpValidateStream(pb, cb))
	{
		if (SDP_NODE_ARRAY* psna = new(sna.NeedCount()) SDP_NODE_ARRAY)
		{
			PSDP_NODE pNode = psna->pop();
			InitializeListHead(&pNode->sequence);
			pNode->Type = SDP_TYPE_SEQUENCE;
			pNode->DataSize = cb;

			if (psna->SdpValidateStream(pb, cb))
			{
				*ppRootNode = pNode - 1;
				return psna;
			}

			LocalFree(psna);
		}
	}

	return 0;
}

BOOL I_GetProtocolValue(_In_ PSDP_NODE pNode, _In_ UINT16 ProtocolUuid, _In_ ULONG DataSize, _Out_ PUSHORT value)
{
	if (pNode->Type == SDP_TYPE_SEQUENCE)
	{
		ULONG i = 0;

		PLIST_ENTRY head = &pNode->sequence, entry = head;

		while ((entry = entry->Flink) != head)
		{
			pNode = static_cast<PSDP_NODE>(entry);

			switch (i++)
			{
			case 0:
				if (pNode->Type == SDP_TYPE_UUID &&
					pNode->DataSize == sizeof(UINT16) &&
					pNode->uuid16 == ProtocolUuid) continue;
				break;
			case 1:
				if (pNode->Type == SDP_TYPE_UINT && pNode->DataSize == DataSize)
				{
					*value = pNode->uint16;
					return TRUE;
				}
				break;
			}
			return FALSE;
		}
	}

	return FALSE;
}

BOOL GetProtocolValue(_In_ PSDP_NODE pNode, _In_ UINT16 ProtocolUuid, _In_ ULONG DataSize, _Out_ PUSHORT value)
{
	if (pNode->Type == SDP_TYPE_SEQUENCE)
	{
		BOOL b = FALSE;

		PLIST_ENTRY head = &pNode->sequence, entry = head;

		while ((entry = entry->Flink) != head)
		{
			pNode = static_cast<PSDP_NODE>(entry);

			if (b)
			{
				if (pNode->Type == SDP_TYPE_SEQUENCE)
				{
					head = &pNode->sequence, entry = head;

					while ((entry = entry->Flink) != head)
					{
						pNode = static_cast<PSDP_NODE>(entry);

						if (I_GetProtocolValue(pNode, ProtocolUuid, DataSize, value))
						{
							return TRUE;
						}
					}
				}

				break;
			}

			switch (pNode->Type)
			{
			case SDP_TYPE_UINT:
				b = pNode->DataSize == sizeof(UINT16) && pNode->uint16 == SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
				break;
			case SDP_TYPE_SEQUENCE:
				if (GetProtocolValue(pNode, ProtocolUuid, DataSize, value))
				{
					return TRUE;
				}
				break;
			}
		}
	}

	return FALSE;
}

void Dump(ELog& log, PSDP_NODE pNode);

BOOL GetProtocolValue(_In_ ELog& log, _In_ const UCHAR* pb, _In_ ULONG cb, _In_ UINT16 ProtocolUuid, _In_ ULONG DataSize, _Out_ PUSHORT value)
{
	BOOL b = FALSE;
	PSDP_NODE pNode;
	if (PVOID pv = SdpValidateStream(pb, cb, &pNode))
	{
		Dump(log, pNode);

		b = GetProtocolValue(pNode, ProtocolUuid, DataSize, value);

		LocalFree(pv);
	}

	return b;
}

_NT_END