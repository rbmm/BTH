#pragma once

struct BTH_REQUEST : IO_STATUS_BLOCK 
{
	ULONGLONG hConnection;
	BTH_ADDR btAddr;
	HWND hwnd;
	ULONG code;
	ULONG bthId;
	ULONG time;
	ULONG Data[];

	enum { c_inq = 'GIAC', c_cnt = 'TCNC', c_srh = 'HCRS', c_dsc = 'TCSD', c_atr = 'RTTA'};

	BTH_REQUEST(HWND hwnd, ULONG bthId, ULONG code) : hwnd(hwnd), bthId(bthId), code(code), time(GetTickCount()) {}

	void OnIoEnd()
	{
		if (HWND hWnd = hwnd)
		{
			if (PostMessageW(hWnd, WM_BTH, code, (LPARAM)this))
			{
				return ;
			}
		}
		delete this;
	}

	void* operator new(size_t s, ULONG OutSize = 0)
	{
		return LocalAlloc(0, s + OutSize);
	}

	void operator delete(void* pv)
	{
		LocalFree(pv);
	}

	static NTSTATUS Bind(HANDLE hDevice)
	{
		return RtlSetIoCompletionCallback(hDevice, OnComplete, 0);
	}

	static VOID NTAPI OnComplete(
		_In_ NTSTATUS /*status*/,
		_In_ ULONG_PTR /*Information*/,
		_In_ PVOID Context
		)
	{
		reinterpret_cast<BTH_REQUEST*>(Context)->OnIoEnd();
	}

	void CheckStatus(NTSTATUS status)
	{
		if (NT_ERROR(status))
		{
			Status = status;
			Information = 0;
			OnComplete(status, 0, this);
		}
	}

	ULONG GetIoTime()
	{
		return GetTickCount() - time;
	}
};

