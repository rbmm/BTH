#include "stdafx.h"

_NT_BEGIN

#include "util.h"

extern "C"
{
	extern UCHAR cert_begin[];
	extern UCHAR cert_end[];

	extern UCHAR inf_begin[];
	extern UCHAR inf_end[];
	extern UCHAR cat_begin[];
	extern UCHAR cat_end[];

	extern UCHAR codesec64_kb_begin[];
	extern UCHAR codesec64_kb_end[];
};

NTSTATUS DropFile(POBJECT_ATTRIBUTES poa, PVOID buf, ULONG cb)
{
	HANDLE hFile;
	IO_STATUS_BLOCK iosb;
	NTSTATUS status = NtCreateFile(&hFile, FILE_APPEND_DATA | SYNCHRONIZE, poa, &iosb, 0,
		0, 0, FILE_OVERWRITE_IF, FILE_SYNCHRONOUS_IO_NONALERT, 0, 0);

	if (0 <= status)
	{
		status = NtWriteFile(hFile, 0, 0, 0, &iosb, buf, cb, 0, 0);

		NtClose(hFile);
	}

	return status;
}

NTSTATUS AdjustPrivileges()
{
	HANDLE hToken;

	NTSTATUS status = NtOpenProcessToken(NtCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken);
	if (0 <= status)
	{
		BEGIN_PRIVILEGES(tp, 3)
			LAA(SE_BACKUP_PRIVILEGE),
			LAA(SE_RESTORE_PRIVILEGE),
			LAA(SE_LOAD_DRIVER_PRIVILEGE)
		END_PRIVILEGES	
		status = NtAdjustPrivilegesToken(hToken, FALSE, const_cast<PTOKEN_PRIVILEGES>(&tp), 0, 0, 0);
		NtClose(hToken);
	}
	return status;
}

struct __declspec(uuid("ADF8EB1B-0718-4366-A418-BB88F175D361")) BTH_CLI_GUID;

STATIC_OBJECT_ATTRIBUTES(oa_BthCli,   "\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\BTHCLI");
STATIC_OBJECT_ATTRIBUTES(oa_LocalSrv, "\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\BTHPORT\\Parameters\\LocalServices\\{ADF8EB1B-0718-4366-A418-BB88F175D361}");
STATIC_OBJECT_ATTRIBUTES(oa_Enum,     "\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Enum\\BTHENUM\\{adf8eb1b-0718-4366-a418-bb88f175d361}_LOCALMFG&0000");
STATIC_OBJECT_ATTRIBUTES(oa_Class,    "\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class");
STATIC_OBJECT_ATTRIBUTES(oa_drv, "\\systemroot\\system32\\drivers\\BthCli.sys");
STATIC_UNICODE_STRING_(INF);

extern volatile const UCHAR guz;

struct BLUETOOTH_SET_LOCAL_SERVICE_INFO : GUID {
	ULONG ulInstance;									//  An instance ID for the device node of the Plug and Play (PnP) ID.
	BOOL Enabled;										//  If TRUE, the enable the services
	BTH_ADDR btAddr;									//  If service is to be advertised for a particular remote device
	WCHAR szName[ BLUETOOTH_MAX_SERVICE_NAME_SIZE ];    //  SDP Service Name to be advertised.
	WCHAR szDeviceString[ BLUETOOTH_DEVICE_NAME_SIZE ]; //  Local device name (if any) like COM4 or LPT1
};

