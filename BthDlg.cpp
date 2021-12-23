#include "stdafx.h"
#include "resource.h"

_NT_BEGIN
#include "../winZ/window.h"
#include "../winZ/wic.h"
#include "CM.h"
#include "util.h"
#include "BthDlg.h"

extern volatile const UCHAR guz;

void BthDlg::ClosePorts()
{
	PLIST_ENTRY head = this, entry = head->Blink;

	while (entry != head)
	{
		BTH_RADIO* port = static_cast<BTH_RADIO*>(entry);

		entry = entry->Blink;

		OnPortRemoved(port);

		delete port;
	}

	InitializeListHead(this);
}

BTH_RADIO* BthDlg::getByRadioId(ULONG id)
{
	PLIST_ENTRY head = this, entry = head;

	while ((entry = entry->Blink) != head)
	{
		BTH_RADIO* port = static_cast<BTH_RADIO*>(entry);

		if (port->_id == id)
		{
			return port;
		}
	}

	return 0;
}

BTH_RADIO* BthDlg::GetSelectedPort()
{
	ULONG i = _dwSelectedItem;

	if (i < _dwItemCount)
	{
		PLIST_ENTRY head = this, entry = head;
		do 
		{
			entry = entry->Flink;
		} while (i--);

		return static_cast<BTH_RADIO*>(entry);
	}

	return 0;
}

HRESULT BthDlg::OnInitDialog(HWND hwndDlg)
{
	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = { 
		sizeof(DEV_BROADCAST_DEVICEINTERFACE), DBT_DEVTYP_DEVICEINTERFACE, 0, GUID_BTHPORT_DEVICE_INTERFACE
	};

	if (HDEVNOTIFY HandleV = RegisterDeviceNotification(hwndDlg, 
		&NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE))
	{
		_HandleV = HandleV;
	}
	else
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	SetIcons(hwndDlg);

	EnumBTH(hwndDlg, &GUID_BTHPORT_DEVICE_INTERFACE);

	return S_OK;
}

void BthDlg::OnDestroy()
{
	union {
		HICON hi;
		HDEVNOTIFY HandleV;
	};

	if (HandleV = _HandleV)
	{
		UnregisterDeviceNotification(HandleV);
	}

	if (hi = _hi[1])
	{
		DestroyIcon(hi);
	}

	if (hi = _hi[0])
	{
		DestroyIcon(hi);
	}

	ClosePorts();
}

void BthDlg::SetIcons(_In_ HWND hwndDlg)
{
	LIC c { 0, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON) };

	if (0 <= c.LoadIconWithPNG(MAKEINTRESOURCEW(IDI_BTH)))
	{
		_hi[0] = c._hi;
		SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)c._hi);
	}

	c._hi = 0,c._cx = GetSystemMetrics(SM_CXICON), c._cy = GetSystemMetrics(SM_CYICON);

	if (0 <= c.LoadIconWithPNG(MAKEINTRESOURCEW(IDI_BTH)))
	{
		_hi[1] = c._hi;
		SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)c._hi);
	}
}

void BthDlg::Remove(_In_ HWND hwndDlg, _In_ PCWSTR pszDeviceInterface)
{
	OnBthRemoval(pszDeviceInterface);

	if (_dwItemCount)
	{
		ULONG hash;
		UNICODE_STRING us;
		RtlInitUnicodeString(&us, pszDeviceInterface);
		RtlHashUnicodeString(&us, FALSE, HASH_STRING_ALGORITHM_DEFAULT, &hash);

		LONG i = 0;
		PLIST_ENTRY head = this, entry = head;
		do 
		{
			entry = entry->Flink;

			if (hash == static_cast<BTH_RADIO*>(entry)->_hash)
			{
				RemoveEntryList(entry);

				BTH_RADIO* port = static_cast<BTH_RADIO*>(entry);

				OnPortRemoved(port);

				delete port;

				HWND hwndCB = GetDlgItem(hwndDlg, GetComboId());

				ComboBox_DeleteString(hwndCB, i);

				if (i <= _dwSelectedItem)
				{
					if (0 < _dwSelectedItem--)
					{
						ComboBox_SetCurSel(hwndCB, _dwSelectedItem);
					}
				}

				if (!--_dwItemCount)
				{
					EnableWindow(hwndCB, FALSE);
					OnAnyBthExist(hwndDlg, FALSE);
				}

				return;
			}

			i++;

		} while (entry != head);
	}
}

void BthDlg::Add(_In_ HWND hwndDlg, _In_ PCWSTR pszDeviceInterface)
{
	OnBthArrival(pszDeviceInterface);

	ULONG hash;
	UNICODE_STRING String;
	RtlInitUnicodeString(&String, pszDeviceInterface);
	RtlHashUnicodeString(&String, FALSE, HASH_STRING_ALGORITHM_DEFAULT, &hash);

	static LONG s_i = 0;

	if (BTH_RADIO* port = new BTH_RADIO(s_i++, hash))
	{
		if (0 <= port->Init(pszDeviceInterface))
		{
			PWSTR pszName;

			if (CR_SUCCESS == GetFriendlyName(&pszName, pszDeviceInterface)) 
			{
				OnNewRadio(port, pszName);

				HWND hwndCB = GetDlgItem(hwndDlg, GetComboId());

				int i = ComboBox_AddString(hwndCB, pszName);

				LocalFree(pszName);

				if (0 <= i)
				{
					if (!(_dwSelectedItem = _dwItemCount++))
					{
						EnableWindow(hwndCB, TRUE);
						OnAnyBthExist(hwndDlg, TRUE);
					}
					ComboBox_SetCurSel(hwndCB, _dwSelectedItem);
					InsertTailList(this, port);
					return ;
				}
			}
		}

		delete port;
	}
}

void BthDlg::OnInterfaceChange(_In_ HWND hwndDlg, _In_ WPARAM wParam, _In_ PDEV_BROADCAST_DEVICEINTERFACE p)
{
	switch (wParam)
	{
	case DBT_DEVICEREMOVECOMPLETE:
	case DBT_DEVICEARRIVAL:
		break;
	default: return;
	}

	//DbgPrint("%x %S\n", wParam, p->dbcc_name);

	if (p->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE && p->dbcc_classguid == GUID_BTHPORT_DEVICE_INTERFACE)
	{
		switch (wParam)
		{
		case DBT_DEVICEREMOVECOMPLETE:
			Remove(hwndDlg, p->dbcc_name);
			break;
		case DBT_DEVICEARRIVAL:
			Add(hwndDlg, p->dbcc_name);
			break;
		}
	}
}

void BthDlg::EnumBTH(HWND hwndDlg, _In_ LPCGUID Guid)
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
		cr = CM_Get_Device_Interface_List_SizeW(&rcb, const_cast<GUID*>(Guid), 0, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

		if (cr != CR_SUCCESS || rcb < 2)
		{
			return;
		}

		if (cb < (rcb *= sizeof(WCHAR)))
		{
			cb = RtlPointerToOffset(buf = alloca(rcb - cb), stack);
		}

		cr = CM_Get_Device_Interface_ListW(const_cast<GUID*>(Guid), 0, Buffer, cb, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

	} while (cr == CR_BUFFER_SMALL);

	if (cr == CR_SUCCESS)
	{
		while (*Buffer)
		{
			Add(hwndDlg, Buffer);
			Buffer += wcslen(Buffer) + 1;
		}
	}
}

_NT_END