#include "stdafx.h"

_NT_BEGIN

#include "util.h"

HRESULT GetLastErrorEx(ULONG dwError/* = GetLastError()*/)
{
	NTSTATUS status = RtlGetLastNtStatus();
	return dwError == RtlNtStatusToDosErrorNoTeb(status) ? HRESULT_FROM_NT(status) : HRESULT_FROM_WIN32(dwError);
}

NTSTATUS SyncIoctl(_In_ HANDLE FileHandle, 
				   _In_ ULONG IoControlCode,
				   _In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
				   _In_ ULONG InputBufferLength,
				   _Out_writes_bytes_opt_(OutputBufferLength) PVOID OutputBuffer,
				   _In_ ULONG OutputBufferLength,
				   _Out_opt_ PULONG_PTR Information)
{
	IO_STATUS_BLOCK iosb;

	NTSTATUS status = NtDeviceIoControlFile(FileHandle, 0, 0, 0, &iosb, IoControlCode, 
		InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);

	if (status == STATUS_PENDING)
	{
		if (WaitForSingleObject(FileHandle, INFINITE) != WAIT_OBJECT_0) __debugbreak();
		status = iosb.Status;
	}

	if (Information)
	{
		*Information = iosb.Information;
	}

	return status;
}

NTSTATUS RegisterService(_In_ HANDLE hDevice, _In_ LPCGUID guid, _In_ UCHAR Port, _Out_ PHANDLE_SDP phRecord)
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

	UCHAR SdpRecord[sizeof(SdpRecordTmplt)];

	memcpy(SdpRecord, SdpRecordTmplt, sizeof(SdpRecordTmplt));
	SdpRecord[43] = Port;

	PGUID pg = (PGUID)(SdpRecord + 8);
	pg->Data1 = _byteswap_ulong(guid->Data1);
	pg->Data2 = _byteswap_ushort(guid->Data2);
	pg->Data3 = _byteswap_ushort(guid->Data3);
	memcpy(pg->Data4, guid->Data4, sizeof(GUID::Data4));

	return SyncIoctl(hDevice, IOCTL_BTH_SDP_SUBMIT_RECORD, SdpRecord, sizeof(SdpRecord), phRecord, sizeof(HANDLE_SDP));
}

NTSTATUS DoHash(PUCHAR pbData, ULONG cbData, PUCHAR pbOutput, ULONG cbOutput, PCWSTR pszAlgId)
{
	NTSTATUS status;
	BCRYPT_HASH_HANDLE hHash;
	BCRYPT_ALG_HANDLE hAlgorithm;

	if (0 <= (status = BCryptOpenAlgorithmProvider(&hAlgorithm, pszAlgId, 0, 0)))
	{
		status = BCryptCreateHash(hAlgorithm, &hHash, 0, 0, 0, 0, 0);

		BCryptCloseAlgorithmProvider(hAlgorithm, 0);

		if (0 <= status)
		{
			0 <= (status = BCryptHashData(hHash, pbData, cbData, 0)) &&
				0 <= (status = BCryptFinishHash(hHash, pbOutput, cbOutput, 0));

			BCryptDestroyHash(hHash);
		}
	}

	return status;
}

int ShowErrorBox(HWND hwnd, HRESULT dwError, PCWSTR pzCaption, UINT uType)
{
	int r = IDCLOSE;

	PWSTR psz;
	ULONG dwFlags, errType = uType & MB_ICONMASK;
	HMODULE hmod;	

	if (dwError & FACILITY_NT_BIT)
	{
		dwError &= ~FACILITY_NT_BIT;
		static HMODULE s_hmod;
		if (!s_hmod)
		{
			s_hmod = GetModuleHandle(L"ntdll");
		}
		hmod = s_hmod;
		dwFlags = FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE;

		if (!errType)
		{
			static const UINT s_errType[] = { MB_ICONINFORMATION, MB_ICONINFORMATION, MB_ICONWARNING, MB_ICONERROR };
			uType |= s_errType[(ULONG)dwError >> 30];
		}
	}
	else
	{
		hmod = 0;
		dwFlags = FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM;
		if (!errType)
		{
			uType |= dwError ? MB_ICONERROR : MB_ICONINFORMATION;
		}
	}

	if (FormatMessageW(dwFlags, hmod, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (PWSTR)&psz, 0, 0))
	{
		r = MessageBoxW(hwnd, psz, pzCaption, uType);
		LocalFree(psz);
	}

	return r;
}

void FixBase64(PWSTR pszString, ULONG cch)
{
	if ( cch )
	{
		do 
		{
			if (*pszString == '/')
			{
				*pszString = '-';
			}
		} while (pszString++, --cch);
	}
}

void UnFixBase64(PWSTR pszString, ULONG cch)
{
	if ( cch )
	{
		do 
		{
			if (*pszString == '-')
			{
				*pszString = '/';
			}
		} while (pszString++, --cch);
	}
}

#if 0
ULONG BthRegisterService(LPCGUID lpServiceClassId, PCWSTR ServiceName, PSOCKADDR_BTH asi)
{
	ULONG SdpVersion = BTH_SDP_VERSION;
	HANDLE hRecord;
	BTH_SET_SERVICE bss = { &SdpVersion, &hRecord };

	BLOB blob = { sizeof(bss), (PUCHAR)&bss };

	CSADDR_INFO csa = { { (PSOCKADDR)asi, sizeof(SOCKADDR_BTH) }, {}, SOCK_STREAM, BTHPROTO_RFCOMM };

	WSAQUERYSET wqs = { sizeof(wqs) };
	wqs.lpszServiceInstanceName = const_cast<PWSTR>(ServiceName); // not optional !
	wqs.lpServiceClassId = const_cast<PGUID>(lpServiceClassId);
	wqs.dwNameSpace = NS_BTH;
	wqs.dwNumberOfCsAddrs = 1;
	wqs.lpcsaBuffer = &csa;
	//wqs.lpBlob = &blob;

	return WSASetService(&wqs, RNRSERVICE_REGISTER, 0) ? WSAGetLastError() : NOERROR;
}

struct __declspec(uuid("00112233-4455-6677-8899-aabbccddeeff")) MyServiceClass;

void testR()
{
	SOCKADDR_BTH asi = { AF_BTH, 0, {}, BT_PORT_ANY };
	SOCKET s = WSASocket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM, 0, 0, WSA_FLAG_NO_HANDLE_INHERIT|WSA_FLAG_OVERLAPPED);
	bind(s, (PSOCKADDR)&asi, sizeof(asi));
	int len = sizeof(asi);
	getsockname(s, (PSOCKADDR)&asi, &len);
	BthRegisterService(&__uuidof(MyServiceClass), L"ABCD", &asi);
}
#endif

_NT_END