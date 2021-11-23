#include "stdafx.h"

_NT_BEGIN

#include "sdp.h"
#include "SdpNodeI.h"

ULONG GetSize(PSDP_NODE pNode)
{
	switch (pNode->Type)
	{
	case SDP_TYPE_NIL:
		return 1;

	case SDP_TYPE_INT:
	case SDP_TYPE_UINT:
	case SDP_TYPE_UUID:
	case SDP_TYPE_BOOLEAN:
		return 1 + pNode->DataSize;

	case SDP_TYPE_STRING:
	case SDP_TYPE_URL:
	case SDP_TYPE_SEQUENCE:
	case SDP_TYPE_ALTERNATIVE:
		ULONG DataSize = pNode->DataSize;
		if (DataSize < 0x100)
		{
			return DataSize + 2;
		}
		if (DataSize < 0x10000)
		{
			return DataSize + 3;
		}
		return DataSize + 4;
	}
	return 0;
}

PSDP_NODE AppendNode(PSDP_NODE pParentNode, PSDP_NODE pNode)
{
	switch (pParentNode->Type)
	{
	case SDP_TYPE_SEQUENCE:
	case SDP_TYPE_ALTERNATIVE:
		if (ULONG DataSize = GetSize(pNode))
		{
			InsertTailList(&pParentNode->sequence, pNode);
			pParentNode->DataSize += DataSize;
			return pParentNode;
		}
	}

	__debugbreak();
	return 0;
}

PSDP_NODE InitNodeUuid(PSDP_NODE pNode, ULONG size, ULONG value)
{
	pNode->Type = SDP_TYPE_UUID;
	pNode->DataSize = size;

	switch (size)
	{
	case 2:
		pNode->uuid32 = _byteswap_ushort((USHORT)value);
		break;
	case 4:
		pNode->uuid32 = _byteswap_ulong(value);
		break;
	default:
		__debugbreak();
		return 0;
	}

	return pNode;
}

PSDP_NODE InitNodeUuid(PSDP_NODE pNode, LPCGUID guid)
{
	pNode->Type = SDP_TYPE_UUID;
	pNode->DataSize = sizeof(GUID);
	pNode->uuid128.Data1 = _byteswap_ulong(guid->Data1);
	pNode->uuid128.Data2 = _byteswap_ushort(guid->Data2);
	pNode->uuid128.Data3 = _byteswap_ushort(guid->Data3);
	memcpy(&pNode->uuid128.Data4, guid->Data4, sizeof(guid->Data4));
	return pNode;
}

PSDP_NODE InitNodeUint(PSDP_NODE pNode, ULONG size, ULONG64 value)
{
	pNode->Type = SDP_TYPE_UINT;
	pNode->DataSize = size;
	switch (size)
	{
	case 1:
		pNode->uint8 = (UCHAR)value;
		break;
	case 2:
		pNode->uint16 = _byteswap_ushort((USHORT)value);
		break;
	case 4:
		pNode->uint32 = _byteswap_ulong((ULONG)value);
		break;
	case 8:
		pNode->uint64 = _byteswap_uint64(value);
		break;
	default:
		__debugbreak();
		return 0;
	}
	return pNode;
}

PSDP_NODE InitNodeBool(PSDP_NODE pNode, BOOLEAN b)
{
	pNode->Type = SDP_TYPE_BOOLEAN;
	pNode->DataSize = 1;
	pNode->boolean = b;
	return pNode;
}

PSDP_NODE InitNodeString(PSDP_NODE pNode, PCSTR string)
{
	pNode->Type = SDP_TYPE_STRING;
	pNode->DataSize = (ULONG)strlen(string);
	pNode->string = const_cast<PSTR>(string);
	return pNode;
}

PSDP_NODE InitNodeSequence(PSDP_NODE pNode)
{
	pNode->Type = SDP_TYPE_SEQUENCE;
	pNode->DataSize = 0;
	InitializeListHead(&pNode->sequence);
	return pNode;
}

PBYTE NodeToStream(PSDP_NODE pNode, PBYTE pb, ULONG cb)
{
	ULONG len = GetSize(pNode);

	if (cb < len)
	{
		return 0;
	}

	SdpTag tag;

	ULONG DataSize = pNode->DataSize;
	ULONG Type = pNode->Type;
	ULONG Size = 0;

	switch (Type)
	{
	case SDP_TYPE_NIL:
	case SDP_TYPE_BOOLEAN:
		Size = 0;
		break;

	case SDP_TYPE_INT:
	case SDP_TYPE_UINT:
	case SDP_TYPE_UUID:
		switch (DataSize)
		{
		case 1:
			Size = 0;
			break;
		case 2:
			Size = 1;
			break;
		case 4:
			Size = 2;
			break;
		case 8:
			Size = 3;
			break;
		case 16:
			Size = 4;
			break;
		default: __debugbreak();
		}
		break;

	case SDP_TYPE_STRING:
	case SDP_TYPE_URL:
	case SDP_TYPE_SEQUENCE:
	case SDP_TYPE_ALTERNATIVE:
		if (len < 0x100)
		{
			Size = 5;
			break;
		}
		if (len < 0x10000)
		{
			Size = 6;
			break;
		}
		if (len < 0x1000000)
		{
			Size = 7;
			break;
		}
		__debugbreak();
		break;
	default: __debugbreak();
		return 0;
	}

	tag.Size = Size;
	tag.Type = Type;

	*pb++ = tag.Tag, --cb;

	switch (Type)
	{
	default: __assume(false);
	case SDP_TYPE_BOOLEAN:
	case SDP_TYPE_INT:
	case SDP_TYPE_UINT:
	case SDP_TYPE_UUID:
		memcpy(pb, pNode->Data, DataSize);
		return pb + DataSize;

	case SDP_TYPE_STRING:
	case SDP_TYPE_URL:
		len = DataSize;
		if (DataSize >= 0x10000)
		{
			*pb++ = (UCHAR)(DataSize >> 16), --cb;
			DataSize &= 0xFFFF;
		}
		if (DataSize >= 0x100)
		{
			*pb++ = (UCHAR)(DataSize >> 8), --cb;
		}
		*pb++ = (UCHAR)DataSize, --cb;

		memcpy(pb, pNode->string, len);
		return pb + len;

	case SDP_TYPE_SEQUENCE:
	case SDP_TYPE_ALTERNATIVE:
		if (DataSize >= 0x10000)
		{
			*pb++ = (UCHAR)(DataSize >> 16), --cb;
			DataSize &= 0xFFFF;
		}
		if (DataSize >= 0x100)
		{
			*pb++ = (UCHAR)(DataSize >> 8), --cb;
		}
		*pb++ = (UCHAR)DataSize, --cb;

		PLIST_ENTRY head = &pNode->sequence, entry = head;
		while ((entry = entry->Flink) != head)
		{
			if (PUCHAR pbNew = NodeToStream(static_cast<PSDP_NODE>(entry), pb, cb))
			{
				cb -= RtlPointerToOffset(pb, pbNew), pb = pbNew;
			}
			else
			{
				return 0;
			}
		}
		return pb;
	}
}

_NT_END