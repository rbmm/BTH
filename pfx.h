#pragma once

NTSTATUS InitUserUuid(_Out_ PGUID guid, _In_ HWND hwndDlg, _In_ UINT nIDDlgItem);

class PFX_CONTEXT : public GUID 
{
	UCHAR _sha256_pin[0x20];
	BCRYPT_KEY_HANDLE _hKey = 0;
	HANDLE _hDirectory = 0;
	PWSTR _FileName = 0;
	CDataPacket* _packet = 0;
	LONG _dwRef = 1;

	void Cleanup();

protected:
	~PFX_CONTEXT();

public:

	void AddRef()
	{
		InterlockedIncrement(&_dwRef);
	}

	void Release()
	{
		if (!InterlockedDecrement(&_dwRef))
		{
			delete this;
		}
	}

	CDataPacket* getPacket()
	{
		return _packet;
	}

	PCWSTR getFileName()
	{
		return _FileName;
	}

	HANDLE getFolder()
	{
		return _hDirectory;
	}

	PBYTE GetHash()
	{
		return _sha256_pin;
	}

	BCRYPT_KEY_HANDLE GetKey()
	{
		return _hKey;
	}

	HRESULT OpenFolder();

	BOOL Init(HWND hwndDlg);

};