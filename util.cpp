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

HRESULT OpenOrCreateKey(_Out_ NCRYPT_KEY_HANDLE *phKey, 
						_In_ PCWSTR pszKeyName, 
						_In_ ULONG dwFlags,
						_Out_opt_ PULONG lpdwDisposition)
{
	NCRYPT_PROV_HANDLE hProvider;

	NTSTATUS status = NCryptOpenStorageProvider(&hProvider, MS_KEY_STORAGE_PROVIDER, 0);

	if (status == NOERROR)
	{
		ULONG dwDisposition = REG_OPENED_EXISTING_KEY;

		status = NCryptOpenKey(hProvider, phKey, pszKeyName, AT_KEYEXCHANGE, dwFlags);

		if (status == NTE_BAD_KEYSET)
		{
			NCRYPT_KEY_HANDLE hKey;

			status = NCryptCreatePersistedKey(hProvider, &hKey, BCRYPT_RSA_ALGORITHM, pszKeyName, 
				AT_KEYEXCHANGE, dwFlags|NCRYPT_OVERWRITE_KEY_FLAG);

			if (status == NOERROR)
			{
				ULONG Length = 2048;

				if ((status = NCryptSetProperty(hKey, NCRYPT_LENGTH_PROPERTY, (PBYTE)&Length, sizeof(Length), 0)) ||
					(status = NCryptSetProperty(hKey, NCRYPT_EXPORT_POLICY_PROPERTY, 
					(PBYTE)&(Length = NCRYPT_ALLOW_EXPORT_FLAG|NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG), sizeof(Length), 0)) ||
					(status = NCryptFinalizeKey(hKey, NCRYPT_SILENT_FLAG)))
				{
					NCryptFreeObject(hKey);
				}
				else
				{
					*phKey = hKey;

					dwDisposition = REG_CREATED_NEW_KEY;
				}
			}
		}

		NCryptFreeObject(hProvider);

		if (lpdwDisposition)
		{
			*lpdwDisposition = dwDisposition;
		}
	}

	return 0 < status ? (status & 0xFFFF) | (FACILITY_SSPI << 16) | 0x80000000 : status;
}

NTSTATUS NKeyToBKey(_Out_ BCRYPT_KEY_HANDLE *phKey, _In_ NCRYPT_KEY_HANDLE hKey)
{
	NTSTATUS status;
	ULONG cb = 0;
	PUCHAR pb = 0;
	while (0 <= (status = NCryptExportKey(hKey, 0, BCRYPT_RSAPRIVATE_BLOB, 0, pb, cb, &cb, 0)))
	{
		if (pb)
		{
			BCRYPT_ALG_HANDLE hAlgorithm;

			if (0 <= (status = BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_RSA_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0)))
			{
				status = BCryptImportKeyPair(hAlgorithm, 0, BCRYPT_RSAPRIVATE_BLOB, phKey, pb, cb, 0);

				BCryptCloseAlgorithmProvider(hAlgorithm, 0);
			}

			break;
		}

		pb = (PUCHAR)alloca(cb);
	}

	return status;
}

HRESULT OpenNKey(_Out_ NCRYPT_KEY_HANDLE *phKey, _In_ PCWSTR pszKeyName, _In_ ULONG dwFlags)
{
	NCRYPT_PROV_HANDLE hProvider;

	NTSTATUS status = NCryptOpenStorageProvider(&hProvider, MS_KEY_STORAGE_PROVIDER, 0);

	if (status == NOERROR)
	{
		status = NCryptOpenKey(hProvider, phKey, pszKeyName, AT_KEYEXCHANGE, dwFlags);

		NCryptFreeObject(hProvider);
	}

	return 0 < status ? (status & 0xFFFF) | (FACILITY_SSPI << 16) | 0x80000000 : status;
}

NTSTATUS OpenBKey(_Out_ BCRYPT_KEY_HANDLE *phKey, _In_ PCWSTR pszKeyName)
{
	NCRYPT_KEY_HANDLE hKey;
	HRESULT hr = OpenNKey(&hKey, pszKeyName, NCRYPT_SILENT_FLAG);
	if (0 <= hr)
	{
		hr = NKeyToBKey(phKey, hKey);
		NCryptFreeObject(hKey);
	}
	return hr;
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

_NT_END