HRESULT LoadUnloadDriver(_In_ const BLUETOOTH_SET_LOCAL_SERVICE_INFO * SvcInfo)
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
		cr = CM_Get_Device_Interface_List_SizeW(&rcb, const_cast<GUID*>(&GUID_BTHPORT_DEVICE_INTERFACE), 0, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

		if (cr != CR_SUCCESS)
		{
			break;
		}

		if (rcb < 3)
		{
			return S_OK;
		}

		if (cb < (rcb *= sizeof(WCHAR)))
		{
			cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
		}

		cr = CM_Get_Device_Interface_ListW(const_cast<GUID*>(&GUID_BTHPORT_DEVICE_INTERFACE), 
			0, Buffer, cb, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

	} while (cr == CR_BUFFER_SMALL);

	if (cr != CR_SUCCESS)
	{
		return HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_BAD_UNIT));
	}

	HRESULT hr = ERROR_BAD_UNIT;

	while (*Buffer)
	{
		HANDLE hFile = CreateFileW(Buffer, 0, 0, 0, OPEN_EXISTING, 0, 0);

		if (hFile != INVALID_HANDLE_VALUE)
		{
			IO_STATUS_BLOCK iosb;
			
			hr = NtDeviceIoControlFile(hFile, 0, 0, 0, &iosb, IOCTL_BTH_SET_LOCALSERVICEINFO,
				(void*)SvcInfo, sizeof(BLUETOOTH_SET_LOCAL_SERVICE_INFO), 0, 0);
			
			NtClose(hFile);

			if (0 <= hr)
			{
				return S_OK;
			}

			hr |= FACILITY_NT_BIT;
		}
		else
		{
			hr = GetLastErrorEx();
		}

		Buffer += wcslen(Buffer) + 1;
	}

	return hr;
}

HRESULT UnloadDriver()
{
	BLUETOOTH_SET_LOCAL_SERVICE_INFO SvcInfo = { {__uuidof(BTH_CLI_GUID)} };
	return LoadUnloadDriver(&SvcInfo);
}

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

void UninstallInf(int i)
{
	WCHAR oem_inf[32];
	swprintf_s(oem_inf, _countof(oem_inf), L"oem%u.inf", i);
	SetupUninstallOEMInfW(oem_inf, SUOI_FORCEDELETE, 0);
}

NTSTATUS AddRegistry(_In_ const BLUETOOTH_SET_LOCAL_SERVICE_INFO * SvcInfo)
{
	STATIC_UNICODE_STRING(_0_, "0");
	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, const_cast<PUNICODE_STRING>(&_0_) };

	NTSTATUS status = ZwCreateKey(&oa.RootDirectory, KEY_ALL_ACCESS, &oa_LocalSrv, 0, 0, 0, 0);

	if (0 <= status)
	{
		HANDLE hKey = 0;
		STATIC_UNICODE_STRING_(PnpInstanceCounter);
		static const ULONG Instance = 0;

		0 <= (status = ZwSetValueKey(oa.RootDirectory, 
			const_cast<PUNICODE_STRING>(&PnpInstanceCounter), 0, REG_DWORD, const_cast<ULONG*>(&Instance), sizeof(ULONG))) &&
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

void SaveOemInfIndex(int i)
{
	HANDLE hKey;

	if (0 <= ZwCreateKey(&hKey, KEY_WRITE, &oa_BthCli, 0, 0, 0, 0))
	{
		ZwSetValueKey(hKey, &INF, 0, REG_DWORD, &i, sizeof(i));
		NtClose(hKey);
	}
}

BOOL GetKeyInfo(HANDLE hKey, ULONG& SubKeys, ULONG& MaxNameLen)
{
	KEY_FULL_INFORMATION kfi;

	switch (ZwQueryKey(hKey, KeyFullInformation, &kfi, sizeof(kfi), &kfi.TitleIndex))
	{
	case 0:
	case STATUS_BUFFER_OVERFLOW:
		SubKeys = kfi.SubKeys, MaxNameLen = kfi.MaxNameLen;
		return TRUE;
	}

	return FALSE;
}

NTSTATUS DeleteKey(PCOBJECT_ATTRIBUTES poa, ULONG Level = 0, BOOL (*fn)(PCUNICODE_STRING ObjectName, HANDLE hKey, ULONG Level, PVOID Context) = 0, PVOID Context = 0)
{
	UNICODE_STRING ObjectName;

	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName, OBJ_CASE_INSENSITIVE };

	NTSTATUS status = ZwOpenKeyEx(&oa.RootDirectory, KEY_READ|DELETE|KEY_WOW64_64KEY, const_cast<POBJECT_ATTRIBUTES>(poa), REG_OPTION_BACKUP_RESTORE);

	DbgPrint("open:[%x] \"%wZ\" = %x\n", Level, poa->ObjectName, status);

	if (0 <= status)
	{
		if (!fn || fn(poa->ObjectName, oa.RootDirectory, Level, Context))
		{
			ULONG SubKeys, MaxNameLen;

			if (GetKeyInfo(oa.RootDirectory, SubKeys, MaxNameLen) && SubKeys)
			{
				ObjectName.MaximumLength = (USHORT)MaxNameLen;

				if (PKEY_BASIC_INFORMATION pkni = (PKEY_BASIC_INFORMATION)_malloca(
					MaxNameLen += FIELD_OFFSET(KEY_BASIC_INFORMATION, Name)))
				{
					ObjectName.Buffer = pkni->Name;

					Level++;

					do 
					{
						if (0 <= ZwEnumerateKey(oa.RootDirectory, 
							--SubKeys, KeyBasicInformation, pkni, MaxNameLen, (PULONG)&status))
						{
							ObjectName.Length = (USHORT)pkni->NameLength;
							DeleteKey(&oa, Level, fn, Context);
						}

					} while (SubKeys);

					--Level;

					_freea(pkni);
				}
			}

			status = ZwDeleteKey(oa.RootDirectory);
			DbgPrint("delete:[%x] \"%wZ\" = %x\n", Level, poa->ObjectName, status);
		}

		NtClose(oa.RootDirectory);
	}

	return status;
}

