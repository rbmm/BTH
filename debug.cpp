#include "stdafx.h"

_NT_BEGIN

#include "sdp.h"
#include "elog.h"

void Dump(ELog& log, PSDP_NODE pNode, PCSTR prefix)
{
	ULONG DataSize = pNode->DataSize;

	switch (pNode->Type)
	{
	default: __debugbreak();
		return;

	case SDP_TYPE_BOOLEAN:
		log(L"%S(%S)\r\n", prefix, pNode->boolean ? "true" : "false");
		[[fallthrough]];
	case SDP_TYPE_NIL:
		return;

	case SDP_TYPE_UINT:
	case SDP_TYPE_INT:

		log(L"%S(UINT%u:%x)\r\n", prefix, DataSize << 3, pNode->uint32);
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
			log(L"%S(UUID128:%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x)\r\n", 
				prefix, guid.Data1, guid.Data2, guid.Data3, 
				guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
				guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
			return;
		}

		log(L"%S(UUID%u:%x)\r\n", prefix, DataSize << 3, pNode->uint32);
		return;

	case SDP_TYPE_STRING:
	case SDP_TYPE_URL:
		log(L"%S(\"%.*S\")\r\n", prefix, DataSize, pNode->string);
		return;

	case SDP_TYPE_SEQUENCE:
	case SDP_TYPE_ALTERNATIVE:
		log(L"%S[ // %X\r\n", prefix--, DataSize);
		PLIST_ENTRY head = &pNode->sequence, entry = head;

		while ((entry = entry->Flink) != head)
		{
			Dump(log, static_cast<PSDP_NODE>(entry), prefix);
		}
		log(L"%S]\r\n", 1 + prefix);
		return;
	}
}

void Dump(ELog& log, PSDP_NODE pNode)
{
	char prefix[32];
	memset(prefix, '\t', _countof(prefix));
	prefix[_countof(prefix) - 1] = 0;

	Dump(log, pNode, prefix + _countof(prefix) - 1);
}

_NT_END