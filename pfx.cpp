#include "stdafx.h"

_NT_BEGIN

#include "../asio/packet.h"
#include "pfx.h"
#include "util.h"

#ifndef MD5_HASH_SIZE
#define MD5_HASH_SIZE 16
#endif

struct X_KEY_AND_NAME 
{
	UCHAR md5[MD5_HASH_SIZE];
	USHORT keyLen, nameLen;
	union {
		BCRYPT_RSAKEY_BLOB rkb;
		UCHAR buf[]; 
	};

	ULONG Size();

	PCWSTR GetName()
	{
		return(PWSTR)(buf + ((keyLen + 1) & ~1));
	}
};

PFX_CONTEXT::~PFX_CONTEXT()
{
	union {
		HANDLE hDirectory;
		PWSTR FileName;
		CDataPacket* packet;
	};

	if (FileName = _FileName)
	{
		delete [] FileName;
	}

	if (packet = _packet)
	{
		packet->Release();
	}

	if (hDirectory = _hDirectory)
	{
		NtClose(hDirectory);
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

void PFX_CONTEXT::Cleanup()
{
	if (PWSTR FileName = _FileName)
	{
		delete [] FileName;
		_FileName = 0;
	}

	if (CDataPacket* packet = _packet)
	{
		packet->Release();
		_packet = 0;
	}
}

BOOL PFX_CONTEXT::Init(HWND hwndEdit)
{
	Cleanup();

	ULONG len = GetWindowTextLengthW(hwndEdit);

	if (len - 1 > 62) 
	{
		ShowErrorBox(hwndEdit, len ?
			HRESULT_FROM_NT(STATUS_NAME_TOO_LONG) : HRESULT_FROM_NT(STATUS_OBJECT_NAME_INVALID), 
			L"Invalid Cerificate name", MB_ICONHAND);
		return FALSE;
	}

	BOOL fOk = FALSE;
	CDataPacket* packet = 0;
	PWSTR pszName = 0;

	NTSTATUS status;
	NCRYPT_KEY_HANDLE hKey;
	ULONG d;

	if (0 <= (status = OpenOrCreateKey(&hKey, L"DFA1ECDB242447beBCA9FE60E043A304", NCRYPT_SILENT_FLAG, &d)))
	{
		X_KEY_AND_NAME* p = 0;
		PBYTE pbPubKey = 0;
		ULONG cb = 0;

		while (NOERROR == (status = NCryptExportKey(hKey, 0, BCRYPT_RSAPUBLIC_BLOB, 0, pbPubKey, cb, &cb, 0)))
		{
			if (pbPubKey)
			{
				p->keyLen = (USHORT)cb;
				p->nameLen = (USHORT)len - 1;
				if (GetWindowTextW(hwndEdit, pszName, len ))
				{
					RtlZeroMemory(p->md5, sizeof(p->md5));
					if (0 <= h_MD5(p, d, p->md5))
					{
						packet->setDataSize(d);
						fOk = TRUE;
					}
				}

				break;
			}

			if (cb > 0x4000)
			{
				break;
			}

			d = FIELD_OFFSET(X_KEY_AND_NAME, buf) + ((cb + 1) & ~1) + ++len * sizeof(WCHAR);

			if (packet = new(d) CDataPacket)
			{
				p = (X_KEY_AND_NAME*)packet->getData();
				pbPubKey = p->buf;
				pszName = (PWSTR)(p->buf + ((cb + 1) & ~1));
			}
			else
			{
				break;
			}
		}

		NCryptFreeObject(hKey);
	}

	if (fOk)
	{
		fOk = FALSE;
		PSTR utf8 = 0;
		ULONG cb = 0;
		while (cb = WideCharToMultiByte(CP_UTF8, 0, pszName, MAXULONG, utf8, cb, 0, 0))
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
							_packet = packet, packet = 0;
							_FileName= filename, filename = 0;
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

	if (packet)
	{
		packet->Release();
	}

	return fOk;
}

NTSTATUS PFX_CONTEXT::InitUserUuid(_In_ HWND hwndDlg, _In_ UINT nIDDlgItem)
{
	BOOL b;
	UINT u = GetDlgItemInt(hwndDlg, nIDDlgItem, &b, FALSE);

	return b ? h_MD5(&u, sizeof(u), (PUCHAR)static_cast<PGUID>(this)) : STATUS_OBJECT_NAME_INVALID;
}

_NT_END