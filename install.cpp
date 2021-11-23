#include "stdafx.h"

_NT_BEGIN

#include <setupapi.h>
#include <bluetoothapis.h>

struct __declspec(uuid("ADF8EB1B-0718-4366-A418-BB88F175D360")) BTHECHOSAMPLE_SVC_GUID;

struct __declspec(uuid("ADF8EB1B-0718-4366-A418-BB88F175D361")) BTHECHOSAMPLE_CLI_GUID;

struct BLUETOOTH_SET_LOCAL_SERVICE_INFO : GUID {
	ULONG ulInstance;									//  An instance ID for the device node of the Plug and Play (PnP) ID.
	BOOL Enabled;										//  If TRUE, the enable the services
	BTH_ADDR btAddr;									//  If service is to be advertised for a particular remote device
	WCHAR szName[ BLUETOOTH_MAX_SERVICE_NAME_SIZE ];    //  SDP Service Name to be advertised.
	WCHAR szDeviceString[ BLUETOOTH_DEVICE_NAME_SIZE ]; //  Local device name (if any) like COM4 or LPT1
};

ULONG InstallInf(_In_ PCWSTR SourceInfFileName, _Out_ int* pi)
{
	*pi = -1;

	ULONG RequiredSize;
	WCHAR DestinationInfFileName[MAX_PATH];
	PWSTR DestinationInfFileNameComponent;
	ULONG dwError = SetupCopyOEMInfW(SourceInfFileName, 0, SPOST_NONE, 0, 
		DestinationInfFileName, _countof(DestinationInfFileName), &RequiredSize, 
		&DestinationInfFileNameComponent) ? NOERROR : GetLastError();

	if (dwError == NOERROR)
	{
		DbgPrint("%S\n%S\n", DestinationInfFileName, DestinationInfFileNameComponent);

		_wcslwr(DestinationInfFileNameComponent);

		if (DestinationInfFileNameComponent[0] == 'o' &&
			DestinationInfFileNameComponent[1] == 'e' &&
			DestinationInfFileNameComponent[2] == 'm')
		{
			ULONG i = wcstoul(DestinationInfFileNameComponent + 3, &DestinationInfFileNameComponent, 10);

			if (DestinationInfFileNameComponent[0] == '.' &&
				DestinationInfFileNameComponent[1] == 'i' &&
				DestinationInfFileNameComponent[2] == 'n' &&
				DestinationInfFileNameComponent[3] == 'f' )
			{
				*pi = i;
			}
		}
	}
	return dwError;
}

extern volatile const UCHAR guz;

