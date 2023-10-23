#include "stdafx.h"

_NT_BEGIN

#include "../asio/packet.h"
#include "pfx.h"
#include "util.h"
#include "resource.h"

#ifndef MD5_HASH_SIZE
#define MD5_HASH_SIZE 16
#endif

HRESULT BuildPkcs10(_In_ PCWSTR pwszName, _Out_ CDataPacket** request, _Out_ BCRYPT_KEY_HANDLE* phKey);

void PFX_CONTEXT::Cleanup()
{
	if (_FileName)
	{
		delete [] _FileName;
		_FileName = 0;
	}

	if (_packet)
	{
		_packet->Release();
		_packet = 0;
	}

	if (_hKey)
	{
		BCryptDestroyKey(_hKey);
		_hKey = 0;
	}
}

PFX_CONTEXT::~PFX_CONTEXT()
{
	Cleanup();

	if (_hDirectory)
	{
		NtClose(_hDirectory);
	}
}

HRESULT PFX_CONTEXT::OpenFolder()
{
	HRESULT hr;

	UNICODE_STRING ObjectName {};
	ULONG Length = 0;
	while (Length = ExpandEnvironmentStringsW(L"\\??\\%APPDATA%\\ScCerts", ObjectName.Buffer, Length))
	{
		if (ObjectName.Buffer)
		{
			ObjectName.MaximumLength = (USHORT)(Length *= sizeof(WCHAR));
			ObjectName.Length = (USHORT)Length - sizeof(WCHAR);

			IO_STATUS_BLOCK iosb;
			OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName };

			if (0 > (hr = NtCreateFile(&_hDirectory, FILE_ADD_FILE|SYNCHRONIZE, &oa, &iosb, 0, 
				FILE_ATTRIBUTE_DIRECTORY, FILE_SHARE_VALID_FLAGS, FILE_OPEN_IF, 
				FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT, 0, 0)))
			{
				return HRESULT_FROM_NT(hr);
			}

			return S_OK;
		}

		if (Length > MAXSHORT)
		{
			return HRESULT_FROM_NT(STATUS_NAME_TOO_LONG);
		}

		ObjectName.Buffer = (PWSTR)alloca(Length * sizeof(WCHAR));
	}

	return HRESULT_FROM_WIN32(GetLastError());
}

BOOL PFX_CONTEXT::Init(HWND hwndDlg)
{
	Cleanup();

	HWND hwndEdit;
	CHAR szpin[64], szpin2[64];
	ULONG len = GetDlgItemTextA(hwndDlg, IDC_EDIT3, szpin, _countof(szpin));

	if (_countof(szpin) <= GetWindowTextLengthW(GetDlgItem(hwndDlg, IDC_EDIT3)))
	{
		ShowErrorBox(hwndDlg, HRESULT_FROM_NT(STATUS_NAME_TOO_LONG), L"Bad PIN", MB_ICONHAND);
		return FALSE;
	}

	if (GetDlgItemTextA(hwndDlg, IDC_EDIT3, szpin, _countof(szpin)) !=
		GetDlgItemTextA(hwndDlg, IDC_EDIT4, szpin2, _countof(szpin2)) ||
		strcmp(szpin, szpin2))
	{
		ShowErrorBox(hwndDlg, HRESULT_FROM_NT(STATUS_OBJECT_NAME_INVALID), L"PIN mismatch", MB_ICONHAND);
		return FALSE;
	}

	if (0 > DoHash((PUCHAR)szpin, len, _sha256_pin, sizeof(_sha256_pin), BCRYPT_SHA256_ALGORITHM))
	{
		return FALSE;
	}

	len = GetWindowTextLengthW(hwndEdit = GetDlgItem(hwndDlg, IDC_EDIT1));

	WCHAR wszName[64];

	if (len - 1 > _countof(wszName) - 2 || !(len = GetWindowTextW(hwndEdit, wszName, _countof(wszName) )))
	{
		ShowErrorBox(hwndEdit, len ?
			HRESULT_FROM_NT(STATUS_NAME_TOO_LONG) : HRESULT_FROM_NT(STATUS_OBJECT_NAME_INVALID), 
			L"Invalid Cerificate name", MB_ICONHAND);
		
		return FALSE;
	}

	BOOL fOk = FALSE;

	NTSTATUS status;

	if (0 <= (status = BuildPkcs10(wszName, &_packet, &_hKey)))
	{
		fOk = FALSE;
		PSTR utf8 = 0;
		ULONG cb = 0;
		++len;
		while (cb = WideCharToMultiByte(CP_UTF8, 0, wszName, len, utf8, cb, 0, 0))
		{
			if (utf8)
			{
				PWSTR filename = 0;
				ULONG cch = 0;
				while (CryptBinaryToStringW((PUCHAR)utf8, cb, CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF, filename, &cch))
				{
					if (filename)
					{
						FixBase64(filename, cch);

						UNICODE_STRING ObjectName;
						OBJECT_ATTRIBUTES oa = { sizeof(oa), _hDirectory, &ObjectName };
						RtlInitUnicodeString(&ObjectName, filename);

						FILE_BASIC_INFORMATION fbi;

						switch (status = ZwQueryAttributesFile(&oa, &fbi))
						{
						case STATUS_SUCCESS:
							if (ShowErrorBox(hwndEdit, HRESULT_FROM_NT(STATUS_OBJECT_NAME_EXISTS), 
								L"Ovewrite existing cert ?", MB_YESNO|MB_DEFBUTTON2) != IDYES)
							{
								status = STATUS_OBJECT_NAME_COLLISION;
								break;
							}
							[[fallthrough]];
						case STATUS_OBJECT_NAME_NOT_FOUND:
							fOk = TRUE;
							_FileName = filename, filename = 0;
							break;
						default:
							ShowErrorBox(hwndEdit, HRESULT_FROM_NT(status), L"fail create cert file", MB_ICONHAND);
						}

						break;
					}

					if (!(filename = new WCHAR [cch]))
					{
						break;
					}
				}

				if (filename)
				{
					delete [] filename;
				}

				break;
			}

			utf8 = (PSTR)alloca(cb);
		}
	}

	return fOk;
}

NTSTATUS InitUserUuid(_Out_ PGUID guid, _In_ HWND hwndDlg, _In_ UINT nIDDlgItem)
{
	WCHAR sz[33];
	if (_countof(sz) - 1 != GetDlgItemTextW(hwndDlg, nIDDlgItem, sz, _countof(sz)))
	{
		return STATUS_OBJECT_NAME_INVALID;
	}

	ULONG cb = guid ? sizeof(GUID) : 0;

	return CryptStringToBinaryW(sz, _countof(sz) - 1, CRYPT_STRING_HEXRAW, (PUCHAR)guid, &cb, 0, 0) &&
		cb == sizeof(GUID) ? STATUS_SUCCESS : STATUS_OBJECT_NAME_INVALID;
}

_NT_END