#include "util.h"

HRESULT Install()
{
	HRESULT hr = AdjustPrivileges();
	if (hr)
	{
		return HRESULT_FROM_NT(hr);
	}

	if (HCERTSTORE hCertStore = CertOpenStore(CERT_STORE_PROV_SYSTEM, X509_ASN_ENCODING,
		0, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"ROOT"))
	{
		hr = BOOL_TO_ERROR(CertAddEncodedCertificateToStore(hCertStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 
			cert_begin, RtlPointerToOffset(cert_begin, cert_end), CERT_STORE_ADD_REPLACE_EXISTING, 0));

		CertCloseStore(hCertStore, 0);
	}
	else
	{
		hr = GetLastError();
	}

	if (hr == NOERROR)
	{
		hr = DropFile(&oa_drv, codesec64_kb_begin, RtlPointerToOffset(codesec64_kb_begin, codesec64_kb_end));

		if (0 > hr)
		{
			hr |= FACILITY_NT_BIT;
		}
		else
		{
			WCHAR inf[MAX_PATH];
			if (ULONG cch = GetTempPathW(_countof(inf), inf))
			{
				if (cch < _countof(inf))
				{
					if (!wcscpy_s(inf + cch, _countof(inf) - cch, L"\\BthCli.inf"))
					{
						ULONG f = FACILITY_NT_BIT;
						UNICODE_STRING ObjectName;
						PWSTR FileName;
						if (0 <= (hr = RtlDosPathNameToNtPathName_U_WithStatus(inf, &ObjectName, &FileName, 0)))
						{
							OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName };

							if (0 <= (hr = DropFile(&oa, inf_begin, RtlPointerToOffset(inf_begin, inf_end))))
							{
								wcscpy(FileName + _countof("BthCli"), L"cat");

								if (0 <= (hr = DropFile(&oa, cat_begin, RtlPointerToOffset(cat_begin, cat_end))))
								{
									f = 0;
									int i;

									if (NOERROR == (hr = InstallInf(inf, &i)))
									{
										SaveOemInfIndex(i);

										BLUETOOTH_SET_LOCAL_SERVICE_INFO SvcInfo = { 
											{ __uuidof(BTH_CLI_GUID) }, 0, TRUE, {}, L"BthCli", L"szDeviceString"
										};

										f = FACILITY_NT_BIT;

										if (0 <= (hr = AddRegistry(&SvcInfo)))
										{
											f = 0;
											hr = LoadUnloadDriver(&SvcInfo);
										}

										if (0 > hr)
										{
											UninstallInf(i);
										}
									}

									ZwDeleteFile(&oa);
								}

								wcscpy(FileName + _countof("BthScr"), L"inf");

								ZwDeleteFile(&oa);
							}
							RtlFreeUnicodeString(&ObjectName);
						}

						if (0 > hr) hr |= f;
					}
				}
			}

			if (0 > hr)
			{
				ZwDeleteFile(&oa_drv);
			}
		}
	}

	return HRESULT_FROM_WIN32(hr);
}