ULONG SetupDrv(_In_ const GUID * pClassGuid, _In_ const _BLUETOOTH_LOCAL_SERVICE_INFO * SvcInfo)
{
	BOOLEAN we;
	RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, TRUE, FALSE, &we);
	C_ASSERT(sizeof(BLUETOOTH_SET_LOCAL_SERVICE_INFO)==0x420);

	CONFIGRET cr;
	ULONG cb = 0, rcb;
	union {
		PVOID buf;
		PZZWSTR Buffer;
	};

	PVOID stack = alloca(guz);
	do 
	{
		cr = CM_Get_Device_Interface_List_SizeW(&rcb, const_cast<GUID*>(&GUID_BTHPORT_DEVICE_INTERFACE), 0, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

		if (cr != CR_SUCCESS)
		{
			break;
		}

		if (cb < (rcb *= sizeof(WCHAR)))
		{
			cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
		}

		cr = CM_Get_Device_Interface_ListW(const_cast<GUID*>(&GUID_BTHPORT_DEVICE_INTERFACE), 
			0, Buffer, cb, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

	} while (cr == CR_BUFFER_SMALL);

	if (cr == CR_SUCCESS)
	{

		while (*Buffer)
		{
			HANDLE hFile = CreateFileW(Buffer, 0, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
			__debugbreak();
			if (hFile != INVALID_HANDLE_VALUE)
			{
				BluetoothSetLocalServiceInfo(hFile, pClassGuid, 0, SvcInfo);
				NtClose(hFile);
			}
			Buffer += wcslen(Buffer) + 1;
		}
	}

	return cr;
}

#define IOCTL_L2CA_OPEN_CHANNEL CTL_CODE(FILE_DEVICE_BLUETOOTH, BRB_L2CA_OPEN_CHANNEL, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct __declspec(uuid("11112222-3333-4444-5555-666677778888")) TestItf;

ULONG OpenMyDevice(PHANDLE phFile)
{
	CONFIGRET cr;
	ULONG cb = 0, rcb;
	union {
		PVOID buf;
		PZZWSTR Buffer;
	};

	PVOID stack = alloca(guz);
	do 
	{
		cr = CM_Get_Device_Interface_List_SizeW(&rcb, const_cast<GUID*>(&__uuidof(TestItf)), 0, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

		if (cr != CR_SUCCESS)
		{
			break;
		}

		if (cb < (rcb *= sizeof(WCHAR)))
		{
			cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
		}

		cr = CM_Get_Device_Interface_ListW(const_cast<GUID*>(&__uuidof(TestItf)), 
			0, Buffer, cb, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

	} while (cr == CR_BUFFER_SMALL);

	if (cr == CR_SUCCESS)
	{
		while (*Buffer)
		{
			HANDLE hFile = CreateFileW(Buffer, 0, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
			__debugbreak();
			if (hFile != INVALID_HANDLE_VALUE)
			{
				*phFile = hFile;
				return NOERROR;
			}
			Buffer += wcslen(Buffer) + 1;
		}

		return ERROR_GEN_FAILURE;
	}

	return cr;
}

VOID NTAPI OnOpenComplete (
						   _In_ PVOID ApcContext,
						   _In_ PIO_STATUS_BLOCK IoStatusBlock,
						   _In_ ULONG /*Reserved*/
						   )
{
	__debugbreak();
	DbgPrint("OnOpenComplete(%p %x %p)\n", ApcContext, IoStatusBlock->Status, IoStatusBlock->Information);
}

void Install()
{
	__debugbreak();
	_BLUETOOTH_LOCAL_SERVICE_INFO SvcInfo = { TRUE, {}, L"BthCli", L"Client L2CAP"};

	int i;
	HANDLE hKey;
	STATIC_OBJECT_ATTRIBUTES(oa, "\\registry\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\BthCli");
	STATIC_UNICODE_STRING_(INF);

	if (NOERROR == InstallInf(L"C:\\echo\\BthCli.inf", &i))
	{
		if (0 <= ZwCreateKey(&hKey, KEY_WRITE, &oa, 0, 0, 0, 0))
		{

			ZwSetValueKey(hKey, &INF, 0, REG_DWORD, &i, sizeof(i));
			NtClose(hKey);
		}

		SetupDrv(&__uuidof(BTHECHOSAMPLE_CLI_GUID), &SvcInfo);
	}

	HANDLE hFile;

	if (!OpenMyDevice(&hFile))
	{
		_BRB_L2CA_OPEN_CHANNEL OpenChannel;
		OpenChannel.BtAddress = 0xACBC3293227C;
		OpenChannel.Psm = PSM_RFCOMM;
		IO_STATUS_BLOCK iosb;
		NtDeviceIoControlFile(hFile, 0, OnOpenComplete, &OpenChannel, &iosb, IOCTL_L2CA_OPEN_CHANNEL, 
			&OpenChannel, sizeof(OpenChannel), &OpenChannel, sizeof(OpenChannel));
		SleepEx(INFINITE, TRUE);
		NtClose(hFile);
	}

	__debugbreak();
	SvcInfo.Enabled = FALSE;
	SetupDrv(&__uuidof(BTHECHOSAMPLE_CLI_GUID), &SvcInfo);

	if (0 <= ZwOpenKey(&hKey, KEY_READ, &oa))
	{
		KEY_VALUE_PARTIAL_INFORMATION kvpi;
		NTSTATUS status = ZwQueryValueKey(hKey, &INF, KeyValuePartialInformation, &kvpi, sizeof(kvpi), &kvpi.TitleIndex);
		NtClose(hKey);

		if (0 <= status && kvpi.Type == REG_DWORD && kvpi.DataLength == sizeof(DWORD))
		{
			WCHAR oem_inf[32];
			swprintf_s(oem_inf, _countof(oem_inf), L"oem%u.inf", (ULONG&)kvpi.Data);
			SetupUninstallOEMInfW(L"oem_inf", SUOI_FORCEDELETE, 0);
		}
	}
	GetLastError();
}

_NT_END