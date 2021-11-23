#include "stdafx.h"

_NT_BEGIN

ULONG RegisterService(_In_ UCHAR Port, _In_ WSAESETSERVICEOP essoperation, _Inout_ HANDLE* pRecordHandle)
{
	const static UCHAR SdpRecordTmplt[] = {
		// [//////////////////////////////////////////////////////////////////////////////////////////////////////////
		0x35, 0x33,																									//
		//
		// UINT16:SDP_ATTRIB_CLASS_ID_LIST																			//
		0x09, 0x00, 0x01,																							//
		//		[/////////////////////////////////////////////////////////////////////////////////////////////////	//
		0x35, 0x11,																								//	//
		// UUID128:{guid}																						//	//
		0x1c, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,	//	//
		//		]/////////////////////////////////////////////////////////////////////////////////////////////////	//
		//
		// UINT16:SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST																//
		0x09, 0x00, 0x04,																							//
		//		[/////////////////////////////////																	//
		0x35, 0x0f,								//																	//
		// (L2CAP, PSM=PSM_RFCOMM)				//																	//
		//			[/////////////////////////	//																	//
		0x35, 0x06,							//	//																	//
		// UUID16:L2CAP_PROTOCOL_UUID16		//	//																	//
		0x19, 0x01, 0x00,					//	//																	//
		// UINT16:PSM_RFCOMM				//	//																	//
		0x09, 0x00, 0x03,					//	//																	//
		//			]/////////////////////////	//																	//
		// (RFCOMM, CN=Port)					//																	//
		//			[/////////////////////////	//																	//
		0x35, 0x05,							//	//																	//
		// UUID16:RFCOMM_PROTOCOL_UUID16	//	//																	//
		0x19, 0x00, 0x03,					//	//																	//
		// UINT8:CN							//	//																	//
		0x08, 0x33,							//	//																	//
		//			]/////////////////////////	//																	//
		//		]/////////////////////////////////																	//
		//
		// UINT16:SDP_ATTRIB_SERVICE_NAME																			//
		0x09, 0x01, 0x00,																							//
		// STR:4 "VSCR"																								//
		0x25, 0x04, 0x56, 0x53, 0x43, 0x52																			//
		// ]//////////////////////////////////////////////////////////////////////////////////////////////////////////
	};

	enum { bth_set_size = FIELD_OFFSET(BTH_SET_SERVICE, pRecord) + sizeof(SdpRecordTmplt) };

	ULONG SdpVersion = BTH_SDP_VERSION;

	PBTH_SET_SERVICE bss = (PBTH_SET_SERVICE)alloca(bth_set_size);

	bss->pSdpVersion = &SdpVersion;
	bss->pRecordHandle = pRecordHandle;
	bss->fCodService = 0;
	bss->ulRecordLength = sizeof(SdpRecordTmplt);
	RtlZeroMemory(bss->Reserved, sizeof(bss->Reserved));
	memcpy(bss->pRecord, SdpRecordTmplt, sizeof(SdpRecordTmplt));

	//memcpy(bss->pRecord + 8, &__uuidof(MyServiceClass), sizeof(GUID));
	//bss->pRecord[35] = Psm >> 8;
	//bss->pRecord[36] = (UCHAR)Psm;
	bss->pRecord[43] = Port;

	BLOB Blob = { bth_set_size, (PBYTE)bss };

	WSAQUERYSETW wsaQuerySet = {
		sizeof(WSAQUERYSET),
		0,//const_cast<PWSTR>(L"VSCR"),
		0,//const_cast<PGUID>(&__uuidof(MyServiceClass)),
		{},
		0,
		NS_BTH,
		0,0,0,0,0,
		0,//1
		0,//&AddrInfo
		0,
		&Blob
	};

	return WSASetServiceW(&wsaQuerySet, essoperation, 0) == 0 ? NOERROR : GetLastError();
}

_NT_END