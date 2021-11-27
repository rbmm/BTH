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

NTSTATUS AddRegistry(_In_ const GUID * pClassGuid, _In_ const _BLUETOOTH_LOCAL_SERVICE_INFO * SvcInfo)
{
	WCHAR sz[128];
	swprintf_s(sz, _countof(sz), 
		L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\BTHPORT\\Parameters\\LocalServices\\{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", 
		pClassGuid->Data1, pClassGuid->Data2, pClassGuid->Data3,
		pClassGuid->Data4[0], pClassGuid->Data4[1], pClassGuid->Data4[2], pClassGuid->Data4[3], 
		pClassGuid->Data4[4], pClassGuid->Data4[5], pClassGuid->Data4[6], pClassGuid->Data4[7]);

	UNICODE_STRING usGuid;
	RtlInitUnicodeString(&usGuid, sz);

	STATIC_UNICODE_STRING(_0_, "0");
	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &usGuid };

	NTSTATUS status = ZwCreateKey(&oa.RootDirectory, KEY_ALL_ACCESS, &oa, 0, 0, 0, 0);

	if (0 <= status)
	{
		oa.ObjectName = const_cast<PUNICODE_STRING>(&_0_);
		HANDLE hKey = 0;
		STATIC_UNICODE_STRING_(PnpInstanceCounter);
		static const ULONG Counter = 1, Instance = 0;

		0 <= (status = ZwSetValueKey(oa.RootDirectory, 
			const_cast<PUNICODE_STRING>(&PnpInstanceCounter), 0, REG_DWORD, const_cast<ULONG*>(&Counter), sizeof(ULONG))) &&
			0 <= (status = ZwCreateKey(&hKey, KEY_ALL_ACCESS, &oa, 0, 0, 0, 0));

		NtClose(oa.RootDirectory);

		if (0 <= status)
		{
			STATIC_UNICODE_STRING_(Enabled);
			STATIC_UNICODE_STRING_(ServiceName);
			STATIC_UNICODE_STRING_(DeviceString);
			STATIC_UNICODE_STRING_(AssocBdAddr);
			STATIC_UNICODE_STRING_(PnpInstance);

			0 <= (status = ZwSetValueKey(hKey, const_cast<PUNICODE_STRING>(&Enabled), 0, REG_DWORD, (void*)&SvcInfo->Enabled, sizeof(ULONG))) &&
				0 <= (status = ZwSetValueKey(hKey, const_cast<PUNICODE_STRING>(&PnpInstance), 0, REG_DWORD, const_cast<ULONG*>(&Instance), sizeof(ULONG))) &&
				0 <= (status = ZwSetValueKey(hKey, const_cast<PUNICODE_STRING>(&AssocBdAddr), 0, REG_BINARY, (void*)&SvcInfo->btAddr, sizeof(BTH_ADDR))) &&
				0 <= (status = ZwSetValueKey(hKey, const_cast<PUNICODE_STRING>(&ServiceName), 0, REG_SZ, const_cast<PWSTR>(SvcInfo->szName), (1 + (ULONG)wcslen(SvcInfo->szName)) *sizeof(WCHAR))) &&
				0 <= (status = ZwSetValueKey(hKey, const_cast<PUNICODE_STRING>(&DeviceString), 0, REG_SZ, const_cast<PWSTR>(SvcInfo->szDeviceString), (1 + (ULONG)wcslen(SvcInfo->szDeviceString)) *sizeof(WCHAR)));

			NtClose(hKey);
		}
	}

	return status;
}

HRESULT SetupDrv(_In_ const GUID * pClassGuid, _In_ const _BLUETOOTH_LOCAL_SERVICE_INFO * SvcInfo)
{
	NTSTATUS status = AddRegistry(pClassGuid, SvcInfo);
	if (0 > status)
	{
		return HRESULT_FROM_NT(status);
	}
	BOOLEAN we;
	RtlAdjustPrivilege(SE_LOAD_DRIVER_PRIVILEGE, TRUE, FALSE, &we);
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
				/*ULONG dwError = */BluetoothSetLocalServiceInfo(hFile, pClassGuid, 0, SvcInfo);
				NtClose(hFile);

			}
			Buffer += wcslen(Buffer) + 1;
		}
	}

	return cr;
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