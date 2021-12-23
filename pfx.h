#pragma once

class PFX_CONTEXT : public GUID 
{
	HANDLE _hDirectory = 0;
	PWSTR _FileName = 0;
	CDataPacket* _packet = 0;
	LONG _dwRef = 1;

	~PFX_CONTEXT();

	void Cleanup();

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

	HRESULT OpenFolder();

	BOOL Init(HWND hwndEdit);

	NTSTATUS InitUserUuid(_In_ HWND hwndDlg, _In_ UINT nIDDlgItem);
};