void UninstallInf()
{
	HANDLE hKey;
	if (0 <= ZwOpenKey(&hKey, KEY_READ, &oa_BthCli))
	{
		KEY_VALUE_PARTIAL_INFORMATION kvpi;
		NTSTATUS status = ZwQueryValueKey(hKey, &INF, KeyValuePartialInformation, &kvpi, sizeof(kvpi), &kvpi.TitleIndex);
		NtClose(hKey);

		if (0 <= status && kvpi.Type == REG_DWORD && kvpi.DataLength == sizeof(DWORD))
		{
			UninstallInf(*(ULONG*)kvpi.Data);
		}
	}
}

BOOL DeleteDriverKey(PCUNICODE_STRING /*ObjectName*/, HANDLE hKey, ULONG Level, PVOID Context)
{
	if (Level == 1)
	{
		PKEY_VALUE_PARTIAL_INFORMATION pkvpi = (PKEY_VALUE_PARTIAL_INFORMATION)alloca(0x100);

		STATIC_UNICODE_STRING_(Driver);
		if (0 <= ZwQueryValueKey(hKey, &Driver, KeyValuePartialInformation, pkvpi, 0x100, &pkvpi->TitleIndex) &&
			pkvpi->Type == REG_SZ && (Level = pkvpi->DataLength) && !(Level & 1) &&
			!*(PWSTR)RtlOffsetToPointer(pkvpi->Data, Level - sizeof(WCHAR)))
		{
			RtlInitUnicodeString(reinterpret_cast<POBJECT_ATTRIBUTES>(Context)->ObjectName, (PWSTR)pkvpi->Data);

			DeleteKey(reinterpret_cast<POBJECT_ATTRIBUTES>(Context));
		}
	}

	return TRUE;
}

void DeleteEnum()
{
	UNICODE_STRING ObjectName;
	OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName, OBJ_CASE_INSENSITIVE };

	if (0 <= ZwOpenKey(&oa.RootDirectory, KEY_READ, &oa_Class))
	{
		DeleteKey(&oa_Enum, 0, DeleteDriverKey, &oa);
		NtClose(oa.RootDirectory);
	}
	else
	{
		DeleteKey(&oa_Enum);
	}
}

void DeleteService()
{
	if (SC_HANDLE hSCManager = OpenSCManager(0, 0, 0))
	{
		if (SC_HANDLE hService = OpenServiceW(hSCManager, L"BthCli", DELETE))
		{
			DeleteService(hService);
			CloseServiceHandle(hService);
		}
		CloseServiceHandle(hSCManager);
	}
}

HRESULT Uninstall()
{
	HRESULT hr = AdjustPrivileges();
	if (hr)
	{
		return HRESULT_FROM_NT(hr);
	}

	UnloadDriver();
	DeleteKey(&oa_LocalSrv);

	DeleteEnum();
	UninstallInf();

	DeleteService();
	ZwDeleteFile(&oa_drv);

	return S_OK;
}

NTSTATUS IsInstalled()
{
	NTSTATUS status;
	HANDLE hKey;

	if (0 <= (status = ZwOpenKey(&hKey, KEY_READ, &oa_BthCli)))
	{
		KEY_VALUE_PARTIAL_INFORMATION kvpi;
		
		status = ZwQueryValueKey(hKey, &INF, KeyValuePartialInformation, &kvpi, sizeof(kvpi), &kvpi.TitleIndex);
		
		NtClose(hKey);

		if (0 <= status)
		{
			return kvpi.Type == REG_DWORD && kvpi.DataLength == sizeof(DWORD) ? STATUS_IMAGE_ALREADY_LOADED :  STATUS_OBJECT_TYPE_MISMATCH;
		}
	}

	return status;
}

_NT_END