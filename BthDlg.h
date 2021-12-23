#pragma once

#include "radio.h"
class __declspec(novtable) BthDlg : public ZDlg, LIST_ENTRY
{
	HICON _hi[2] = {};
	HDEVNOTIFY _HandleV = 0;
	ULONG _dwItemCount = 0;
	LONG _dwSelectedItem = -1;

	void SetIcons(_In_ HWND hwndDlg);

	void ClosePorts();

	void Remove(_In_ HWND hwndDlg, _In_ PCWSTR pszDeviceInterface);

	void Add(_In_ HWND hwndDlg, _In_ PCWSTR pszDeviceInterface);

	void EnumBTH(HWND hwndDlg, _In_ LPCGUID Guid);

protected:
	BthDlg()
	{
		InitializeListHead(this);
	}

	virtual void OnPortRemoved(_In_ BTH_RADIO* port) = 0;
	virtual void OnAnyBthExist(_In_ HWND hwndDlg, _In_ BOOL b) = 0;
	virtual ULONG GetComboId() = 0;

	virtual void OnBthArrival(_In_ PCWSTR pszDeviceInterface)
	{
		DbgPrint("++ i:%S\r\n", pszDeviceInterface);
	}

	virtual void OnBthRemoval(_In_ PCWSTR pszDeviceInterface)
	{
		DbgPrint("-- %S\r\n", pszDeviceInterface);
	}

	virtual void OnNewRadio(_In_ BTH_RADIO* /*port*/, _In_ PCWSTR /*pszName*/)
	{
	}

	void OnInterfaceChange(_In_ HWND hwndDlg, _In_ WPARAM wParam, _In_ PDEV_BROADCAST_DEVICEINTERFACE p);

	void SetSelected(_In_ LONG dwSelectedItem)
	{
		_dwSelectedItem = dwSelectedItem;
	}

	ULONG GetBthCount()
	{
		return _dwItemCount;
	}

	BTH_RADIO* getByRadioId(_In_ ULONG id);

	BTH_RADIO* GetSelectedPort();

	HRESULT OnInitDialog(_In_ HWND hwndDlg);

	void OnDestroy();
};