#include "stdafx.h"

_NT_BEGIN

#include "sdp.h"
#include "SdpNodeI.h"

struct __declspec(uuid("00112233-4455-6677-8899-aabbccddeeff")) MyServiceClass;


BOOL BuildSdp(USHORT Psm, UCHAR Cn)
{
	SDP_NODE Root, ClassList, DescriptorList, L2cap, l2_uuid, l2_psm, rfcomm, rf_uuid, rf_cn, guid, name, a1, a2, a3;

	AppendNode(AppendNode(InitNodeSequence(&L2cap), 
		InitNodeUuid(&l2_uuid, sizeof(UINT16), L2CAP_PROTOCOL_UUID16)),
		InitNodeUint(&l2_psm, sizeof(UINT16), Psm));

	AppendNode(AppendNode(InitNodeSequence(&rfcomm), 
		InitNodeUuid(&rf_uuid, sizeof(UINT16), RFCOMM_PROTOCOL_UUID16)), 
		InitNodeUint(&rf_cn, sizeof(UINT8), Cn));


	AppendNode(InitNodeSequence(&DescriptorList), &L2cap);
	//AppendNode(, &rfcomm);

	AppendNode(InitNodeSequence(&ClassList), InitNodeUuid(&guid, &__uuidof(MyServiceClass)));

	AppendNode(AppendNode(AppendNode(AppendNode(AppendNode(AppendNode(
		InitNodeSequence(&Root),
		InitNodeUint(&a1, sizeof(UINT16), SDP_ATTRIB_CLASS_ID_LIST)), &ClassList),
		InitNodeUint(&a2, sizeof(UINT16), SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST)), &DescriptorList), 
		InitNodeUint(&a3, sizeof(UINT16), SDP_ATTRIB_SERVICE_NAME)), InitNodeString(&name, "VSCR"));

	if (ULONG cb = GetSize(&Root))
	{
		PUCHAR pb = (PUCHAR)alloca(cb);
		if (NodeToStream(&Root, pb, cb))
		{
			ELog log;
			GetProtocolValue(log, pb, cb, L2CAP_PROTOCOL_UUID16, sizeof(UINT16), &Psm);
			GetProtocolValue(log, pb, cb, RFCOMM_PROTOCOL_UUID16, sizeof(UINT8), &Psm);
		}
	}
	return TRUE;
}

_NT_END